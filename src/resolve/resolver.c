/* Alias resolution engine — looks up a user-supplied alias in the
 * parsed config, expands constants and environment variables in the
 * target path, and substitutes positional parameters ({1},{2},...)
 * for EXEC-type shortcuts. */
#include "resolver.h"
#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int resolve_alias(const JumpConfig *cfg, const char *alias,
                  int param_count, const char **params,
                  ResolveResult *result) {
    memset(result, 0, sizeof(*result));

    /* Search for matching alias (case-insensitive) */
    const Shortcut *match = NULL;
    for (int i = 0; i < cfg->shortcut_count; i++) {
        const Shortcut *sc = &cfg->shortcuts[i];
        for (int a = 0; a < sc->alias_count; a++) {
            if (_stricmp(sc->aliases[a], alias) == 0) {
                match = sc;
                break;
            }
        }
        if (match) break;
    }

    if (!match)
        return J_EXIT_NOT_FOUND;

    /* Populate result */
    result->type = match->type;
    result->hide_console = match->hide_console;
    strncpy(result->label, match->label, MAX_LABEL_LEN - 1);
    result->label[MAX_LABEL_LEN - 1] = '\0';

    /* Expand constants/env vars in target */
    config_expand(cfg, match->target, result->expanded_target, MAX_PATH_LEN);

    /* For EXEC: substitute {1},{2},... and append remaining params */
    if (match->type == SHORTCUT_EXEC && param_count > 0 && params) {
        char buf[MAX_PATH_LEN];
        const char *src = result->expanded_target;
        int highest_placeholder = 0;  /* highest {N} found */
        int has_placeholder = 0;

        /* First pass: find highest placeholder index */
        for (const char *p = src; *p; p++) {
            if (*p == '{' && p[1] >= '1' && p[1] <= '9') {
                int n = 0;
                const char *d = p + 1;
                while (*d >= '0' && *d <= '9') {
                    n = n * 10 + (*d - '0');
                    d++;
                }
                if (*d == '}') {
                    has_placeholder = 1;
                    if (n > highest_placeholder)
                        highest_placeholder = n;
                }
            }
        }

        if (has_placeholder) {
            /* Second pass: substitute placeholders */
            char *dst = buf;
            char *end = buf + MAX_PATH_LEN - 1;
            for (const char *p = src; *p && dst < end; ) {
                if (*p == '{' && p[1] >= '1' && p[1] <= '9') {
                    int n = 0;
                    const char *d = p + 1;
                    while (*d >= '0' && *d <= '9') {
                        n = n * 10 + (*d - '0');
                        d++;
                    }
                    if (*d == '}') {
                        /* {N} → params[N-1] if available, else remove */
                        int idx = n - 1;
                        if (idx < param_count) {
                            size_t len = strlen(params[idx]);
                            if (dst + len < end) {
                                memcpy(dst, params[idx], len);
                                dst += len;
                            }
                        }
                        p = d + 1;
                        continue;
                    }
                }
                *dst++ = *p++;
            }
            *dst = '\0';

            /* Append remaining params beyond highest placeholder */
            for (int i = highest_placeholder; i < param_count; i++) {
                size_t cur_len = strlen(buf);
                size_t param_len = strlen(params[i]);
                if (cur_len + 1 + param_len < MAX_PATH_LEN) {
                    buf[cur_len] = ' ';
                    memcpy(buf + cur_len + 1, params[i], param_len + 1);
                }
            }

            strncpy(result->expanded_target, buf, MAX_PATH_LEN - 1);
            result->expanded_target[MAX_PATH_LEN - 1] = '\0';
        } else {
            /* No placeholders: append all params */
            char *dst = result->expanded_target;
            size_t cur_len = strlen(dst);
            for (int i = 0; i < param_count; i++) {
                size_t param_len = strlen(params[i]);
                if (cur_len + 1 + param_len < MAX_PATH_LEN) {
                    dst[cur_len] = ' ';
                    memcpy(dst + cur_len + 1, params[i], param_len + 1);
                    cur_len += 1 + param_len;
                }
            }
        }
    }

    return 0;
}
