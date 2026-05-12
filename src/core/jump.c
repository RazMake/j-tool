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

/* Execute the resolved shortcut action (CD, OPEN, or EXEC) */
static void perform_action(const ResolveResult *result) {
    switch (result->type) {
    case SHORTCUT_CD: {
        /* Navigate TC panel only if launched from TC */
        if (tc_is_ancestor())
            tc_navigate(result->expanded_target);
        /* Write path to stdout for PowerShell wrapper */
        HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hStdout && hStdout != INVALID_HANDLE_VALUE) {
            DWORD written;
            WriteFile(hStdout, result->expanded_target,
                      (DWORD)strlen(result->expanded_target), &written, NULL);
            WriteFile(hStdout, "\n", 1, &written, NULL);
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
        ShellExecuteA(NULL, "open", result->expanded_target,
                      NULL, NULL, SW_SHOWNORMAL);
        write_temp_cmd("@rem\n");
        break;

    case SHORTCUT_EXEC: {
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        char cmd_copy[MAX_PATH_LEN];
        const char *target = result->expanded_target;

        /* .cmd/.bat files cannot be launched directly by CreateProcessA;
         * they must be run through cmd.exe /c */
        if (exec_needs_cmd_wrapper(target)) {
            sprintf_s(cmd_copy, sizeof(cmd_copy),
                      "cmd.exe /c %s", target);
        } else if (exec_needs_ps_wrapper(target)) {
            /* .ps1 scripts need PowerShell; try pwsh first, fall back to powershell */
            sprintf_s(cmd_copy, sizeof(cmd_copy),
                      "pwsh -NoProfile -ExecutionPolicy Bypass -File %s", target);
        } else {
            strcpy_s(cmd_copy, sizeof(cmd_copy), target);
        }

        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        memset(&pi, 0, sizeof(pi));

        if (CreateProcessA(NULL, cmd_copy, NULL, NULL, FALSE,
                           0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        } else if (exec_needs_ps_wrapper(target)) {
            /* pwsh not found, fall back to powershell.exe */
            sprintf_s(cmd_copy, sizeof(cmd_copy),
                      "powershell -NoProfile -ExecutionPolicy Bypass -File %s", target);
            if (CreateProcessA(NULL, cmd_copy, NULL, NULL, FALSE,
                               0, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            }
        }
        write_temp_cmd("@rem\n");
        break;
    }
    }
}

int jump_main(int argc, char *argv[]) {
    error_init();

    /* No arguments → help */
    if (argc < 2) {
        print_usage();
        return J_EXIT_OK;
    }

    /* --osd mode */
    if (_stricmp(argv[1], "--osd") == 0) {
        if (argc >= 3) {
            OsdIcon icon = OSD_ICON_CD;
            if (argc >= 4) {
                if (_stricmp(argv[3], "open") == 0) icon = OSD_ICON_OPEN;
                else if (_stricmp(argv[3], "exec") == 0) icon = OSD_ICON_EXEC;
            }
            osd_show(argv[2], icon);
        }
        return J_EXIT_OK;
    }

    /* --install mode */
    if (_stricmp(argv[1], "--install") == 0) {
        const char *panel = NULL;
        int i;
        for (i = 2; i < argc; i++) {
            if (_strnicmp(argv[i], "--tc-panel=", 11) == 0) {
                panel = argv[i] + 11;
            }
        }
        return jump_install(panel);
    }

    /* --uninstall mode */
    if (_stricmp(argv[1], "--uninstall") == 0) {
        return jump_uninstall();
    }

    /* --list mode */
    if (_stricmp(argv[1], "--list") == 0) {
        JumpConfig *cfg = (JumpConfig *)calloc(1, sizeof(JumpConfig));
        int rc;
        if (!cfg) return J_EXIT_RUNTIME_ERROR;
        rc = config_load(cfg);
        if (rc != 0) { free(cfg); return rc; }
        print_list(cfg);
        free(cfg);
        return J_EXIT_OK;
    }

    /* --version mode */
    if (_stricmp(argv[1], "--version") == 0) {
        fprintf(stderr, "Jump v%s\n", JUMP_VERSION);
        return J_EXIT_OK;
    }

    /* --update mode */
    if (_stricmp(argv[1], "--update") == 0) {
        return jump_update();
    }

    /* Normal mode: resolve alias */
    {
        JumpConfig *cfg = (JumpConfig *)calloc(1, sizeof(JumpConfig));
        ResolveResult result;
        int rc;
        int alias_words;

        if (!cfg) return J_EXIT_RUNTIME_ERROR;
        rc = config_load(cfg);
        if (rc != 0) { free(cfg); return rc; }

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
            rc = resolve_alias(cfg, multi_alias,
                               param_count,
                               (const char **)(param_count > 0 ? &argv[param_start] : NULL),
                               &result);
            if (rc == 0) break;
        }

        if (rc == J_EXIT_NOT_FOUND) {
            Suggestion suggestions[MAX_SUGGESTIONS];
            int count;
            char error_buf[2048];
            int pos = 0;

            pos += snprintf(error_buf + pos, sizeof(error_buf) - pos,
                            "Unknown alias '%s'\n", argv[1]);
            count = suggest_aliases(cfg, argv[1], suggestions, MAX_SUGGESTIONS);
            if (count > 0) {
                int s;
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
            return J_EXIT_NOT_FOUND;
        }

        perform_action(&result);
        spawn_osd(result.label, result.type);
        free(cfg);
        return J_EXIT_OK;
    }
}
