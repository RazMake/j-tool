#ifndef JUMP_MAIN_H
#define JUMP_MAIN_H

/*
 * Main entry point for Jump tool.
 * Called by both console (main.c) and windows (main_win.c) entry points.
 *
 * Dispatches based on argv:
 *   j <alias> [params...]       - resolve alias, perform action, spawn OSD
 *   j --osd "text"              - show OSD overlay
 *   jc --install [--tc-panel=X] - install integration
 *   jc --uninstall              - uninstall integration
 *   jc --list                   - list all aliases
 *   j (no args)                 - print usage/help
 *
 * Returns J_EXIT_OK, J_EXIT_NOT_FOUND, J_EXIT_CONFIG_ERROR, or J_EXIT_RUNTIME_ERROR.
 */
int jump_main(int argc, char *argv[]);

/* Returns 1 if the EXEC command line targets a .cmd or .bat script,
 * meaning it needs a cmd.exe /c wrapper to run via CreateProcessA. */
int exec_needs_cmd_wrapper(const char *cmd_line);

/* Returns 1 if the EXEC command line targets a .ps1 script,
 * meaning it needs a PowerShell wrapper (pwsh or powershell) to run. */
int exec_needs_ps_wrapper(const char *cmd_line);

/* Returns 1 if the EXEC command line targets a .vbs script,
 * meaning it needs a script-host wrapper (cscript or wscript) to run. */
int exec_needs_vbs_wrapper(const char *cmd_line);

#endif /* JUMP_MAIN_H */
