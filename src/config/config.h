#ifndef CONFIG_H
#define CONFIG_H

#include "types.h"

/*
 * Load configuration from INI files.
 *
 * 1. Reads %JUMPS% env var to find root config file.
 * 2. Checks binary cache freshness; loads from cache if fresh.
 * 3. Otherwise: parses root INI (auto-detect encoding: UTF-8 BOM, UTF-16 BOM,
 *    or system codepage), resolves [Include] entries, parses included files,
 *    merges [Constants] (error on duplicates), builds shortcuts.
 * 4. Validates: duplicate aliases, max aliases per section.
 * 5. Saves to binary cache for next time.
 *
 * Returns 0 on success, J_EXIT_CONFIG_ERROR on failure.
 * Error details are written to stderr.
 */
int config_load(JumpConfig *cfg);

/*
 * Expand {{CONSTANT}} and {{ENV:VARNAME}} placeholders in a string.
 * Writes the expanded result to out_buf (up to out_size bytes including NUL).
 *
 * Returns 0 on success, non-zero if an unknown constant is referenced
 * or the output buffer is too small.
 */
int config_expand(const JumpConfig *cfg, const char *input,
                  char *out_buf, size_t out_size);

/*
 * Validate a loaded config for semantic errors:
 *   - Duplicate aliases across all shortcuts.
 *   - Duplicate constant names.
 *   - Sections with more than MAX_ALIASES_PER_SHORTCUT aliases.
 *
 * Returns 0 if valid, non-zero if errors found (details written to stderr).
 */
int config_validate(const JumpConfig *cfg);

#endif /* CONFIG_H */
