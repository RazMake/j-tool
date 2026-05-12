#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "jump.h"

/* --- exec_needs_cmd_wrapper tests --- */

static void test_cmd_extension_unquoted(void **state) {
    (void)state;
    assert_int_equal(1, exec_needs_cmd_wrapper("C:\\tools\\build.cmd"));
    assert_int_equal(1, exec_needs_cmd_wrapper("build.cmd arg1 arg2"));
    assert_int_equal(1, exec_needs_cmd_wrapper("C:\\tools\\build.CMD"));
}

static void test_bat_extension_unquoted(void **state) {
    (void)state;
    assert_int_equal(1, exec_needs_cmd_wrapper("C:\\tools\\run.bat"));
    assert_int_equal(1, exec_needs_cmd_wrapper("run.bat --verbose"));
    assert_int_equal(1, exec_needs_cmd_wrapper("run.BAT"));
}

static void test_cmd_extension_quoted(void **state) {
    (void)state;
    assert_int_equal(1, exec_needs_cmd_wrapper("\"C:\\Program Files\\build.cmd\" arg1"));
    assert_int_equal(1, exec_needs_cmd_wrapper("\"C:\\tools\\run.bat\""));
}

static void test_exe_not_wrapped(void **state) {
    (void)state;
    assert_int_equal(0, exec_needs_cmd_wrapper("C:\\tools\\app.exe"));
    assert_int_equal(0, exec_needs_cmd_wrapper("\"C:\\Program Files\\app.exe\" arg1"));
    assert_int_equal(0, exec_needs_cmd_wrapper("notepad.exe"));
}

static void test_no_extension_not_wrapped(void **state) {
    (void)state;
    assert_int_equal(0, exec_needs_cmd_wrapper("app"));
    assert_int_equal(0, exec_needs_cmd_wrapper("cmd"));
}

static void test_short_token_not_wrapped(void **state) {
    (void)state;
    assert_int_equal(0, exec_needs_cmd_wrapper("a.b"));
    assert_int_equal(0, exec_needs_cmd_wrapper(""));
}

int run_jump_tests(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_cmd_extension_unquoted),
        cmocka_unit_test(test_bat_extension_unquoted),
        cmocka_unit_test(test_cmd_extension_quoted),
        cmocka_unit_test(test_exe_not_wrapped),
        cmocka_unit_test(test_no_extension_not_wrapped),
        cmocka_unit_test(test_short_token_not_wrapped),
    };
    return cmocka_run_group_tests_name("jump", tests, NULL, NULL);
}
