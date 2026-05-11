#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>
#include <string.h>
#include "suggest.h"

/* 1. Identical strings → distance 0 */
static void test_levenshtein_identical(void **state) {
    (void)state;
    assert_int_equal(0, levenshtein_distance("hello", "hello"));
}

/* 2. Empty vs non-empty string */
static void test_levenshtein_empty_vs_nonempty(void **state) {
    (void)state;
    assert_int_equal(5, levenshtein_distance("", "hello"));
    assert_int_equal(3, levenshtein_distance("abc", ""));
}

/* 3. Both empty → distance 0 */
static void test_levenshtein_both_empty(void **state) {
    (void)state;
    assert_int_equal(0, levenshtein_distance("", ""));
}

/* 4. Single character difference */
static void test_levenshtein_single_char_diff(void **state) {
    (void)state;
    assert_int_equal(1, levenshtein_distance("cat", "bat"));  /* substitution */
    assert_int_equal(1, levenshtein_distance("cat", "cats")); /* insertion */
    assert_int_equal(1, levenshtein_distance("cats", "cat")); /* deletion */
}

/* 5. Multiple edits */
static void test_levenshtein_multiple_edits(void **state) {
    (void)state;
    assert_int_equal(3, levenshtein_distance("kitten", "sitting"));
    assert_int_equal(2, levenshtein_distance("book", "back"));
}

/* 6. Case insensitive: "Hello" vs "hello" → distance 0 */
static void test_levenshtein_case_insensitive(void **state) {
    (void)state;
    assert_int_equal(0, levenshtein_distance("Hello", "hello"));
    assert_int_equal(0, levenshtein_distance("ABC", "abc"));
    assert_int_equal(1, levenshtein_distance("Hello", "hallo"));
}

/* Helper to create a JumpConfig with given aliases */
static JumpConfig *make_config(const char *aliases[], int count) {
    JumpConfig *cfg = (JumpConfig *)calloc(1, sizeof(JumpConfig));
    /* Put each alias as a single-alias shortcut */
    for (int i = 0; i < count && i < MAX_SHORTCUTS; i++) {
        strncpy(cfg->shortcuts[i].aliases[0], aliases[i], MAX_ALIAS_LEN - 1);
        cfg->shortcuts[i].alias_count = 1;
    }
    cfg->shortcut_count = count < MAX_SHORTCUTS ? count : MAX_SHORTCUTS;
    return cfg;
}

/* 7. suggest_aliases with empty config → 0 suggestions */
static void test_suggest_empty_config(void **state) {
    (void)state;
    JumpConfig *cfg = (JumpConfig *)calloc(1, sizeof(JumpConfig));
    cfg->shortcut_count = 0;
    Suggestion suggestions[MAX_SUGGESTIONS];
    int n = suggest_aliases(cfg, "anything", suggestions, MAX_SUGGESTIONS);
    assert_int_equal(0, n);
    free(cfg);
}

/* 8. suggest_aliases returns correct top-3 sorted by distance */
static void test_suggest_top3_sorted(void **state) {
    (void)state;
    const char *aliases[] = {"work", "home", "docs", "downloads", "projects"};
    JumpConfig *cfg = make_config(aliases, 5);
    Suggestion suggestions[MAX_SUGGESTIONS];

    int n = suggest_aliases(cfg, "hone", suggestions, MAX_SUGGESTIONS);
    assert_true(n > 0);
    assert_true(n <= MAX_SUGGESTIONS);
    /* Results must be sorted ascending by distance */
    for (int i = 1; i < n; i++) {
        assert_true(suggestions[i].distance >= suggestions[i - 1].distance);
    }
    /* "home" should be first (distance 1 from "hone") */
    assert_string_equal("home", suggestions[0].alias);
    assert_int_equal(1, suggestions[0].distance);

    free(cfg);
}

/* 9. suggest_aliases skips exact matches */
static void test_suggest_skips_exact(void **state) {
    (void)state;
    const char *aliases[] = {"home", "hone", "dome"};
    JumpConfig *cfg = make_config(aliases, 3);
    Suggestion suggestions[MAX_SUGGESTIONS];

    int n = suggest_aliases(cfg, "home", suggestions, MAX_SUGGESTIONS);
    /* "home" is exact match, should be skipped */
    for (int i = 0; i < n; i++) {
        assert_true(strcmp(suggestions[i].alias, "home") != 0);
    }
    /* "hone" and "dome" should appear (both distance 1) */
    assert_int_equal(2, n);

    free(cfg);
}

/* 10. suggest_aliases with fewer aliases than max_suggestions */
static void test_suggest_fewer_than_max(void **state) {
    (void)state;
    const char *aliases[] = {"foo"};
    JumpConfig *cfg = make_config(aliases, 1);
    Suggestion suggestions[MAX_SUGGESTIONS];

    int n = suggest_aliases(cfg, "bar", suggestions, MAX_SUGGESTIONS);
    assert_int_equal(1, n);
    assert_string_equal("foo", suggestions[0].alias);

    free(cfg);
}

/* 11. suggest_aliases replaces worst when array is full */
static void test_suggest_replaces_worst(void **state) {
    (void)state;
    /* 4 distant aliases, then 1 close alias → must replace a distant one */
    const char *aliases[] = {"aaaa", "bbbb", "cccc", "dddd", "hom"};
    JumpConfig *cfg = make_config(aliases, 5);
    Suggestion suggestions[MAX_SUGGESTIONS];

    int n = suggest_aliases(cfg, "home", suggestions, MAX_SUGGESTIONS);
    assert_int_equal(MAX_SUGGESTIONS, n);

    /* "hom" (distance 1) must appear in results */
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(suggestions[i].alias, "hom") == 0) {
            found = 1;
            assert_int_equal(1, suggestions[i].distance);
        }
    }
    assert_true(found);

    /* Results remain sorted ascending by distance */
    for (int i = 1; i < n; i++)
        assert_true(suggestions[i].distance >= suggestions[i - 1].distance);

    free(cfg);
}

int run_suggest_tests(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_levenshtein_identical),
        cmocka_unit_test(test_levenshtein_empty_vs_nonempty),
        cmocka_unit_test(test_levenshtein_both_empty),
        cmocka_unit_test(test_levenshtein_single_char_diff),
        cmocka_unit_test(test_levenshtein_multiple_edits),
        cmocka_unit_test(test_levenshtein_case_insensitive),
        cmocka_unit_test(test_suggest_empty_config),
        cmocka_unit_test(test_suggest_top3_sorted),
        cmocka_unit_test(test_suggest_skips_exact),
        cmocka_unit_test(test_suggest_fewer_than_max),
        cmocka_unit_test(test_suggest_replaces_worst),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
