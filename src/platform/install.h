/* Shell integration installer — registers the `j` command in CMD,
 * PowerShell and Git Bash, adds the exe to PATH, and handles uninstallation. */
#ifndef INSTALL_H
#define INSTALL_H

/*
 * Install Jump tool integration:
 *   1. Set JUMPS env var (HKCU\Environment) if not already set (prompts user).
 *   2. Detect Total Commander from HKLM registry.
 *   3. Create DOSKEY macro in HKCU\...\Command Processor\AutoRun.
 *   4. Append PowerShell function to $PROFILE (with BEGIN/END JUMP markers).
 *   4b. Append Git Bash function to ~/.bashrc (with BEGIN/END JUMP markers).
 *   5. Add j.exe directory to HKCU\Environment\Path.
 *   6. Broadcast WM_SETTINGCHANGE.
 *
 * tc_panel: "L" or "R" (NULL defaults to "L").
 * All operations are idempotent - safe to re-run.
 *
 * Returns 0 on success, non-zero on failure.
 * Progress and errors written to stderr.
 */
int jump_install(const char *tc_panel);

/*
 * Uninstall Jump tool integration:
 *   1. Remove directory from HKCU\Environment\Path.
 *   2. Remove DOSKEY macro from AutoRun.
 *   3. Remove PowerShell function from $PROFILE (by markers).
 *   3b. Remove Git Bash function from ~/.bashrc (by markers).
 *   4. Broadcast WM_SETTINGCHANGE.
 *
 * Returns 0 on success, non-zero on failure.
 * Progress and errors written to stderr.
 */
int jump_uninstall(void);

#endif /* INSTALL_H */
