/*
 * error.h — Error reporting interface for Jump.
 *
 * Provides a unified error reporting mechanism that works in both
 * console mode (jc.exe) and GUI mode (j.exe). Console mode prints
 * red text to stderr; GUI mode shows a dismissable popup window.
 */

#ifndef ERROR_H
#define ERROR_H

/*
 * error_init — Initialize the error reporting subsystem.
 *
 * Call once at startup before any error_report() calls.
 * Detects whether the process has an attached console
 * (using GetConsoleWindow) to decide between stderr output
 * and GUI popup for subsequent error reports.
 */
void error_init(void);

/*
 * error_report — Report an error message to the user.
 *
 * Accepts printf-style format string and arguments.
 * In console mode: prints red text to stderr.
 * In GUI mode: shows a semi-transparent popup window that
 * auto-dismisses after 5 seconds or on Escape.
 */
void error_report(const char *fmt, ...);

#endif /* ERROR_H */
