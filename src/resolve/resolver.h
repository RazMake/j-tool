/* Alias resolver — maps a user-typed alias to a fully-expanded
 * target path, applying constant/env-var expansion and parameter
 * substitution for EXEC shortcuts. */
#ifndef RESOLVER_H
#define RESOLVER_H

#include "types.h"

typedef struct {
    ShortcutType type;
    char expanded_target[MAX_PATH_LEN];
    char label[MAX_LABEL_LEN];
    int  hide_console;   /* EXEC-only: hide console window of spawned process */
} ResolveResult;

/*
 * Resolve an alias to an action.
 *
 * 1. Searches cfg->shortcuts for a matching alias (case-insensitive).
 * 2. Expands {{CONSTANT}} and {{ENV:VAR}} in the target.
 * 3. For SHORTCUT_EXEC: substitutes {1}, {2}, ... with params[0], params[1], ...
 *    and appends any remaining params beyond the highest placeholder.
 *
 * Returns 0 on success (result populated), J_EXIT_NOT_FOUND if alias not found.
 */
int resolve_alias(const JumpConfig *cfg, const char *alias,
                  int param_count, const char **params,
                  ResolveResult *result);

#endif /* RESOLVER_H */
