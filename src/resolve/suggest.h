#ifndef SUGGEST_H
#define SUGGEST_H

#include "types.h"

#define MAX_SUGGESTIONS 3

typedef struct {
    char alias[MAX_ALIAS_LEN];
    char label[MAX_LABEL_LEN];  /* Label of the shortcut (for display) */
    int  distance;
} Suggestion;

/*
 * Compute Levenshtein edit distance between two strings.
 * Comparison is case-insensitive.
 */
int levenshtein_distance(const char *a, const char *b);

/*
 * Find the closest matching aliases for a given input string.
 * Searches all aliases in all shortcuts in cfg.
 * Populates the suggestions array (up to max_suggestions entries),
 * sorted by distance ascending.
 * Returns the number of suggestions found.
 */
int suggest_aliases(const JumpConfig *cfg, const char *input,
                    Suggestion *suggestions, int max_suggestions);

#endif /* SUGGEST_H */
