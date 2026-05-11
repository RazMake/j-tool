#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "tc.h"
#include <string.h>

static void test_tc_build_cd_command_left(void **state) {
    (void)state;
    char buf[512];
    int rc = tc_build_cd_command("C:\\TC\\TOTALCMD64.EXE", "L",
                                 "C:\\Projects", buf, sizeof(buf));
    assert_int_equal(rc, 0);
    assert_non_null(strstr(buf, "/L="));
    assert_non_null(strstr(buf, "C:\\TC\\TOTALCMD64.EXE"));
    assert_non_null(strstr(buf, "C:\\Projects"));
}

static void test_tc_build_cd_command_right(void **state) {
    (void)state;
    char buf[512];
    int rc = tc_build_cd_command("C:\\TC\\TOTALCMD64.EXE", "R",
                                 "D:\\Work", buf, sizeof(buf));
    assert_int_equal(rc, 0);
    assert_non_null(strstr(buf, "/R="));
    assert_non_null(strstr(buf, "D:\\Work"));
    /* Must not contain /L= */
    assert_null(strstr(buf, "/L="));
}

static void test_tc_build_cd_command_spaces(void **state) {
    (void)state;
    char buf[512];
    int rc = tc_build_cd_command("C:\\Program Files\\TC\\TOTALCMD64.EXE", "L",
                                 "C:\\My Documents\\Projects", buf, sizeof(buf));
    assert_int_equal(rc, 0);
    /* TC path and directory must be quoted */
    assert_non_null(strstr(buf, "\"C:\\Program Files\\TC\\TOTALCMD64.EXE\""));
    assert_non_null(strstr(buf, "\"C:\\My Documents\\Projects\""));
}

static void test_tc_build_cd_command_small_buffer(void **state) {
    (void)state;
    char buf[8];
    int rc = tc_build_cd_command("C:\\TC\\TOTALCMD64.EXE", "L",
                                 "C:\\Projects", buf, sizeof(buf));
    assert_int_equal(rc, -1);
}

static void test_tc_find_path_no_crash(void **state) {
    (void)state;
    char buf[512];
    int rc = tc_find_path(buf, sizeof(buf));
    /* TC may or may not be installed; just verify valid return */
    assert_true(rc == 0 || rc == -1);
}

static void test_tc_find_path_small_buffer(void **state) {
    (void)state;
    char buf[5];
    int rc = tc_find_path(buf, sizeof(buf));
    /* If TC is installed, buffer is too small → -1.
       If TC is not installed, also -1. */
    assert_int_equal(-1, rc);
}

int run_tc_tests(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_tc_build_cd_command_left),
        cmocka_unit_test(test_tc_build_cd_command_right),
        cmocka_unit_test(test_tc_build_cd_command_spaces),
        cmocka_unit_test(test_tc_build_cd_command_small_buffer),
        cmocka_unit_test(test_tc_find_path_no_crash),
        cmocka_unit_test(test_tc_find_path_small_buffer),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
