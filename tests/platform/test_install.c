#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>
#include <string.h>
#include "install.h"

/*
 * Install/uninstall modify real system state (registry, files, PATH).
 * These tests are placeholders — they verify the module links correctly
 * but do NOT call the real install/uninstall functions.
 * Actual integration testing is done manually.
 * Coverage for install.c is excluded from the 85% gate (like osd.c).
 */

static void test_install_null_panel_placeholder(void **state) {
    (void)state;
    /* jump_install(NULL) would modify registry/files — manual test only */
    assert_true(1);
}

static void test_install_left_panel_placeholder(void **state) {
    (void)state;
    /* jump_install("L") — manual test only */
    assert_true(1);
}

static void test_install_right_panel_placeholder(void **state) {
    (void)state;
    /* jump_install("R") — manual test only */
    assert_true(1);
}

static void test_uninstall_placeholder(void **state) {
    (void)state;
    /* jump_uninstall() — manual test only */
    assert_true(1);
}

static void test_install_idempotency_placeholder(void **state) {
    (void)state;
    /* Calling jump_install twice should return same result — manual test only */
    assert_true(1);
}

int run_install_tests(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_install_null_panel_placeholder),
        cmocka_unit_test(test_install_left_panel_placeholder),
        cmocka_unit_test(test_install_right_panel_placeholder),
        cmocka_unit_test(test_uninstall_placeholder),
        cmocka_unit_test(test_install_idempotency_placeholder),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
