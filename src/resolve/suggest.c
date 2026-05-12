#include "suggest.h"
#include <string.h>
#include <ctype.h>

int levenshtein_distance(const char *a, const char *b) {
    int len_a = (int)strlen(a);
    int len_b = (int)strlen(b);

    /* Two-row approach: only prev and curr rows needed.
     * MAX_ALIAS_LEN is 40, so max len_b+1 is 41 — fits on stack. */
    int prev[MAX_ALIAS_LEN + 1];
    int curr[MAX_ALIAS_LEN + 1];

    if (len_b > MAX_ALIAS_LEN) len_b = MAX_ALIAS_LEN;

    for (int j = 0; j <= len_b; j++) prev[j] = j;

    for (int i = 1; i <= len_a; i++) {
        curr[0] = i;
        for (int j = 1; j <= len_b; j++) {
            int cost = (tolower((unsigned char)a[i - 1]) == tolower((unsigned char)b[j - 1])) ? 0 : 1;
            int del = prev[j] + 1;
            int ins = curr[j - 1] + 1;
            int sub = prev[j - 1] + cost;
            int min = del < ins ? del : ins;
            curr[j] = min < sub ? min : sub;
        }
        memcpy(prev, curr, (size_t)(len_b + 1) * sizeof(int));
    }

    return prev[len_b];
}

int suggest_aliases(const JumpConfig *cfg, const char *input,
                    Suggestion *suggestions, int max_suggestions) {
    int count = 0;

    for (int i = 0; i < cfg->shortcut_count; i++) {
        for (int j = 0; j < cfg->shortcuts[i].alias_count; j++) {
            const char *alias = cfg->shortcuts[i].aliases[j];
            int dist = levenshtein_distance(input, alias);
            if (dist == 0) continue; /* skip exact matches */

            if (count < max_suggestions) {
                /* Still filling up the suggestions array */
                strncpy(suggestions[count].alias, alias, MAX_ALIAS_LEN - 1);
                suggestions[count].alias[MAX_ALIAS_LEN - 1] = '\0';
                /* Copy the parent shortcut's label for display */
                strncpy(suggestions[count].label, cfg->shortcuts[i].label, MAX_LABEL_LEN - 1);
                suggestions[count].label[MAX_LABEL_LEN - 1] = '\0';
                suggestions[count].distance = dist;
                count++;
                /* Keep sorted by insertion sort */
                for (int k = count - 1; k > 0 && suggestions[k].distance < suggestions[k - 1].distance; k--) {
                    Suggestion tmp = suggestions[k];
                    suggestions[k] = suggestions[k - 1];
                    suggestions[k - 1] = tmp;
                }
            } else if (dist < suggestions[count - 1].distance) {
                /* Replace the worst (last) suggestion */
                strncpy(suggestions[count - 1].alias, alias, MAX_ALIAS_LEN - 1);
                suggestions[count - 1].alias[MAX_ALIAS_LEN - 1] = '\0';
                /* Copy the parent shortcut's label for display */
                strncpy(suggestions[count - 1].label, cfg->shortcuts[i].label, MAX_LABEL_LEN - 1);
                suggestions[count - 1].label[MAX_LABEL_LEN - 1] = '\0';
                suggestions[count - 1].distance = dist;
                /* Re-sort */
                for (int k = count - 1; k > 0 && suggestions[k].distance < suggestions[k - 1].distance; k--) {
                    Suggestion tmp = suggestions[k];
                    suggestions[k] = suggestions[k - 1];
                    suggestions[k - 1] = tmp;
                }
            }
        }
    }

    return count;
}
