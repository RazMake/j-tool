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

static void test_cache_save_null_config(void **state) {
    (void)state;
    wchar_t tmp[MAX_PATH];
    make_temp_path(tmp, MAX_PATH, L"jtest_null.cache");
    CacheSourceFile sf = {0};
    assert_int_equal(-1, cache_save(tmp, NULL, &sf, 1));
    DeleteFileW(tmp);
}

static void test_cache_save_bad_source_count(void **state) {
    (void)state;
    wchar_t tmp[MAX_PATH];
    make_temp_path(tmp, MAX_PATH, L"jtest_badsc.cache");
    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    CacheSourceFile sf = {0};
    assert_int_equal(-1, cache_save(tmp, cfg, &sf, -1));
    assert_int_equal(-1, cache_save(tmp, cfg, &sf, MAX_SOURCE_FILES + 1));
    free(cfg);
    DeleteFileW(tmp);
}

static void test_cache_save_invalid_path(void **state) {
    (void)state;
    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    CacheSourceFile sf = {0};
    assert_int_equal(-1, cache_save(L"Z:\\no\\such\\dir\\file.cache", cfg, &sf, 0));
    free(cfg);
}

static void test_cache_load_bad_counts(void **state) {
    (void)state;
    wchar_t tmp[MAX_PATH];
    make_temp_path(tmp, MAX_PATH, L"jtest_badcounts.cache");

    HANDLE hFile = CreateFileW(tmp, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    assert_true(hFile != INVALID_HANDLE_VALUE);
    CacheHeader hdr = {0};
    hdr.magic = CACHE_MAGIC;
    hdr.version = CACHE_VERSION;
    hdr.shortcut_count = MAX_SHORTCUTS + 1;
    DWORD written;
    WriteFile(hFile, &hdr, sizeof(hdr), &written, NULL);
    CloseHandle(hFile);

    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    assert_int_equal(-1, cache_load(tmp, cfg));
    free(cfg);
    DeleteFileW(tmp);
}

static void test_cache_is_fresh_bad_magic(void **state) {
    (void)state;
    wchar_t tmp[MAX_PATH];
    make_temp_path(tmp, MAX_PATH, L"jtest_badmagic_f.cache");

    HANDLE hFile = CreateFileW(tmp, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    assert_true(hFile != INVALID_HANDLE_VALUE);
    CacheHeader hdr = {0};
    hdr.magic = 0xDEADBEEF;
    hdr.version = CACHE_VERSION;
    DWORD written;
    WriteFile(hFile, &hdr, sizeof(hdr), &written, NULL);
    CloseHandle(hFile);

    assert_int_equal(0, cache_is_fresh(tmp));
    DeleteFileW(tmp);
}

static void test_cache_is_fresh_truncated(void **state) {
    (void)state;
    wchar_t tmp[MAX_PATH];
    char buf[] = "tiny";
    DWORD written;
    HANDLE hFile;
    make_temp_path(tmp, MAX_PATH, L"jtest_trunc_f.cache");

    hFile = CreateFileW(tmp, GENERIC_WRITE, 0, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    assert_true(hFile != INVALID_HANDLE_VALUE);
    WriteFile(hFile, buf, sizeof(buf), &written, NULL);
    CloseHandle(hFile);

    assert_int_equal(0, cache_is_fresh(tmp));
    DeleteFileW(tmp);
}

static void test_cache_is_fresh_missing_source(void **state) {
    (void)state;
    wchar_t tmp[MAX_PATH];
    make_temp_path(tmp, MAX_PATH, L"jtest_missrc_f.cache");

    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    CacheSourceFile sf = {0};
    wcscpy_s(sf.path, MAX_PATH, L"C:\\nonexistent_jtest_src_99.txt");

    assert_int_equal(0, cache_save(tmp, cfg, &sf, 1));
    assert_int_equal(0, cache_is_fresh(tmp));

    free(cfg);
    DeleteFileW(tmp);
}

static void test_cache_load_null_config(void **state) {
    (void)state;
    assert_int_equal(-1, cache_load(L"C:\\some.cache", NULL));
}

static void test_cache_load_truncated(void **state) {
    (void)state;
    wchar_t tmp[MAX_PATH];
    char buf[] = "too short";
    DWORD written;
    HANDLE hFile;
    make_temp_path(tmp, MAX_PATH, L"jtest_trunc_load.cache");

    hFile = CreateFileW(tmp, GENERIC_WRITE, 0, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    assert_true(hFile != INVALID_HANDLE_VALUE);
    WriteFile(hFile, buf, sizeof(buf), &written, NULL);
    CloseHandle(hFile);

    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    assert_int_equal(-1, cache_load(tmp, cfg));
    free(cfg);
    DeleteFileW(tmp);
}

static void test_cache_load_truncated_shortcuts(void **state) {
    (void)state;
    wchar_t tmp[MAX_PATH];
    make_temp_path(tmp, MAX_PATH, L"jtest_trunc_sc.cache");

    /* Write a valid header claiming 1 shortcut, but no shortcut data */
    HANDLE hFile = CreateFileW(tmp, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    assert_true(hFile != INVALID_HANDLE_VALUE);
    CacheHeader hdr = {0};
    hdr.magic = CACHE_MAGIC;
    hdr.version = CACHE_VERSION;
    hdr.shortcut_count = 1;
    hdr.constant_count = 0;
    DWORD written;
    WriteFile(hFile, &hdr, sizeof(hdr), &written, NULL);
    CloseHandle(hFile);

    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    assert_int_equal(-1, cache_load(tmp, cfg));
    free(cfg);
    DeleteFileW(tmp);
}

static void test_cache_load_truncated_constants(void **state) {
    (void)state;
    wchar_t tmp[MAX_PATH];
    make_temp_path(tmp, MAX_PATH, L"jtest_trunc_ct.cache");

    /* Write valid header + full shortcuts, but truncated constants */
    JumpConfig *cfg_out = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg_out);
    cfg_out->shortcut_count = 1;
    strcpy_s(cfg_out->shortcuts[0].aliases[0], MAX_ALIAS_LEN, "x");
    cfg_out->shortcuts[0].alias_count = 1;
    cfg_out->shortcuts[0].type = SHORTCUT_CD;

    /* First, save a valid cache with 1 shortcut + 0 constants */
    CacheSourceFile sf = {0};
    assert_int_equal(0, cache_save(tmp, cfg_out, &sf, 0));

    /* Now re-write just the header with constant_count=1 (but no constant data) */
    HANDLE hFile = CreateFileW(tmp, GENERIC_WRITE, 0, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    assert_true(hFile != INVALID_HANDLE_VALUE);

    CacheHeader hdr = {0};
    hdr.magic = CACHE_MAGIC;
    hdr.version = CACHE_VERSION;
    hdr.shortcut_count = 1;
    hdr.constant_count = 1; /* claims 1 constant but data won't be there */
    DWORD written;
    WriteFile(hFile, &hdr, sizeof(hdr), &written, NULL);
    CloseHandle(hFile);

    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    assert_int_equal(-1, cache_load(tmp, cfg));

    free(cfg_out);
    free(cfg);
    DeleteFileW(tmp);
}

static void test_cache_is_fresh_stale(void **state) {
    (void)state;
    wchar_t tmp[MAX_PATH];
    make_temp_path(tmp, MAX_PATH, L"jtest_stale_f.cache");
    wchar_t src_path[MAX_PATH];
    make_temp_path(src_path, MAX_PATH, L"jtest_stale_src.txt");

    /* Create source file */
    HANDLE hSrc = CreateFileW(src_path, GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    assert_true(hSrc != INVALID_HANDLE_VALUE);
    CloseHandle(hSrc);

    /* Get its real mtime */
    WIN32_FILE_ATTRIBUTE_DATA fdata;
    assert_true(GetFileAttributesExW(src_path, GetFileExInfoStandard, &fdata));

    /* Store in cache with a different mtime */
    CacheSourceFile sf = {0};
    wcscpy_s(sf.path, MAX_PATH, src_path);
    sf.mtime = fdata.ftLastWriteTime;
    sf.mtime.dwHighDateTime += 1; /* intentionally wrong */

    JumpConfig *cfg = calloc(1, sizeof(JumpConfig));
    assert_non_null(cfg);
    assert_int_equal(0, cache_save(tmp, cfg, &sf, 1));
    assert_int_equal(0, cache_is_fresh(tmp));

    free(cfg);
    DeleteFileW(tmp);
    DeleteFileW(src_path);
}

int run_cache_tests(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_cache_get_default_path),
        cmocka_unit_test(test_cache_save_load_roundtrip),
        cmocka_unit_test(test_cache_is_fresh_nonexistent),
        cmocka_unit_test(test_cache_is_fresh_after_save),
        cmocka_unit_test(test_cache_load_corrupted_magic),
        cmocka_unit_test(test_cache_load_nonexistent),
        cmocka_unit_test(test_cache_save_null_config),
        cmocka_unit_test(test_cache_save_bad_source_count),
        cmocka_unit_test(test_cache_save_invalid_path),
        cmocka_unit_test(test_cache_load_bad_counts),
        cmocka_unit_test(test_cache_load_null_config),
        cmocka_unit_test(test_cache_load_truncated),
        cmocka_unit_test(test_cache_load_truncated_shortcuts),
        cmocka_unit_test(test_cache_load_truncated_constants),
        cmocka_unit_test(test_cache_is_fresh_bad_magic),
        cmocka_unit_test(test_cache_is_fresh_truncated),
        cmocka_unit_test(test_cache_is_fresh_missing_source),
        cmocka_unit_test(test_cache_is_fresh_stale),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
