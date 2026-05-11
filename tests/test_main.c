#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "tests.h"

int main(void) {
    int failed = 0;
    failed += run_ini_parser_tests();
    failed += run_cache_tests();
    failed += run_config_tests();
    failed += run_resolver_tests();
    failed += run_suggest_tests();
    failed += run_tc_tests();
    failed += run_install_tests();
    return failed;
}
