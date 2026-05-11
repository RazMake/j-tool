#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ini_parser.h"

/* 1. Empty input → 0 sections */
static void test_ini_parse_empty(void **state) {
    (void)state;
    IniFile *f = calloc(1, sizeof(IniFile));
    assert_non_null(f);
    assert_int_equal(0, ini_parse("", 0, f));
    assert_int_equal(0, f->section_count);
    free(f);
}

/* Also test NULL text */
static void test_ini_parse_null(void **state) {
    (void)state;
    IniFile *f = calloc(1, sizeof(IniFile));
    assert_non_null(f);
    assert_int_equal(0, ini_parse(NULL, 0, f));
    assert_int_equal(0, f->section_count);
    free(f);
}

/* 2. Comments only → 0 sections */
static void test_ini_parse_comments_only(void **state) {
    (void)state;
    const char *text = "; this is a comment\n# another comment\n; more\n";
    IniFile *f = calloc(1, sizeof(IniFile));
    assert_non_null(f);
    assert_int_equal(0, ini_parse(text, strlen(text), f));
    assert_int_equal(0, f->section_count);
    free(f);
}

/* 3. Single section with key=value entries */
static void test_ini_parse_single_section(void **state) {
    (void)state;
    const char *text =
        "[Settings]\n"
        "color=blue\n"
        "size=42\n";
    IniFile *f = calloc(1, sizeof(IniFile));
    assert_non_null(f);
    assert_int_equal(0, ini_parse(text, strlen(text), f));
    assert_int_equal(1, f->section_count);
    assert_string_equal("Settings", f->sections[0].name);
    assert_int_equal(2, f->sections[0].entry_count);
    assert_string_equal("color", f->sections[0].entries[0].key);
    assert_string_equal("blue", f->sections[0].entries[0].value);
    assert_string_equal("size", f->sections[0].entries[1].key);
    assert_string_equal("42", f->sections[0].entries[1].value);
    free(f);
}

/* 4. Multiple sections */
static void test_ini_parse_multiple_sections(void **state) {
    (void)state;
    const char *text =
        "[First]\n"
        "a=1\n"
        "[Second]\n"
        "b=2\n"
        "[Third]\n"
        "c=3\n";
    IniFile *f = calloc(1, sizeof(IniFile));
    assert_non_null(f);
    assert_int_equal(0, ini_parse(text, strlen(text), f));
    assert_int_equal(3, f->section_count);
    assert_string_equal("First", f->sections[0].name);
    assert_string_equal("Second", f->sections[1].name);
    assert_string_equal("Third", f->sections[2].name);
    free(f);
}

/* 5. Key-value with whitespace trimming */
static void test_ini_parse_whitespace_trimming(void **state) {
    (void)state;
    const char *text =
        "[Trim]\n"
        "  key1  =  value1  \n"
        "\tkey2\t=\tvalue2\t\n";
    IniFile *f = calloc(1, sizeof(IniFile));
    assert_non_null(f);
    assert_int_equal(0, ini_parse(text, strlen(text), f));
    assert_int_equal(1, f->section_count);
    assert_int_equal(2, f->sections[0].entry_count);
    assert_string_equal("key1", f->sections[0].entries[0].key);
    assert_string_equal("value1", f->sections[0].entries[0].value);
    assert_string_equal("key2", f->sections[0].entries[1].key);
    assert_string_equal("value2", f->sections[0].entries[1].value);
    free(f);
}

/* 6. Lines without '=' (keyless entries for [Include] support) */
static void test_ini_parse_keyless_entries(void **state) {
    (void)state;
    const char *text =
        "[Include]\n"
        "somefile.ini\n"
        "anotherfile.ini\n";
    IniFile *f = calloc(1, sizeof(IniFile));
    assert_non_null(f);
    assert_int_equal(0, ini_parse(text, strlen(text), f));
    assert_int_equal(1, f->section_count);
    assert_int_equal(2, f->sections[0].entry_count);
    assert_string_equal("", f->sections[0].entries[0].key);
    assert_string_equal("somefile.ini", f->sections[0].entries[0].value);
    assert_string_equal("", f->sections[0].entries[1].key);
    assert_string_equal("anotherfile.ini", f->sections[0].entries[1].value);
    free(f);
}

/* 7. Section name case-insensitive find */
static void test_ini_find_section_case_insensitive(void **state) {
    (void)state;
    const char *text = "[MySection]\nkey=val\n";
    IniFile *f = calloc(1, sizeof(IniFile));
    assert_non_null(f);
    assert_int_equal(0, ini_parse(text, strlen(text), f));
    assert_non_null(ini_find_section(f, "mysection"));
    assert_non_null(ini_find_section(f, "MYSECTION"));
    assert_non_null(ini_find_section(f, "MySection"));
    assert_null(ini_find_section(f, "NoSuch"));
    free(f);
}

/* 8. Key case-insensitive find */
static void test_ini_find_value_case_insensitive(void **state) {
    (void)state;
    const char *text = "[S]\nMyKey=hello\n";
    IniFile *f = calloc(1, sizeof(IniFile));
    assert_non_null(f);
    assert_int_equal(0, ini_parse(text, strlen(text), f));
    const IniSection *s = ini_find_section(f, "S");
    assert_non_null(s);
    assert_string_equal("hello", ini_find_value(s, "mykey"));
    assert_string_equal("hello", ini_find_value(s, "MYKEY"));
    assert_string_equal("hello", ini_find_value(s, "MyKey"));
    assert_null(ini_find_value(s, "nokey"));
    free(f);
}

/* 9. Malformed: unclosed '[' bracket → error */
static void test_ini_parse_unclosed_bracket(void **state) {
    (void)state;
    const char *text = "[Broken\nkey=val\n";
    IniFile *f = calloc(1, sizeof(IniFile));
    assert_non_null(f);
    assert_int_not_equal(0, ini_parse(text, strlen(text), f));
    assert_true(strlen(f->error_msg) > 0);
    free(f);
}

/* 10. Overflow: too many sections → error */
static void test_ini_parse_too_many_sections(void **state) {
    (void)state;
    /* Build text with INI_MAX_SECTIONS + 1 sections */
    char *text = calloc(1, (INI_MAX_SECTIONS + 2) * 32);
    assert_non_null(text);
    for (int i = 0; i <= INI_MAX_SECTIONS; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "[Section%d]\n", i);
        strcat(text, buf);
    }
    IniFile *f = calloc(1, sizeof(IniFile));
    assert_non_null(f);
    assert_int_not_equal(0, ini_parse(text, strlen(text), f));
    assert_true(strlen(f->error_msg) > 0);
    free(f);
    free(text);
}

/* 11. Overflow: too many entries in one section → error */
static void test_ini_parse_too_many_entries(void **state) {
    (void)state;
    /* Build text with one section and INI_MAX_ENTRIES + 1 entries */
    char *text = calloc(1, (INI_MAX_ENTRIES + 2) * 32);
    assert_non_null(text);
    strcat(text, "[Big]\n");
    for (int i = 0; i <= INI_MAX_ENTRIES; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "k%d=v%d\n", i, i);
        strcat(text, buf);
    }
    IniFile *f = calloc(1, sizeof(IniFile));
    assert_non_null(f);
    assert_int_not_equal(0, ini_parse(text, strlen(text), f));
    assert_true(strlen(f->error_msg) > 0);
    free(f);
    free(text);
}

/* 12. Content before first section → ignored */
static void test_ini_parse_content_before_section(void **state) {
    (void)state;
    const char *text =
        "orphan_key=orphan_val\n"
        "stray line\n"
        "[Real]\n"
        "key=value\n";
    IniFile *f = calloc(1, sizeof(IniFile));
    assert_non_null(f);
    assert_int_equal(0, ini_parse(text, strlen(text), f));
    assert_int_equal(1, f->section_count);
    assert_string_equal("Real", f->sections[0].name);
    assert_int_equal(1, f->sections[0].entry_count);
    free(f);
}

/* 13. Empty section (header only, no entries) */
static void test_ini_parse_empty_section(void **state) {
    (void)state;
    const char *text = "[Empty]\n[Another]\nk=v\n";
    IniFile *f = calloc(1, sizeof(IniFile));
    assert_non_null(f);
    assert_int_equal(0, ini_parse(text, strlen(text), f));
    assert_int_equal(2, f->section_count);
    assert_string_equal("Empty", f->sections[0].name);
    assert_int_equal(0, f->sections[0].entry_count);
    assert_string_equal("Another", f->sections[1].name);
    assert_int_equal(1, f->sections[1].entry_count);
    free(f);
}

/* 14. Value containing '=' (only first '=' splits key from value) */
static void test_ini_parse_value_with_equals(void **state) {
    (void)state;
    const char *text = "[Math]\nexpr=a=b=c\n";
    IniFile *f = calloc(1, sizeof(IniFile));
    assert_non_null(f);
    assert_int_equal(0, ini_parse(text, strlen(text), f));
    assert_int_equal(1, f->section_count);
    assert_string_equal("expr", f->sections[0].entries[0].key);
    assert_string_equal("a=b=c", f->sections[0].entries[0].value);
    free(f);
}

/* Extra: ini_find_section / ini_find_value with NULL args */
static void test_ini_find_null_args(void **state) {
    (void)state;
    assert_null(ini_find_section(NULL, "x"));
    assert_null(ini_find_section(NULL, NULL));
    assert_null(ini_find_value(NULL, "x"));
    assert_null(ini_find_value(NULL, NULL));
}

int run_ini_parser_tests(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ini_parse_empty),
        cmocka_unit_test(test_ini_parse_null),
        cmocka_unit_test(test_ini_parse_comments_only),
        cmocka_unit_test(test_ini_parse_single_section),
        cmocka_unit_test(test_ini_parse_multiple_sections),
        cmocka_unit_test(test_ini_parse_whitespace_trimming),
        cmocka_unit_test(test_ini_parse_keyless_entries),
        cmocka_unit_test(test_ini_find_section_case_insensitive),
        cmocka_unit_test(test_ini_find_value_case_insensitive),
        cmocka_unit_test(test_ini_parse_unclosed_bracket),
        cmocka_unit_test(test_ini_parse_too_many_sections),
        cmocka_unit_test(test_ini_parse_too_many_entries),
        cmocka_unit_test(test_ini_parse_content_before_section),
        cmocka_unit_test(test_ini_parse_empty_section),
        cmocka_unit_test(test_ini_parse_value_with_equals),
        cmocka_unit_test(test_ini_find_null_args),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
