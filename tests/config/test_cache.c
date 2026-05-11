#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cache.h"

static void make_temp_path(wchar_t *out, size_t max_chars, const wchar_t *name) {
    GetTempPathW((DWORD)max_chars, out);
    wcsncat_s(out, max_chars, name, _TRUNCATE);
}

static void test_cache_get_default_path(void **state) {
    (void)state;
    wchar_t path[MAX_PATH];
    assert_int_equal(0, cache_get_default_path(path, MAX_PATH));
    assert_non_null(wcsstr(path, L"jump.cache"));
}

static void test_cache_save_load_roundtrip(void **state) {
    (void)state;
    wchar_t tmp[MAX_PATH];
    make_temp_path(tmp, MAX_PATH, L"jtest_roundtrip.cache");

    JumpConfig *cfg_out = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg_out);
    cfg_out->shortcut_count = 1;
    strcpy_s(cfg_out->shortcuts[0].aliases[0], MAX_ALIAS_LEN, "proj");
    cfg_out->shortcuts[0].alias_count = 1;
    strcpy_s(cfg_out->shortcuts[0].target, MAX_PATH_LEN, "C:\\Projects");
    cfg_out->shortcuts[0].type = SHORTCUT_CD;
    cfg_out->constant_count = 1;
    strcpy_s(cfg_out->constants[0].name, MAX_ALIAS_LEN, "HOME");
    strcpy_s(cfg_out->constants[0].value, MAX_PATH_LEN, "C:\\Users\\Test");

    CacheSourceFile src = {0};
    wcscpy_s(src.path, MAX_PATH, L"C:\\fake.ini");

    assert_int_equal(0, cache_save(tmp, cfg_out, &src, 1));

    JumpConfig *cfg_in = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg_in);
    assert_int_equal(0, cache_load(tmp, cfg_in));

    assert_int_equal(cfg_out->shortcut_count, cfg_in->shortcut_count);
    assert_int_equal(cfg_out->constant_count, cfg_in->constant_count);
    assert_string_equal(cfg_out->shortcuts[0].aliases[0],
                        cfg_in->shortcuts[0].aliases[0]);
    assert_string_equal(cfg_out->shortcuts[0].target,
                        cfg_in->shortcuts[0].target);
    assert_int_equal(cfg_out->shortcuts[0].type, cfg_in->shortcuts[0].type);
    assert_string_equal(cfg_out->constants[0].name,
                        cfg_in->constants[0].name);
    assert_string_equal(cfg_out->constants[0].value,
                        cfg_in->constants[0].value);

    free(cfg_out);
    free(cfg_in);
    DeleteFileW(tmp);
}

static void test_cache_is_fresh_nonexistent(void **state) {
    (void)state;
    assert_int_equal(0, cache_is_fresh(L"C:\\nonexistent_jtest.cache"));
}

static void test_cache_is_fresh_after_save(void **state) {
    (void)state;
    wchar_t tmp[MAX_PATH];
    make_temp_path(tmp, MAX_PATH, L"jtest_fresh.cache");

    /* Use a real file whose mtime we can capture */
    wchar_t src_path[MAX_PATH];
    make_temp_path(src_path, MAX_PATH, L"jtest_src.txt");

    /* Create the source file */
    HANDLE hSrc = CreateFileW(src_path, GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    assert_true(hSrc != INVALID_HANDLE_VALUE);
    CloseHandle(hSrc);

    /* Get its mtime */
    WIN32_FILE_ATTRIBUTE_DATA fdata;
    assert_true(GetFileAttributesExW(src_path, GetFileExInfoStandard, &fdata));

    CacheSourceFile sf = {0};
    wcscpy_s(sf.path, MAX_PATH, src_path);
    sf.mtime = fdata.ftLastWriteTime;

    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    assert_int_equal(0, cache_save(tmp, cfg, &sf, 1));
    assert_int_equal(1, cache_is_fresh(tmp));

    free(cfg);
    DeleteFileW(tmp);
    DeleteFileW(src_path);
}

static void test_cache_load_corrupted_magic(void **state) {
    (void)state;
    wchar_t tmp[MAX_PATH];
    make_temp_path(tmp, MAX_PATH, L"jtest_corrupt.cache");

    /* Write garbage with wrong magic */
    HANDLE hFile = CreateFileW(tmp, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    assert_true(hFile != INVALID_HANDLE_VALUE);
    CacheHeader hdr = {0};
    hdr.magic = 0xDEADBEEF;
    hdr.version = CACHE_VERSION;
    DWORD written;
    WriteFile(hFile, &hdr, sizeof(hdr), &written, NULL);
    CloseHandle(hFile);

    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    assert_int_equal(-1, cache_load(tmp, cfg));

    free(cfg);
    DeleteFileW(tmp);
}

static void test_cache_load_nonexistent(void **state) {
    (void)state;
    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    assert_int_equal(-1, cache_load(L"C:\\nonexistent_jtest.cache", cfg));
    free(cfg);
}

int run_cache_tests(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_cache_get_default_path),
        cmocka_unit_test(test_cache_save_load_roundtrip),
        cmocka_unit_test(test_cache_is_fresh_nonexistent),
        cmocka_unit_test(test_cache_is_fresh_after_save),
        cmocka_unit_test(test_cache_load_corrupted_magic),
        cmocka_unit_test(test_cache_load_nonexistent),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
