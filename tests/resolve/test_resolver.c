#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>
#include <string.h>
#include "resolver.h"
#include "types.h"

/* Helper: allocate a zeroed JumpConfig on the heap (~650KB) */
static JumpConfig *make_config(void) {
    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    return cfg;
}

/* Helper: add a shortcut to cfg */
static Shortcut *add_shortcut(JumpConfig *cfg, ShortcutType type,
                               const char *label, const char *target,
                               int alias_count, ...) {
    assert_true(cfg->shortcut_count < MAX_SHORTCUTS);
    Shortcut *sc = &cfg->shortcuts[cfg->shortcut_count++];
    sc->type = type;
    strncpy(sc->label, label, MAX_LABEL_LEN - 1);
    strncpy(sc->target, target, MAX_PATH_LEN - 1);
    va_list ap;
    va_start(ap, alias_count);
    sc->alias_count = alias_count;
    for (int i = 0; i < alias_count; i++) {
        const char *a = va_arg(ap, const char *);
        strncpy(sc->aliases[i], a, MAX_ALIAS_LEN - 1);
    }
    va_end(ap);
    return sc;
}

/* 1. Alias not found → returns J_EXIT_NOT_FOUND */
static void test_alias_not_found(void **state) {
    (void)state;
    JumpConfig *cfg = make_config();
    add_shortcut(cfg, SHORTCUT_CD, "Projects", "C:\\Projects", 1, "proj");

    ResolveResult result;
    int rc = resolve_alias(cfg, "nonexistent", 0, NULL, &result);
    assert_int_equal(J_EXIT_NOT_FOUND, rc);
    free(cfg);
}

/* 2. Exact match (case-insensitive) for CD type */
static void test_cd_match_case_insensitive(void **state) {
    (void)state;
    JumpConfig *cfg = make_config();
    add_shortcut(cfg, SHORTCUT_CD, "Projects", "C:\\Projects", 2, "proj", "projects");

    ResolveResult result;
    int rc = resolve_alias(cfg, "PROJ", 0, NULL, &result);
    assert_int_equal(0, rc);
    assert_int_equal(SHORTCUT_CD, result.type);
    assert_string_equal("Projects", result.label);
    assert_string_equal("C:\\Projects", result.expanded_target);
    free(cfg);
}

/* 3. OPEN type → URL in expanded_target */
static void test_open_type_url(void **state) {
    (void)state;
    JumpConfig *cfg = make_config();
    add_shortcut(cfg, SHORTCUT_OPEN, "Google", "https://www.google.com", 1, "goog");

    ResolveResult result;
    int rc = resolve_alias(cfg, "goog", 0, NULL, &result);
    assert_int_equal(0, rc);
    assert_int_equal(SHORTCUT_OPEN, result.type);
    assert_string_equal("Google", result.label);
    assert_string_equal("https://www.google.com", result.expanded_target);
    free(cfg);
}

/* 4. EXEC type with {1}, {2} params substituted */
static void test_exec_param_substitution(void **state) {
    (void)state;
    JumpConfig *cfg = make_config();
    add_shortcut(cfg, SHORTCUT_EXEC, "Notepad", "notepad.exe {1} {2}", 1, "np");

    const char *params[] = {"file.txt", "-r"};
    ResolveResult result;
    int rc = resolve_alias(cfg, "np", 2, params, &result);
    assert_int_equal(0, rc);
    assert_int_equal(SHORTCUT_EXEC, result.type);
    assert_string_equal("notepad.exe file.txt -r", result.expanded_target);
    free(cfg);
}

/* 5. EXEC with extra params appended beyond highest placeholder */
static void test_exec_extra_params_appended(void **state) {
    (void)state;
    JumpConfig *cfg = make_config();
    add_shortcut(cfg, SHORTCUT_EXEC, "Notepad", "notepad.exe {1} {2}", 1, "np");

    const char *params[] = {"file.txt", "-r", "--extra"};
    ResolveResult result;
    int rc = resolve_alias(cfg, "np", 3, params, &result);
    assert_int_equal(0, rc);
    assert_string_equal("notepad.exe file.txt -r --extra", result.expanded_target);
    free(cfg);
}

/* 6. EXEC with no placeholders → params appended */
static void test_exec_no_placeholders_params_appended(void **state) {
    (void)state;
    JumpConfig *cfg = make_config();
    add_shortcut(cfg, SHORTCUT_EXEC, "Code", "code.exe", 1, "code");

    const char *params[] = {".", "--new-window"};
    ResolveResult result;
    int rc = resolve_alias(cfg, "code", 2, params, &result);
    assert_int_equal(0, rc);
    assert_string_equal("code.exe . --new-window", result.expanded_target);
    free(cfg);
}

/* 7. First alias vs second alias in same shortcut */
static void test_second_alias_match(void **state) {
    (void)state;
    JumpConfig *cfg = make_config();
    add_shortcut(cfg, SHORTCUT_CD, "Projects", "C:\\Projects", 2, "proj", "projects");

    ResolveResult result;
    int rc = resolve_alias(cfg, "projects", 0, NULL, &result);
    assert_int_equal(0, rc);
    assert_string_equal("Projects", result.label);
    assert_string_equal("C:\\Projects", result.expanded_target);
    free(cfg);
}

/* 8. Empty config → not found */
static void test_empty_config_not_found(void **state) {
    (void)state;
    JumpConfig *cfg = make_config();

    ResolveResult result;
    int rc = resolve_alias(cfg, "anything", 0, NULL, &result);
    assert_int_equal(J_EXIT_NOT_FOUND, rc);
    free(cfg);
}

/* 9. Multi-word alias exact match */
static void test_multi_word_alias_match(void **state) {
    (void)state;
    JumpConfig *cfg = make_config();
    add_shortcut(cfg, SHORTCUT_CD, "My Project", "C:\\MyProject", 2, "my project", "mp");

    ResolveResult result;
    int rc = resolve_alias(cfg, "my project", 0, NULL, &result);
    assert_int_equal(0, rc);
    assert_int_equal(SHORTCUT_CD, result.type);
    assert_string_equal("My Project", result.label);
    assert_string_equal("C:\\MyProject", result.expanded_target);
    free(cfg);
}

/* 10. Multi-word alias case-insensitive */
static void test_multi_word_alias_case_insensitive(void **state) {
    (void)state;
    JumpConfig *cfg = make_config();
    add_shortcut(cfg, SHORTCUT_CD, "My Project", "C:\\MyProject", 1, "my project");

    ResolveResult result;
    int rc = resolve_alias(cfg, "My Project", 0, NULL, &result);
    assert_int_equal(0, rc);
    assert_string_equal("C:\\MyProject", result.expanded_target);
    free(cfg);
}

/* 11. Multi-word alias with params (EXEC) */
static void test_multi_word_alias_exec_with_params(void **state) {
    (void)state;
    JumpConfig *cfg = make_config();
    add_shortcut(cfg, SHORTCUT_EXEC, "Visual Studio", "devenv.exe {1}", 1, "visual studio");

    const char *params[] = {"solution.sln"};
    ResolveResult result;
    int rc = resolve_alias(cfg, "visual studio", 1, params, &result);
    assert_int_equal(0, rc);
    assert_int_equal(SHORTCUT_EXEC, result.type);
    assert_string_equal("devenv.exe solution.sln", result.expanded_target);
    free(cfg);
}

/* 12. Single-word alias still works alongside multi-word shortcuts */
static void test_single_word_alias_with_multi_word_present(void **state) {
    (void)state;
    JumpConfig *cfg = make_config();
    add_shortcut(cfg, SHORTCUT_CD, "My Project", "C:\\MyProject", 1, "my project");
    add_shortcut(cfg, SHORTCUT_CD, "Work", "C:\\Work", 1, "work");

    ResolveResult result;
    int rc = resolve_alias(cfg, "work", 0, NULL, &result);
    assert_int_equal(0, rc);
    assert_string_equal("C:\\Work", result.expanded_target);
    free(cfg);
}

int run_resolver_tests(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_alias_not_found),
        cmocka_unit_test(test_cd_match_case_insensitive),
        cmocka_unit_test(test_open_type_url),
        cmocka_unit_test(test_exec_param_substitution),
        cmocka_unit_test(test_exec_extra_params_appended),
        cmocka_unit_test(test_exec_no_placeholders_params_appended),
        cmocka_unit_test(test_second_alias_match),
        cmocka_unit_test(test_empty_config_not_found),
        cmocka_unit_test(test_multi_word_alias_match),
        cmocka_unit_test(test_multi_word_alias_case_insensitive),
        cmocka_unit_test(test_multi_word_alias_exec_with_params),
        cmocka_unit_test(test_single_word_alias_with_multi_word_present),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
