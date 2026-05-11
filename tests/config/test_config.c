#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <windows.h>
#include "config.h"
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
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
