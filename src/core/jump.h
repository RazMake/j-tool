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

#endif /* JUMP_MAIN_H */
