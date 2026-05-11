#ifndef TC_H
#define TC_H

#include <stddef.h>

/*
 * Find Total Commander executable path from registry.
 * Reads HKLM\Software\Ghisler\Total Commander for the InstallDir,
 * then appends TOTALCMD64.EXE (or TOTALCMD.EXE).
 *
 * Returns 0 on success (tc_path filled), non-zero if TC not found.
 */
int tc_find_path(char *tc_path, size_t tc_path_size);

/*
 * Build a Total Commander command line to change directory.
 *   tc_path:   full path to TC executable
 *   panel:     "L" (left) or "R" (right)
 *   directory: target directory path
 *   cmd_buf:   output buffer for the full command line
 *   cmd_size:  size of cmd_buf
 *
 * Produces: "tc_path" /O /S /L="directory"  (or /R= for right panel)
 * Returns 0 on success.
 */
int tc_build_cd_command(const char *tc_path, const char *panel,
                        const char *directory, char *cmd_buf, size_t cmd_size);

/*
 * Navigate Total Commander's source (active) panel to `directory`.
 *
 * Checks whether TC is already running (window class "TTOTAL_CMD").
 * If TC is running, finds its executable and launches:
 *     "tc_path" /O /S /L="directory"
 * which tells the existing instance to change the source panel.
 *
 * Returns 0 if TC was navigated, non-zero if TC is not running or not found.
 */
int tc_navigate(const char *directory);

#endif /* TC_H */
