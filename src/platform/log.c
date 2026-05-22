/* Diagnostic logging implementation.
 *
 * Uses a flag file (%TEMP%\jump_log.flag) to enable one-shot logging.
 * When active, writes timestamped lines to %TEMP%\jump.log.
 * The flag is cleared after log_close() so only one invocation is logged.
 */

#include "log.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define FLAG_FILENAME L"jump_log.flag"

static HANDLE s_log_file = INVALID_HANDLE_VALUE;
static int    s_active   = 0;
static wchar_t s_flag_path[MAX_PATH];
static wchar_t s_log_path[MAX_PATH];

/* Build %TEMP%\<filename> into dst. Returns 0 on success. */
static int build_temp_path(wchar_t *dst, size_t max_chars,
                           const wchar_t *filename) {
    DWORD len = GetTempPathW((DWORD)max_chars, dst);
    if (len == 0 || len >= max_chars) return -1;
    wcsncat_s(dst, max_chars, filename, _TRUNCATE);
    return 0;
}

int log_set_flag(void) {
    wchar_t path[MAX_PATH];
    HANDLE h;

    if (build_temp_path(path, MAX_PATH, FLAG_FILENAME) != 0)
        return -1;

    h = CreateFileW(path, GENERIC_WRITE, 0, NULL,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return -1;

    CloseHandle(h);
    return 0;
}

void log_init(void) {
    /* Build paths */
    if (build_temp_path(s_flag_path, MAX_PATH, FLAG_FILENAME) != 0) return;

    /* Check if flag file exists */
    if (GetFileAttributesW(s_flag_path) == INVALID_FILE_ATTRIBUTES)
        return;

    /* Build log filename with date-time stamp */
    {
        SYSTEMTIME st;
        wchar_t log_name[64];
        GetLocalTime(&st);
        _snwprintf_s(log_name, 64, _TRUNCATE,
                     L"jump_%04d%02d%02d_%02d%02d%02d.log",
                     st.wYear, st.wMonth, st.wDay,
                     st.wHour, st.wMinute, st.wSecond);
        if (build_temp_path(s_log_path, MAX_PATH, log_name) != 0) return;
    }

    /* Open log file */
    s_log_file = CreateFileW(s_log_path, GENERIC_WRITE, FILE_SHARE_READ,
                             NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (s_log_file == INVALID_HANDLE_VALUE)
        return;

    s_active = 1;

    /* Write header */
    log_write("LOG01", "=== Jump diagnostic log started ===");
}

void log_close(void) {
    if (!s_active) return;

    log_write("LOG02", "=== Jump diagnostic log ended ===");

    if (s_log_file != INVALID_HANDLE_VALUE) {
        CloseHandle(s_log_file);
        s_log_file = INVALID_HANDLE_VALUE;
    }

    /* Delete the flag file so logging is one-shot */
    DeleteFileW(s_flag_path);
    s_active = 0;
}

int log_enabled(void) {
    return s_active;
}

const wchar_t *log_get_path(void) {
    if (!s_active && s_log_path[0] == L'\0') return NULL;
    return s_log_path;
}

void log_write(const char *id, const char *fmt, ...) {
    char line[4096];
    SYSTEMTIME st;
    int prefix_len;
    va_list ap;
    int body_len;
    DWORD written;

    if (!s_active || s_log_file == INVALID_HANDLE_VALUE) return;

    GetLocalTime(&st);
    prefix_len = sprintf_s(line, sizeof(line),
                           "%04d-%02d-%02d %02d:%02d:%02d.%03d [%s] ",
                           st.wYear, st.wMonth, st.wDay,
                           st.wHour, st.wMinute, st.wSecond,
                           st.wMilliseconds, id);
    if (prefix_len < 0) return;

    va_start(ap, fmt);
    body_len = vsnprintf(line + prefix_len,
                         sizeof(line) - (size_t)prefix_len - 2,
                         fmt, ap);
    va_end(ap);

    if (body_len < 0) body_len = 0;
    prefix_len += body_len;

    /* Append newline */
    line[prefix_len++] = '\r';
    line[prefix_len++] = '\n';

    WriteFile(s_log_file, line, (DWORD)prefix_len, &written, NULL);
}
