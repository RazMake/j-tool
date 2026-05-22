/* Diagnostic logging — writes timestamped, tagged log lines to a file
 * for debugging. Activated by the --log flag (one-shot: auto-clears
 * after the next command completes).
 *
 * Flag file:  %TEMP%\jump_log.flag   (created by --log, deleted after use)
 * Log output: %TEMP%\jump.log        (overwritten each logging session)
 */
#ifndef LOG_H
#define LOG_H

#include <wchar.h>

/*
 * log_set_flag — Create the one-shot flag file so the NEXT invocation
 *                logs its operation. Does not read config or cache.
 *                Returns 0 on success, -1 on failure.
 */
int log_set_flag(void);

/*
 * log_init — Check for the flag file. If present, open the log file
 *            and enable logging for this invocation.
 *            Call once at the top of jump_main(), before any other work.
 */
void log_init(void);

/*
 * log_close — Flush and close the log file, then delete the flag file
 *             so logging is not repeated on the next run.
 *             Call once at the end of jump_main().
 */
void log_close(void);

/*
 * log_enabled — Returns non-zero if logging is active for this run.
 */
int log_enabled(void);

/*
 * log_get_path — Return the full path of the current log file,
 *                or NULL if logging is not active.
 */
const wchar_t *log_get_path(void);

/*
 * log_write — Write a tagged, timestamped line to the log file.
 *
 *   id:  Short identifier that maps to the call site, e.g. "JMP01".
 *   fmt: printf-style format string, followed by arguments.
 *
 * No-op if logging is not active.
 */
void log_write(const char *id, const char *fmt, ...);

#endif /* LOG_H */
