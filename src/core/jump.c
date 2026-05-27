/*
 * jump.c - Core dispatch logic for the Jump shortcut launcher.
 *
 * Parses CLI arguments, dispatches to the appropriate mode (resolve alias,
 * install/uninstall, list, OSD overlay, update), and executes the resolved
 * shortcut action (CD, OPEN, or EXEC).
 */

#include "jump.h"
#include "types.h"
#include "config.h"
#include "resolver.h"
#include "suggest.h"
#include "osd.h"
#include "error.h"
#include "install.h"
#include "tc.h"
#include "version.h"
#include "update.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* Print usage/help text to stderr */
static void print_usage(void) {
    fprintf(stderr,
        "Jump v%s - quick directory/URL/program launcher\n"
        "\n"
        "Usage:\n"
        "  j  <alias> [params...]       Resolve alias and perform action\n"
        "  jc <alias> [params...]       Same, with console output\n"
        "  jc --install [--tc-panel=X]  Install shell integration (X=L|R)\n"
        "  jc --uninstall               Remove shell integration\n"
        "  jc --list                    List all defined aliases\n"
        "  jc --version                 Show version information\n"
        "  jc --update                  Check for updates and install\n"
        "  jc --log                     Enable logging for the next command\n"
        "  j  --osd \"text\"              Show OSD overlay (internal)\n"
        "\n"
        "Environment:\n"
        "  JUMPS  Path to root INI configuration file\n",
        JUMP_VERSION);
}

/* Print all defined shortcuts in tabular format */
static void print_list(const JumpConfig *cfg) {
    int i, j;
    for (i = 0; i < cfg->shortcut_count; i++) {
        const Shortcut *s = &cfg->shortcuts[i];
        const char *type_str = "CD";
        if (s->type == SHORTCUT_OPEN) type_str = "OPEN";
        else if (s->type == SHORTCUT_EXEC) type_str = "EXEC";

        fprintf(stderr, "  %-6s ", type_str);
        for (j = 0; j < s->alias_count; j++) {
            if (j > 0) fprintf(stderr, ",");
            fprintf(stderr, "%s", s->aliases[j]);
        }
        if (s->label[0])
            fprintf(stderr, "  - %s", s->label);
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n%d shortcut(s) defined.\n", cfg->shortcut_count);
}

/* Map ShortcutType to icon name string for OSD spawning */
static const char *icon_arg(ShortcutType type) {
    switch (type) {
    case SHORTCUT_CD:   return "cd";
    case SHORTCUT_OPEN: return "open";
    case SHORTCUT_EXEC: return "exec";
    default:            return "cd";
    }
}

/* Launch j.exe in a separate process to show OSD notification */
static void spawn_osd(const char *text, ShortcutType type) {
    char exe_path[MAX_PATH];
    char cmd_line[MAX_PATH + MAX_LABEL_LEN + 64];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    if (!text || !text[0]) return;

    /* Get path to this executable, replace filename with j.exe for OSD */
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    {
        char *last_sep = strrchr(exe_path, '\\');
        if (last_sep) {
            strcpy_s(last_sep + 1, MAX_PATH - (last_sep - exe_path + 1), "j.exe");
        }
    }

    sprintf_s(cmd_line, sizeof(cmd_line), "\"%s\" --osd \"%s\" %s",
              exe_path, text, icon_arg(type));

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    memset(&pi, 0, sizeof(pi));

    if (CreateProcessA(NULL, cmd_line, NULL, NULL, FALSE,
                       DETACHED_PROCESS | CREATE_NO_WINDOW,
                       NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

/* Write a temp batch file used by DOSKEY macro for CD support */
static void write_temp_cmd(const char *content) {
    char path[MAX_PATH];
    DWORD len;
    HANDLE hFile;
    DWORD written;

    len = GetTempPathA(MAX_PATH, path);
    if (len == 0 || len >= MAX_PATH) return;
    strcat_s(path, MAX_PATH, "jump_cd.cmd");

    hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return;
    WriteFile(hFile, content, (DWORD)strlen(content), &written, NULL);
    CloseHandle(hFile);
}

/* Check if an EXEC command line targets a .cmd or .bat script.
 * Extracts the first token (respecting double-quotes) and checks extension. */
int exec_needs_cmd_wrapper(const char *cmd_line) {
    char first_token[MAX_PATH];
    const char *p = cmd_line;
    size_t len;

    /* Skip leading whitespace */
    while (*p == ' ' || *p == '\t') p++;

    if (*p == '"') {
        /* Quoted path: extract contents between quotes */
        p++;
        const char *close = strchr(p, '"');
        if (!close) return 0;
        len = (size_t)(close - p);
    } else {
        /* Unquoted: first token ends at space or end of string */
        const char *end = p;
        while (*end && *end != ' ' && *end != '\t') end++;
        len = (size_t)(end - p);
    }

    if (len >= MAX_PATH || len < 4) return 0;
    memcpy(first_token, p, len);
    first_token[len] = '\0';

    /* Check for .cmd or .bat extension (case-insensitive) */
    const char *ext = first_token + len - 4;
    return (_stricmp(ext, ".cmd") == 0 || _stricmp(ext, ".bat") == 0);
}

/* Check if an EXEC command line targets a .ps1 script.
 * Extracts the first token (respecting double-quotes) and checks extension. */
int exec_needs_ps_wrapper(const char *cmd_line) {
    char first_token[MAX_PATH];
    const char *p = cmd_line;
    size_t len;

    /* Skip leading whitespace */
    while (*p == ' ' || *p == '\t') p++;

    if (*p == '"') {
        /* Quoted path: extract contents between quotes */
        p++;
        const char *close = strchr(p, '"');
        if (!close) return 0;
        len = (size_t)(close - p);
    } else {
        /* Unquoted: first token ends at space or end of string */
        const char *end = p;
        while (*end && *end != ' ' && *end != '\t') end++;
        len = (size_t)(end - p);
    }

    if (len >= MAX_PATH || len < 4) return 0;
    memcpy(first_token, p, len);
    first_token[len] = '\0';

    /* Check for .ps1 extension (case-insensitive) */
    const char *ext = first_token + len - 4;
    return (_stricmp(ext, ".ps1") == 0);
}

/* Check if an EXEC command line targets a .vbs script.
 * Extracts the first token (respecting double-quotes) and checks extension. */
int exec_needs_vbs_wrapper(const char *cmd_line) {
    char first_token[MAX_PATH];
    const char *p = cmd_line;
    size_t len;

    /* Skip leading whitespace */
    while (*p == ' ' || *p == '\t') p++;

    if (*p == '"') {
        /* Quoted path: extract contents between quotes */
        p++;
        const char *close = strchr(p, '"');
        if (!close) return 0;
        len = (size_t)(close - p);
    } else {
        /* Unquoted: first token ends at space or end of string */
        const char *end = p;
        while (*end && *end != ' ' && *end != '\t') end++;
        len = (size_t)(end - p);
    }

    if (len >= MAX_PATH || len < 4) return 0;
    memcpy(first_token, p, len);
    first_token[len] = '\0';

    /* Check for .vbs extension (case-insensitive) */
    const char *ext = first_token + len - 4;
    return (_stricmp(ext, ".vbs") == 0);
}

/* Execute the resolved shortcut action (CD, OPEN, or EXEC).
 * Returns the child process handle for visible EXEC shortcuts
 * (caller must wait and close it), or NULL otherwise. */
static HANDLE perform_action(const ResolveResult *result) {
    log_write("JMP30", "perform_action: type=%d, target='%s'",
              result->type, result->expanded_target);
    switch (result->type) {
    case SHORTCUT_CD: {
        /* Navigate TC panel only if launched from TC */
        if (tc_is_ancestor()) {
            log_write("JMP31", "TC ancestor detected, navigating panel");
            tc_navigate(result->expanded_target);
        }

        /* If no console is attached and we're not inside Total Commander
         * (e.g. launched from Win+R Run dialog), open a new cmd window
         * at the target directory. */
        if (!GetConsoleWindow() && !tc_is_ancestor()) {
            STARTUPINFOA si_cd;
            PROCESS_INFORMATION pi_cd;
            char cmd_run[MAX_PATH_LEN + 32];
            log_write("JMP31b", "No console detected, opening cmd at '%s'",
                      result->expanded_target);
            sprintf_s(cmd_run, sizeof(cmd_run), "cmd.exe /k cd /d \"%s\"",
                      result->expanded_target);
            memset(&si_cd, 0, sizeof(si_cd));
            si_cd.cb = sizeof(si_cd);
            memset(&pi_cd, 0, sizeof(pi_cd));
            if (CreateProcessA(NULL, cmd_run, NULL, NULL, FALSE,
                               CREATE_NEW_CONSOLE, NULL, NULL,
                               &si_cd, &pi_cd)) {
                CloseHandle(pi_cd.hThread);
                CloseHandle(pi_cd.hProcess);
            }
            break;
        }

        /* Write path to stdout for PowerShell wrapper */
        HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hStdout && hStdout != INVALID_HANDLE_VALUE) {
            DWORD written;
            WriteFile(hStdout, result->expanded_target,
                      (DWORD)strlen(result->expanded_target), &written, NULL);
            WriteFile(hStdout, "\n", 1, &written, NULL);
            log_write("JMP32", "CD: wrote path to stdout");
        } else {
            log_write("JMP33", "CD: stdout handle unavailable");
        }
        /* Write temp cmd file for DOSKEY macro */
        {
            char cmd[MAX_PATH_LEN + 64];
            sprintf_s(cmd, sizeof(cmd), "@cd /d \"%s\"\n",
                      result->expanded_target);
            write_temp_cmd(cmd);
        }
        break;
    }
    case SHORTCUT_OPEN:
        log_write("JMP34", "OPEN: ShellExecute('%s')", result->expanded_target);
        ShellExecuteA(NULL, "open", result->expanded_target,
                      NULL, NULL, SW_SHOWNORMAL);
        write_temp_cmd("@rem\n");
        break;

    case SHORTCUT_EXEC: {
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        char cmd_copy[MAX_PATH_LEN];
        const char *target = result->expanded_target;
        DWORD creation_flags = 0;
        HANDLE hConIn = INVALID_HANDLE_VALUE;
        HANDLE hConOut = INVALID_HANDLE_VALUE;
        BOOL inherit_handles = FALSE;
        HANDLE child_proc = NULL;

        /* .cmd/.bat files cannot be launched directly by CreateProcessA;
         * they must be run through cmd.exe /c */
        if (exec_needs_cmd_wrapper(target)) {
            log_write("JMP35", "EXEC: target is .cmd/.bat, wrapping with cmd.exe");
            sprintf_s(cmd_copy, sizeof(cmd_copy),
                      "cmd.exe /c %s", target);
        } else if (exec_needs_ps_wrapper(target)) {
            log_write("JMP36", "EXEC: target is .ps1, wrapping with pwsh");
            /* .ps1 scripts need PowerShell; try pwsh first, fall back to powershell */
            sprintf_s(cmd_copy, sizeof(cmd_copy),
                      "pwsh -NoProfile -ExecutionPolicy Bypass -File %s", target);
        } else if (exec_needs_vbs_wrapper(target)) {
            /* .vbs scripts need a Windows Script Host. Use wscript.exe when the
             * caller asked to hide the console (it is windowed by default and
             * never shows a console), otherwise cscript.exe so any WScript.Echo
             * output is visible in the current console. */
            const char *host = result->hide_console ? "wscript.exe" : "cscript.exe";
            log_write("JMP37", "EXEC: target is .vbs, wrapping with %s", host);
            sprintf_s(cmd_copy, sizeof(cmd_copy),
                      "%s //Nologo %s", host, target);
        } else {
            log_write("JMP38", "EXEC: direct launch");
            strcpy_s(cmd_copy, sizeof(cmd_copy), target);
        }

        log_write("JMP39", "EXEC: final command='%s', hide=%d",
                  cmd_copy, result->hide_console);

        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        memset(&pi, 0, sizeof(pi));

        if (result->hide_console) {
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            creation_flags |= CREATE_NO_WINDOW;
        } else {
            /* Open real console handles so the child gets proper
             * stdin/stdout even when ours are redirected (e.g. by
             * the for/f wrapper or PowerShell output capture). */
            SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
            hConIn = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ, &sa,
                                 OPEN_EXISTING, 0, NULL);
            hConOut = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_WRITE, &sa,
                                  OPEN_EXISTING, 0, NULL);
            if (hConIn != INVALID_HANDLE_VALUE &&
                hConOut != INVALID_HANDLE_VALUE) {
                si.dwFlags |= STARTF_USESTDHANDLES;
                si.hStdInput  = hConIn;
                si.hStdOutput = hConOut;
                si.hStdError  = hConOut;
                inherit_handles = TRUE;
                log_write("JMP39b", "EXEC: passing console handles to child");
            }
        }

        if (CreateProcessA(NULL, cmd_copy, NULL, NULL, inherit_handles,
                           creation_flags, NULL, NULL, &si, &pi)) {
            log_write("JMP40", "EXEC: CreateProcess succeeded");
            CloseHandle(pi.hThread);
            if (!result->hide_console)
                child_proc = pi.hProcess;
            else
                CloseHandle(pi.hProcess);
        } else if (exec_needs_ps_wrapper(target)) {
            log_write("JMP41", "EXEC: pwsh failed (err=%lu), falling back to powershell.exe",
                      GetLastError());
            /* pwsh not found, fall back to powershell.exe */
            sprintf_s(cmd_copy, sizeof(cmd_copy),
                      "powershell -NoProfile -ExecutionPolicy Bypass -File %s", target);
            if (CreateProcessA(NULL, cmd_copy, NULL, NULL, inherit_handles,
                               creation_flags, NULL, NULL, &si, &pi)) {
                log_write("JMP42", "EXEC: powershell.exe fallback succeeded");
                CloseHandle(pi.hThread);
                if (!result->hide_console)
                    child_proc = pi.hProcess;
                else
                    CloseHandle(pi.hProcess);
            } else {
                log_write("JMP43", "EXEC: powershell.exe fallback also failed (err=%lu)",
                          GetLastError());
            }
        } else {
            log_write("JMP44", "EXEC: CreateProcess failed (err=%lu)", GetLastError());
        }

        /* Close our copies of the console handles; child has its own */
        if (hConIn != INVALID_HANDLE_VALUE) CloseHandle(hConIn);
        if (hConOut != INVALID_HANDLE_VALUE) CloseHandle(hConOut);

        write_temp_cmd("@rem\n");
        if (child_proc) return child_proc;
        break;
    }
    }
    return NULL;
}

int jump_main(int argc, char *argv[]) {
    int exit_code;

    error_init();

    /* --log mode: set flag and exit immediately.
     * Does NOT read config or cache — safe for debugging first-run issues. */
    if (argc >= 2 && _stricmp(argv[1], "--log") == 0) {
        if (log_set_flag() == 0)
            fprintf(stderr, "Logging enabled. The next command will write to %%TEMP%%\\jump_YYYYMMDD_HHMMSS.log\n");
        else
            fprintf(stderr, "Failed to enable logging.\n");
        return J_EXIT_OK;
    }

    /* Initialize logging (checks for flag file from a prior --log call) */
    log_init();

    /* Log the full command line */
    if (log_enabled()) {
        char cmdline[4096] = {0};
        int i;
        size_t cpos = 0;
        for (i = 0; i < argc && cpos < sizeof(cmdline) - 1; i++) {
            if (i > 0 && cpos < sizeof(cmdline) - 1) cmdline[cpos++] = ' ';
            cpos += snprintf(cmdline + cpos, sizeof(cmdline) - cpos, "%s", argv[i]);
        }
        log_write("JMP01", "Command line: %s", cmdline);
        log_write("JMP02", "Jump v%s, argc=%d", JUMP_VERSION, argc);
    }

    /* No arguments → help */
    if (argc < 2) {
        log_write("JMP03", "No arguments, printing usage");
        print_usage();
        exit_code = J_EXIT_OK;
        goto done;
    }

    /* --osd mode */
    if (_stricmp(argv[1], "--osd") == 0) {
        log_write("JMP04", "OSD mode requested");
        if (argc >= 3) {
            OsdIcon icon = OSD_ICON_CD;
            if (argc >= 4) {
                if (_stricmp(argv[3], "open") == 0) icon = OSD_ICON_OPEN;
                else if (_stricmp(argv[3], "exec") == 0) icon = OSD_ICON_EXEC;
            }
            log_write("JMP05", "Showing OSD: text='%s'", argv[2]);
            osd_show(argv[2], icon);
        }
        exit_code = J_EXIT_OK;
        goto done;
    }

    /* --install mode */
    if (_stricmp(argv[1], "--install") == 0) {
        const char *panel = NULL;
        int i;
        log_write("JMP06", "Install mode requested");
        for (i = 2; i < argc; i++) {
            if (_strnicmp(argv[i], "--tc-panel=", 11) == 0) {
                panel = argv[i] + 11;
            }
        }
        log_write("JMP07", "Calling jump_install(panel=%s)", panel ? panel : "NULL");
        exit_code = jump_install(panel);
        log_write("JMP08", "jump_install returned %d", exit_code);
        goto done;
    }

    /* --uninstall mode */
    if (_stricmp(argv[1], "--uninstall") == 0) {
        log_write("JMP09", "Uninstall mode requested");
        exit_code = jump_uninstall();
        log_write("JMP10", "jump_uninstall returned %d", exit_code);
        goto done;
    }

    /* --version mode */
    if (_stricmp(argv[1], "--version") == 0) {
        log_write("JMP15", "Version mode: v%s", JUMP_VERSION);
        fprintf(stderr, "Jump v%s\n", JUMP_VERSION);
        exit_code = J_EXIT_OK;
        goto done;
    }

    /* --update mode */
    if (_stricmp(argv[1], "--update") == 0) {
        log_write("JMP16", "Update mode requested");
        exit_code = jump_update();
        log_write("JMP17", "jump_update returned %d", exit_code);
        goto done;
    }

    /* --- All modes below require configuration (INI files) --- */

    /* --list mode */
    if (_stricmp(argv[1], "--list") == 0) {
        JumpConfig *cfg = (JumpConfig *)calloc(1, sizeof(JumpConfig));
        int rc;
        log_write("JMP11", "List mode requested");
        if (!cfg) {
            log_write("JMP12", "Failed to allocate JumpConfig for --list");
            exit_code = J_EXIT_RUNTIME_ERROR;
            goto done;
        }
        rc = config_load(cfg);
        if (rc != 0) {
            log_write("JMP13", "config_load failed with %d in --list mode", rc);
            free(cfg);
            exit_code = rc;
            goto done;
        }
        log_write("JMP14", "Listing %d shortcuts", cfg->shortcut_count);
        print_list(cfg);
        free(cfg);
        exit_code = J_EXIT_OK;
        goto done;
    }

    /* Normal mode: resolve alias */
    {
        JumpConfig *cfg = (JumpConfig *)calloc(1, sizeof(JumpConfig));
        ResolveResult result;
        int rc;
        int alias_words;

        if (!cfg) {
            log_write("JMP18", "Failed to allocate JumpConfig");
            exit_code = J_EXIT_RUNTIME_ERROR;
            goto done;
        }
        log_write("JMP19", "Loading configuration");
        rc = config_load(cfg);
        if (rc != 0) {
            log_write("JMP20", "config_load failed with %d", rc);
            free(cfg);
            exit_code = rc;
            goto done;
        }
        log_write("JMP21", "Config loaded: %d shortcuts, %d constants",
                  cfg->shortcut_count, cfg->constant_count);

        /* Try multi-word aliases: longest match first.
         * For "j my project file.txt", tries:
         *   "my project file.txt" (0 params)
         *   "my project"          (params: file.txt)
         *   "my"                  (params: project, file.txt)
         */
        rc = J_EXIT_NOT_FOUND;
        for (alias_words = argc - 1; alias_words >= 1; alias_words--) {
            char multi_alias[MAX_ALIAS_LEN];
            size_t pos = 0;
            int w;
            int param_start, param_count;

            for (w = 1; w <= alias_words; w++) {
                size_t wlen = strlen(argv[w]);
                if (w > 1) {
                    if (pos >= MAX_ALIAS_LEN - 1) break;
                    multi_alias[pos++] = ' ';
                }
                if (pos + wlen >= MAX_ALIAS_LEN) break;
                memcpy(multi_alias + pos, argv[w], wlen);
                pos += wlen;
            }
            multi_alias[pos] = '\0';

            param_start = 1 + alias_words;
            param_count = argc - param_start;
            log_write("JMP22", "Trying alias '%s' (words=%d, params=%d)",
                      multi_alias, alias_words, param_count);
            rc = resolve_alias(cfg, multi_alias,
                               param_count,
                               (const char **)(param_count > 0 ? &argv[param_start] : NULL),
                               &result);
            if (rc == 0) {
                log_write("JMP23", "Alias '%s' resolved: type=%d, target='%s'",
                          multi_alias, result.type, result.expanded_target);
                break;
            }
        }

        if (rc == J_EXIT_NOT_FOUND) {
            Suggestion suggestions[MAX_SUGGESTIONS];
            int count;
            char error_buf[2048];
            int pos = 0;

            log_write("JMP24", "Alias '%s' not found", argv[1]);
            pos += snprintf(error_buf + pos, sizeof(error_buf) - pos,
                            "Unknown alias '%s'\n", argv[1]);
            count = suggest_aliases(cfg, argv[1], suggestions, MAX_SUGGESTIONS);
            if (count > 0) {
                int s;
                log_write("JMP25", "Found %d suggestions", count);
                pos += snprintf(error_buf + pos, sizeof(error_buf) - pos,
                                "Did you mean:\n");
                for (s = 0; s < count; s++) {
                    if (suggestions[s].label[0])
                        pos += snprintf(error_buf + pos, sizeof(error_buf) - pos,
                                        "  %s  - %s\n", suggestions[s].alias,
                                        suggestions[s].label);
                    else
                        pos += snprintf(error_buf + pos, sizeof(error_buf) - pos,
                                        "  %s\n", suggestions[s].alias);
                }
            }
            error_report("%s", error_buf);
            write_temp_cmd("@rem\n");
            free(cfg);
            exit_code = J_EXIT_NOT_FOUND;
            goto done;
        }

        log_write("JMP26", "Performing action for label='%s'", result.label);
        {
            HANDLE exec_proc = perform_action(&result);
            spawn_osd(result.label, result.type);
            if (exec_proc) {
                log_write("JMP27", "Waiting for EXEC child process");
                WaitForSingleObject(exec_proc, INFINITE);
                CloseHandle(exec_proc);
            }
        }
        free(cfg);
        exit_code = J_EXIT_OK;
        goto done;
    }

done:
    log_write("JMP99", "Exiting with code %d", exit_code);
    if (log_enabled()) {
        const wchar_t *log_path = log_get_path();
        if (log_path) {
            char log_path_a[MAX_PATH];
            WideCharToMultiByte(CP_ACP, 0, log_path, -1,
                                log_path_a, MAX_PATH, NULL, NULL);
            fprintf(stderr, "Log written to: %s\n", log_path_a);
            spawn_osd(log_path_a, SHORTCUT_CD);

            /* Copy log file path to clipboard */
            if (OpenClipboard(NULL)) {
                size_t len = strlen(log_path_a);
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
                if (hMem) {
                    char *p = (char *)GlobalLock(hMem);
                    memcpy(p, log_path_a, len + 1);
                    GlobalUnlock(hMem);
                    EmptyClipboard();
                    SetClipboardData(CF_TEXT, hMem);
                }
                CloseClipboard();
            }
        }
    }
    log_close();
    return exit_code;
}
