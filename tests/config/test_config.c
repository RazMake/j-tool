#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <windows.h>
#include "config.h"
#include "cache.h"
#include "types.h"

/* ── config_expand tests ──────────────────────────────────────────────── */

static void test_config_expand_passthrough(void **state) {
    (void)state;
    JumpConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    char buf[256];
    assert_int_equal(0, config_expand(&cfg, "hello world", buf, sizeof(buf)));
    assert_string_equal("hello world", buf);
}

static void test_config_expand_constant(void **state) {
    (void)state;
    JumpConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    strcpy_s(cfg.constants[0].name, MAX_ALIAS_LEN, "PROJECTS");
    strcpy_s(cfg.constants[0].value, MAX_PATH_LEN, "C:\\MyProjects");
    cfg.constant_count = 1;

    char buf[256];
    assert_int_equal(0, config_expand(&cfg, "dir={{PROJECTS}}", buf, sizeof(buf)));
    assert_string_equal("dir=C:\\MyProjects", buf);
}

static void test_config_expand_env_var(void **state) {
    (void)state;
    JumpConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    char expected[512];
    char env_val[256];
    DWORD r = GetEnvironmentVariableA("USERPROFILE", env_val, sizeof(env_val));
    if (r == 0) {
        skip();
    }
    _snprintf_s(expected, sizeof(expected), _TRUNCATE, "home=%s", env_val);

    char buf[512];
    assert_int_equal(0, config_expand(&cfg, "home={{ENV:USERPROFILE}}", buf, sizeof(buf)));
    assert_string_equal(expected, buf);
}

static void test_config_expand_unknown_constant(void **state) {
    (void)state;
    JumpConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    char buf[256];
    assert_int_not_equal(0, config_expand(&cfg, "{{NOPE}}", buf, sizeof(buf)));
}

static void test_config_expand_multiple(void **state) {
    (void)state;
    JumpConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    strcpy_s(cfg.constants[0].name, MAX_ALIAS_LEN, "A");
    strcpy_s(cfg.constants[0].value, MAX_PATH_LEN, "alpha");
    strcpy_s(cfg.constants[1].name, MAX_ALIAS_LEN, "B");
    strcpy_s(cfg.constants[1].value, MAX_PATH_LEN, "beta");
    cfg.constant_count = 2;

    char buf[256];
    assert_int_equal(0, config_expand(&cfg, "{{A}}/{{B}}", buf, sizeof(buf)));
    assert_string_equal("alpha/beta", buf);
}

static void test_config_expand_buffer_too_small(void **state) {
    (void)state;
    JumpConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    strcpy_s(cfg.constants[0].name, MAX_ALIAS_LEN, "BIG");
    strcpy_s(cfg.constants[0].value, MAX_PATH_LEN, "a_long_replacement_value");
    cfg.constant_count = 1;

    char buf[5]; /* way too small */
    assert_int_not_equal(0, config_expand(&cfg, "{{BIG}}", buf, sizeof(buf)));
}

/* ── config_validate tests ────────────────────────────────────────────── */

static void test_config_validate_ok(void **state) {
    (void)state;
    JumpConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    strcpy_s(cfg.shortcuts[0].aliases[0], MAX_ALIAS_LEN, "home");
    cfg.shortcuts[0].alias_count = 1;
    strcpy_s(cfg.shortcuts[1].aliases[0], MAX_ALIAS_LEN, "work");
    cfg.shortcuts[1].alias_count = 1;
    cfg.shortcut_count = 2;

    strcpy_s(cfg.constants[0].name, MAX_ALIAS_LEN, "A");
    strcpy_s(cfg.constants[1].name, MAX_ALIAS_LEN, "B");
    cfg.constant_count = 2;

    assert_int_equal(0, config_validate(&cfg));
}

static void test_config_validate_dup_alias(void **state) {
    (void)state;
    JumpConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    strcpy_s(cfg.shortcuts[0].aliases[0], MAX_ALIAS_LEN, "dup");
    strcpy_s(cfg.shortcuts[0].aliases[1], MAX_ALIAS_LEN, "a");
    cfg.shortcuts[0].alias_count = 2;
    strcpy_s(cfg.shortcuts[1].aliases[0], MAX_ALIAS_LEN, "DUP");
    strcpy_s(cfg.shortcuts[1].aliases[1], MAX_ALIAS_LEN, "b");
    cfg.shortcuts[1].alias_count = 2;
    cfg.shortcut_count = 2;

    assert_int_equal(J_EXIT_CONFIG_ERROR, config_validate(&cfg));
}

static void test_config_validate_dup_constant(void **state) {
    (void)state;
    JumpConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    strcpy_s(cfg.constants[0].name, MAX_ALIAS_LEN, "MYVAR");
    strcpy_s(cfg.constants[1].name, MAX_ALIAS_LEN, "myvar");
    cfg.constant_count = 2;

    assert_int_equal(J_EXIT_CONFIG_ERROR, config_validate(&cfg));
}

/* ── config_load tests ────────────────────────────────────────────────── */

static char saved_jumps_env[MAX_PATH];
static int had_jumps_env;

static int config_load_setup(void **state) {
    (void)state;
    DWORD r = GetEnvironmentVariableA("JUMPS", saved_jumps_env, MAX_PATH);
    had_jumps_env = (r > 0);
    /* Delete any existing cache */
    wchar_t cp[MAX_PATH];
    if (cache_get_default_path(cp, MAX_PATH) == 0)
        DeleteFileW(cp);
    return 0;
}

static int config_load_teardown(void **state) {
    (void)state;
    if (had_jumps_env)
        SetEnvironmentVariableA("JUMPS", saved_jumps_env);
    else
        SetEnvironmentVariableA("JUMPS", NULL);
    wchar_t cp[MAX_PATH];
    if (cache_get_default_path(cp, MAX_PATH) == 0)
        DeleteFileW(cp);
    return 0;
}

static void test_config_load_no_env(void **state) {
    (void)state;
    SetEnvironmentVariableA("JUMPS", NULL);
    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    assert_int_equal(J_EXIT_CONFIG_ERROR, config_load(cfg));
    free(cfg);
}

static void test_config_load_bad_file(void **state) {
    (void)state;
    SetEnvironmentVariableA("JUMPS", "Z:\\nonexistent\\fake.ini");
    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    assert_int_equal(J_EXIT_CONFIG_ERROR, config_load(cfg));
    free(cfg);
}

static void test_config_load_malformed(void **state) {
    (void)state;
    SetEnvironmentVariableA("JUMPS", FIXTURES_DIR "/malformed.ini");
    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    assert_int_equal(J_EXIT_CONFIG_ERROR, config_load(cfg));
    free(cfg);
}

static void test_config_load_success(void **state) {
    (void)state;
    SetEnvironmentVariableA("JUMPS", FIXTURES_DIR "/valid_root.ini");
    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    int rc = config_load(cfg);
    assert_int_equal(0, rc);

    /* Constants: PROJECTS, TOOLS (root) + MUSIC (personal) */
    assert_int_equal(3, cfg->constant_count);

    /* Shortcuts: work, company, editor (work.ini) + home, music (personal.ini) */
    assert_int_equal(5, cfg->shortcut_count);

    /* Verify all three shortcut types are present */
    int has_cd = 0, has_open = 0, has_exec = 0;
    for (int i = 0; i < cfg->shortcut_count; i++) {
        if (cfg->shortcuts[i].type == SHORTCUT_CD)   has_cd = 1;
        if (cfg->shortcuts[i].type == SHORTCUT_OPEN) has_open = 1;
        if (cfg->shortcuts[i].type == SHORTCUT_EXEC) has_exec = 1;
    }
    assert_true(has_cd);
    assert_true(has_open);
    assert_true(has_exec);

    free(cfg);
}

static void test_config_load_dup_alias(void **state) {
    (void)state;
    SetEnvironmentVariableA("JUMPS", FIXTURES_DIR "/duplicate_alias.ini");
    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    assert_int_equal(J_EXIT_CONFIG_ERROR, config_load(cfg));
    free(cfg);
}

static void test_config_load_dup_constant(void **state) {
    (void)state;
    SetEnvironmentVariableA("JUMPS", FIXTURES_DIR "/duplicate_constant.ini");
    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    assert_int_equal(J_EXIT_CONFIG_ERROR, config_load(cfg));
    free(cfg);
}

static void test_config_load_utf8_bom(void **state) {
    (void)state;
    wchar_t tmp_path[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp_path);
    wcsncat_s(tmp_path, MAX_PATH, L"jtest_bom.ini", _TRUNCATE);

    HANDLE h = CreateFileW(tmp_path, GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    assert_true(h != INVALID_HANDLE_VALUE);

    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    const char *content =
        "[BomTest]\nLabel=BOM Test\nJumps=bomtest\nPath=C:\\BomTest\n";
    DWORD written;
    WriteFile(h, bom, sizeof(bom), &written, NULL);
    WriteFile(h, content, (DWORD)strlen(content), &written, NULL);
    CloseHandle(h);

    char tmp_a[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, tmp_path, -1, tmp_a, MAX_PATH, NULL, NULL);
    SetEnvironmentVariableA("JUMPS", tmp_a);

    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    assert_int_equal(0, config_load(cfg));
    assert_int_equal(1, cfg->shortcut_count);

    free(cfg);
    DeleteFileW(tmp_path);
}

static void test_config_load_utf16le_bom(void **state) {
    (void)state;
    wchar_t tmp_path[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp_path);
    wcsncat_s(tmp_path, MAX_PATH, L"jtest_utf16.ini", _TRUNCATE);

    HANDLE h = CreateFileW(tmp_path, GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    assert_true(h != INVALID_HANDLE_VALUE);

    const unsigned char bom[] = {0xFF, 0xFE};
    const wchar_t *content =
        L"[Utf16Test]\nLabel=UTF16 Test\nJumps=utf16test\nPath=C:\\Utf16\n";
    DWORD written;
    WriteFile(h, bom, sizeof(bom), &written, NULL);
    WriteFile(h, content, (DWORD)(wcslen(content) * sizeof(wchar_t)),
              &written, NULL);
    CloseHandle(h);

    char tmp_a[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, tmp_path, -1, tmp_a, MAX_PATH, NULL, NULL);
    SetEnvironmentVariableA("JUMPS", tmp_a);

    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    assert_int_equal(0, config_load(cfg));
    assert_int_equal(1, cfg->shortcut_count);

    free(cfg);
    DeleteFileW(tmp_path);
}

static void test_config_load_cache_hit(void **state) {
    (void)state;
    SetEnvironmentVariableA("JUMPS", FIXTURES_DIR "/valid_root.ini");

    /* First load — parses INI, writes cache */
    JumpConfig *cfg1 = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg1);
    assert_int_equal(0, config_load(cfg1));

    /* Second load — should hit cache */
    JumpConfig *cfg2 = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg2);
    assert_int_equal(0, config_load(cfg2));

    assert_int_equal(cfg1->shortcut_count, cfg2->shortcut_count);
    assert_int_equal(cfg1->constant_count, cfg2->constant_count);

    free(cfg1);
    free(cfg2);
}

int run_config_tests(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_config_expand_passthrough),
        cmocka_unit_test(test_config_expand_constant),
        cmocka_unit_test(test_config_expand_env_var),
        cmocka_unit_test(test_config_expand_unknown_constant),
        cmocka_unit_test(test_config_expand_multiple),
        cmocka_unit_test(test_config_expand_buffer_too_small),
        cmocka_unit_test(test_config_validate_ok),
        cmocka_unit_test(test_config_validate_dup_alias),
        cmocka_unit_test(test_config_validate_dup_constant),
        cmocka_unit_test_setup_teardown(test_config_load_no_env,
            config_load_setup, config_load_teardown),
        cmocka_unit_test_setup_teardown(test_config_load_bad_file,
            config_load_setup, config_load_teardown),
        cmocka_unit_test_setup_teardown(test_config_load_malformed,
            config_load_setup, config_load_teardown),
        cmocka_unit_test_setup_teardown(test_config_load_success,
            config_load_setup, config_load_teardown),
        cmocka_unit_test_setup_teardown(test_config_load_dup_alias,
            config_load_setup, config_load_teardown),
        cmocka_unit_test_setup_teardown(test_config_load_dup_constant,
            config_load_setup, config_load_teardown),
        cmocka_unit_test_setup_teardown(test_config_load_utf8_bom,
            config_load_setup, config_load_teardown),
        cmocka_unit_test_setup_teardown(test_config_load_utf16le_bom,
            config_load_setup, config_load_teardown),
        cmocka_unit_test_setup_teardown(test_config_load_cache_hit,
            config_load_setup, config_load_teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
