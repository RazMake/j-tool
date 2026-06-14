/* Shell integration installer/uninstaller — sets up the `j` command
 * across CMD (via DOSKEY AutoRun), PowerShell (via $PROFILE function),
 * adds the exe directory to PATH, and manages Total Commander detection.
 * All operations are idempotent and print progress to stderr. */
#include "install.h"
#include "tc.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <shlobj.h>

#define DOSKEY_CMD     "doskey j=@call jump_j.cmd $*"
#define DOSKEY_CMD_OLD "doskey j=jc.exe $* >nul $T call %TEMP%\\jump_cd.cmd"

#define WRAPPER_FILENAME "jump_j.cmd"
#define WRAPPER_CONTENT  "@jc.exe %* >nul && @call %TEMP%\\jump_cd.cmd\r\n"

#define CMD_PROC_KEY "Software\\Microsoft\\Command Processor"
#define ENV_KEY      "Environment"

#define PS_MARKER_BEGIN "# BEGIN JUMP"
#define PS_MARKER_END   "# END JUMP"

/* ---------- helpers ---------------------------------------------------- */

/* Get directory of the running executable (no trailing backslash). */
static int get_exe_dir(char *buf, size_t buf_size) {
    DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)buf_size);
    if (n == 0 || n >= buf_size) return 1;
    /* strip filename */
    char *last = strrchr(buf, '\\');
    if (last) *last = '\0';
    return 0;
}

/*
 * Build a PowerShell profile path using the real Documents folder
 * (respects OneDrive/folder redirection).
 *   subfolder: "PowerShell" or "WindowsPowerShell"
 */
static int get_ps_profile_path(const char *subfolder,
                               char *buf, size_t buf_size) {
    char docs[MAX_PATH];
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, docs)))
        return -1;
    int n = snprintf(buf, buf_size, "%s\\%s\\Microsoft.PowerShell_profile.ps1",
                     docs, subfolder);
    if (n < 0 || (size_t)n >= buf_size)
        return -1;
    return 0;
}

/* Build the Git Bash startup file path (~/.bashrc) from %USERPROFILE%. */
static int get_bashrc_path(char *buf, size_t buf_size) {
    char home[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("USERPROFILE", home, sizeof(home));
    if (n == 0 || n >= sizeof(home)) return -1;
    int r = snprintf(buf, buf_size, "%s\\.bashrc", home);
    if (r < 0 || (size_t)r >= buf_size) return -1;
    return 0;
}

/* Case-insensitive substring search. */
static const char *stristr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    size_t nlen = strlen(needle);
    if (nlen == 0) return haystack;
    for (; *haystack; haystack++) {
        if (_strnicmp(haystack, needle, nlen) == 0)
            return haystack;
    }
    return NULL;
}

/* Create all directories in a path (like mkdir -p). */
static void ensure_parent_dirs(const char *filepath) {
    char tmp[MAX_PATH];
    strncpy_s(tmp, MAX_PATH, filepath, _TRUNCATE);
    /* strip filename */
    char *last = strrchr(tmp, '\\');
    if (!last) return;
    *last = '\0';
    /* walk forward creating dirs */
    for (char *p = tmp + 3; *p; p++) { /* skip "C:\" */
        if (*p == '\\') {
            *p = '\0';
            CreateDirectoryA(tmp, NULL);
            *p = '\\';
        }
    }
    CreateDirectoryA(tmp, NULL);
}

/* ---------- Step 1: JUMPS env var -------------------------------------- */

static void check_jumps_env(void) {
    HKEY hkey;
    char val[2048];
    DWORD val_size = sizeof(val);
    DWORD type;

    LONG rc = RegOpenKeyExA(HKEY_CURRENT_USER, ENV_KEY, 0, KEY_READ, &hkey);
    if (rc == ERROR_SUCCESS) {
        rc = RegQueryValueExA(hkey, "JUMPS", NULL, &type, (BYTE *)val, &val_size);
        RegCloseKey(hkey);
        if (rc == ERROR_SUCCESS && val_size > 1) {
            fprintf(stderr, "[install] JUMPS env var is set: %s\n", val);
            return;
        }
    }
    fprintf(stderr,
            "[install] JUMPS env var is not set.\n"
            "  Please set it manually to the path of your root config INI file:\n"
            "    setx JUMPS \"C:\\path\\to\\your\\root.ini\"\n"
            "  Then re-run jc --install.\n");
}

/* ---------- Step 2b: wrapper batch file -------------------------------- */

static int write_wrapper_cmd(const char *exe_dir) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s\\%s", exe_dir, WRAPPER_FILENAME);

    HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[install] Failed to write wrapper: %s\n", path);
        return 1;
    }
    DWORD written;
    WriteFile(hFile, WRAPPER_CONTENT,
             (DWORD)strlen(WRAPPER_CONTENT), &written, NULL);
    CloseHandle(hFile);
    fprintf(stderr, "[install] Wrapper script written: %s\n", path);
    return 0;
}

static void delete_wrapper_cmd(const char *exe_dir) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s\\%s", exe_dir, WRAPPER_FILENAME);
    if (DeleteFileA(path))
        fprintf(stderr, "[uninstall] Wrapper script removed: %s\n", path);
}

/* ---------- Step 3: DOSKEY AutoRun ------------------------------------- */

static int setup_autorun(void) {
    HKEY hkey;
    LONG rc = RegCreateKeyExA(HKEY_CURRENT_USER, CMD_PROC_KEY, 0, NULL,
                              0, KEY_READ | KEY_WRITE, NULL, &hkey, NULL);
    if (rc != ERROR_SUCCESS) {
        fprintf(stderr, "[install] Failed to open Command Processor key (%ld)\n", rc);
        return 1;
    }

    char existing[4096] = {0};
    DWORD exist_size = sizeof(existing) - 1;
    DWORD type = REG_SZ;
    rc = RegQueryValueExA(hkey, "AutoRun", NULL, &type,
                          (BYTE *)existing, &exist_size);

    /* Remove old $T-based macro if present */
    const char *old_pos = stristr(existing, DOSKEY_CMD_OLD);
    if (old_pos) {
        char cleaned[4096] = {0};
        size_t before_len = (size_t)(old_pos - existing);
        if (before_len > 0)
            memcpy(cleaned, existing, before_len);
        const char *after = old_pos + strlen(DOSKEY_CMD_OLD);
        size_t cl = strlen(cleaned);
        while (cl >= 3 && cleaned[cl-1]==' ' && cleaned[cl-2]=='&' && cleaned[cl-3]==' ') {
            cl -= 3; cleaned[cl] = '\0';
        }
        if (strncmp(after, " & ", 3) == 0) after += 3;
        if (*after) {
            if (cleaned[0]) strcat_s(cleaned, sizeof(cleaned), " & ");
            strcat_s(cleaned, sizeof(cleaned), after);
        }
        strcpy_s(existing, sizeof(existing), cleaned);
        exist_size = (DWORD)strlen(existing);
        fprintf(stderr, "[install] Removed old $T-based DOSKEY macro.\n");
    }

    if (stristr(existing, DOSKEY_CMD)) {
        fprintf(stderr, "[install] DOSKEY macro already in AutoRun, skipping.\n");
        RegCloseKey(hkey);
        return 0;
    }

    char newval[4096];
    if (existing[0] != '\0') {
        snprintf(newval, sizeof(newval), "%s & %s", existing, DOSKEY_CMD);
    } else {
        snprintf(newval, sizeof(newval), "%s", DOSKEY_CMD);
    }

    rc = RegSetValueExA(hkey, "AutoRun", 0, REG_SZ,
                        (const BYTE *)newval, (DWORD)(strlen(newval) + 1));
    RegCloseKey(hkey);
    if (rc != ERROR_SUCCESS) {
        fprintf(stderr, "[install] Failed to write AutoRun (%ld)\n", rc);
        return 1;
    }
    fprintf(stderr, "[install] DOSKEY macro added to AutoRun.\n");
    return 0;
}

/* ---------- Step 4: PowerShell profile --------------------------------- */

static int write_profile_block(const char *profile_path, const char *block,
                               const char *label) {
    /* Read existing profile if any */
    char *content = NULL;
    DWORD content_len = 0;
    HANDLE hf = CreateFileA(profile_path, GENERIC_READ, FILE_SHARE_READ,
                            NULL, OPEN_EXISTING, 0, NULL);
    if (hf != INVALID_HANDLE_VALUE) {
        content_len = GetFileSize(hf, NULL);
        if (content_len > 0 && content_len != INVALID_FILE_SIZE) {
            content = (char *)malloc(content_len + 1);
            if (content) {
                DWORD read_bytes;
                ReadFile(hf, content, content_len, &read_bytes, NULL);
                content[read_bytes] = '\0';
                content_len = read_bytes;
            }
        }
        CloseHandle(hf);
    }

    /* Build new content */
    char *new_content = NULL;
    if (content && content_len > 0) {
        char *begin = strstr(content, PS_MARKER_BEGIN);
        char *end = begin ? strstr(begin, PS_MARKER_END) : NULL;
        if (begin && end) {
            /* Replace existing block */
            end += strlen(PS_MARKER_END);
            /* skip trailing newline */
            if (*end == '\r') end++;
            if (*end == '\n') end++;
            size_t before_len = (size_t)(begin - content);
            size_t after_len = strlen(end);
            size_t new_len = before_len + strlen(block) + after_len + 1;
            new_content = (char *)malloc(new_len);
            if (new_content) {
                memcpy(new_content, content, before_len);
                memcpy(new_content + before_len, block, strlen(block));
                memcpy(new_content + before_len + strlen(block), end, after_len);
                new_content[before_len + strlen(block) + after_len] = '\0';
            }
        } else {
            /* Append */
            size_t new_len = content_len + 2 + strlen(block) + 1;
            new_content = (char *)malloc(new_len);
            if (new_content) {
                memcpy(new_content, content, content_len);
                /* Ensure newline before block */
                size_t pos = content_len;
                if (content_len > 0 && content[content_len - 1] != '\n') {
                    new_content[pos++] = '\n';
                }
                memcpy(new_content + pos, block, strlen(block));
                pos += strlen(block);
                new_content[pos] = '\0';
            }
        }
    } else {
        new_content = _strdup(block);
    }
    free(content);

    if (!new_content) {
        fprintf(stderr, "[install] Memory allocation failed for PS profile.\n");
        return 1;
    }

    /* Create parent dirs and write */
    ensure_parent_dirs(profile_path);
    hf = CreateFileA(profile_path, GENERIC_WRITE, 0, NULL,
                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[install] Failed to write PS profile: %s\n", profile_path);
        free(new_content);
        return 1;
    }
    DWORD written;
    WriteFile(hf, new_content, (DWORD)strlen(new_content), &written, NULL);
    CloseHandle(hf);
    free(new_content);

    fprintf(stderr, "[install] %s updated: %s\n", label, profile_path);
    return 0;
}

static int setup_ps_profile(const char *tc_path, const char *panel) {
    /* Build the PS block — simple Set-Location wrapper.
     * TC navigation is handled via the temp cmd file for CMD users;
     * PowerShell users get a clean directory change without bringing
     * Total Commander to the foreground. */
    char block[2048];
    (void)tc_path;
    (void)panel;
    snprintf(block, sizeof(block),
             "%s\n"
             "function j {\n"
             "    $path = & jc.exe @args\n"
             "    if ($path) {\n"
             "        Set-Location $path\n"
             "    }\n"
             "}\n"
             "%s\n",
             PS_MARKER_BEGIN, PS_MARKER_END);

    int errors = 0;

    /* Windows PowerShell 5.x profile */
    {
        char profile_path[MAX_PATH];
        if (get_ps_profile_path("WindowsPowerShell", profile_path,
                                sizeof(profile_path)) == 0) {
            errors += write_profile_block(profile_path, block, "PowerShell profile");
        }
    }

    /* PowerShell 7+ (pwsh) profile */
    {
        char profile_path[MAX_PATH];
        if (get_ps_profile_path("PowerShell", profile_path,
                                sizeof(profile_path)) == 0) {
            errors += write_profile_block(profile_path, block, "PowerShell profile");
        }
    }

    return errors ? 1 : 0;
}

/* ---------- Step 4b: Git Bash profile ---------------------------------- */

static int setup_bash_profile(void) {
    char bashrc_path[MAX_PATH];
    if (get_bashrc_path(bashrc_path, sizeof(bashrc_path)) != 0) {
        fprintf(stderr, "[install] Could not determine Git Bash profile path.\n");
        return 1;
    }

    /* Bash `j` function: capture the CD path jc.exe prints on stdout and
     * change directory. cygpath converts the Windows path (C:\foo) into a
     * POSIX path (/c/foo) that bash's cd understands. Written with LF line
     * endings so Git Bash does not choke on CR characters. */
    char block[1024];
    snprintf(block, sizeof(block),
             "%s\n"
             "j() {\n"
             "    local __jump_target\n"
             "    __jump_target=\"$(jc.exe \"$@\")\"\n"
             "    if [ -n \"$__jump_target\" ]; then\n"
             "        cd \"$(cygpath -u \"$__jump_target\" 2>/dev/null || printf '%%s' \"$__jump_target\")\"\n"
             "    fi\n"
             "}\n"
             "%s\n",
             PS_MARKER_BEGIN, PS_MARKER_END);

    return write_profile_block(bashrc_path, block, "Git Bash profile");
}

/* ---------- Step 5: PATH ----------------------------------------------- */

static int add_to_path(const char *exe_dir) {
    HKEY hkey;
    LONG rc = RegOpenKeyExA(HKEY_CURRENT_USER, ENV_KEY, 0,
                            KEY_READ | KEY_WRITE, &hkey);
    if (rc != ERROR_SUCCESS) {
        fprintf(stderr, "[install] Failed to open Environment key (%ld)\n", rc);
        return 1;
    }

    char path_val[8192] = {0};
    DWORD path_size = sizeof(path_val) - 1;
    DWORD type = REG_EXPAND_SZ;
    rc = RegQueryValueExA(hkey, "Path", NULL, &type,
                          (BYTE *)path_val, &path_size);

    /* Check if already present */
    if (rc == ERROR_SUCCESS && stristr(path_val, exe_dir)) {
        fprintf(stderr, "[install] Directory already in PATH, skipping.\n");
        RegCloseKey(hkey);
        return 0;
    }

    /* Append */
    char new_path[8192];
    if (rc == ERROR_SUCCESS && path_val[0] != '\0') {
        /* Remove trailing semicolon if present */
        size_t len = strlen(path_val);
        if (len > 0 && path_val[len - 1] == ';')
            path_val[len - 1] = '\0';
        snprintf(new_path, sizeof(new_path), "%s;%s", path_val, exe_dir);
    } else {
        snprintf(new_path, sizeof(new_path), "%s", exe_dir);
    }

    rc = RegSetValueExA(hkey, "Path", 0, REG_EXPAND_SZ,
                        (const BYTE *)new_path, (DWORD)(strlen(new_path) + 1));
    RegCloseKey(hkey);
    if (rc != ERROR_SUCCESS) {
        fprintf(stderr, "[install] Failed to update PATH (%ld)\n", rc);
        return 1;
    }
    fprintf(stderr, "[install] Added to PATH: %s\n", exe_dir);
    return 0;
}

static int remove_from_path(const char *exe_dir) {
    HKEY hkey;
    LONG rc = RegOpenKeyExA(HKEY_CURRENT_USER, ENV_KEY, 0,
                            KEY_READ | KEY_WRITE, &hkey);
    if (rc != ERROR_SUCCESS) return 0; /* nothing to do */

    char path_val[8192] = {0};
    DWORD path_size = sizeof(path_val) - 1;
    DWORD type = REG_EXPAND_SZ;
    rc = RegQueryValueExA(hkey, "Path", NULL, &type,
                          (BYTE *)path_val, &path_size);
    if (rc != ERROR_SUCCESS) {
        RegCloseKey(hkey);
        return 0;
    }

    /* Rebuild PATH without our directory */
    char new_path[8192] = {0};
    char *ctx = NULL;
    char *token = strtok_s(path_val, ";", &ctx);
    int first = 1;
    while (token) {
        if (_stricmp(token, exe_dir) != 0) {
            if (!first) strcat_s(new_path, sizeof(new_path), ";");
            strcat_s(new_path, sizeof(new_path), token);
            first = 0;
        }
        token = strtok_s(NULL, ";", &ctx);
    }

    rc = RegSetValueExA(hkey, "Path", 0, REG_EXPAND_SZ,
                        (const BYTE *)new_path, (DWORD)(strlen(new_path) + 1));
    RegCloseKey(hkey);
    if (rc != ERROR_SUCCESS) {
        fprintf(stderr, "[uninstall] Failed to update PATH (%ld)\n", rc);
        return 1;
    }
    fprintf(stderr, "[uninstall] Removed from PATH: %s\n", exe_dir);
    return 0;
}

/* ---------- Broadcast -------------------------------------------------- */

static void broadcast_env_change(void) {
    DWORD_PTR result;
    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        (LPARAM)"Environment", SMTO_ABORTIFHUNG, 5000, &result);
}

/* ---------- Uninstall helpers ------------------------------------------ */

static int remove_autorun(void) {
    HKEY hkey;
    LONG rc = RegOpenKeyExA(HKEY_CURRENT_USER, CMD_PROC_KEY, 0,
                            KEY_READ | KEY_WRITE, &hkey);
    if (rc != ERROR_SUCCESS) return 0; /* key doesn't exist, nothing to remove */

    char existing[4096] = {0};
    DWORD exist_size = sizeof(existing) - 1;
    DWORD type = REG_SZ;
    rc = RegQueryValueExA(hkey, "AutoRun", NULL, &type,
                          (BYTE *)existing, &exist_size);
    if (rc != ERROR_SUCCESS) {
        RegCloseKey(hkey);
        return 0;
    }

    /* Find and remove our doskey command (try new format, then old) */
    const char *pos = stristr(existing, DOSKEY_CMD);
    size_t cmd_len = strlen(DOSKEY_CMD);
    if (!pos) {
        pos = stristr(existing, DOSKEY_CMD_OLD);
        cmd_len = strlen(DOSKEY_CMD_OLD);
    }
    if (!pos) {
        RegCloseKey(hkey);
        return 0;
    }

    char newval[4096] = {0};
    /* Copy part before our command */
    size_t before_len = (size_t)(pos - existing);
    if (before_len > 0) {
        memcpy(newval, existing, before_len);
    }

    /* Skip our command */
    const char *after = pos + cmd_len;

    /* Clean up separator: " & " before or after */
    /* Trim trailing " & " from before part */
    size_t nv_len = strlen(newval);
    while (nv_len >= 3 && newval[nv_len - 1] == ' '
           && newval[nv_len - 2] == '&' && newval[nv_len - 3] == ' ') {
        nv_len -= 3;
        newval[nv_len] = '\0';
    }

    /* Skip leading " & " from after part */
    if (strncmp(after, " & ", 3) == 0) {
        after += 3;
    }

    /* Append remaining */
    if (*after) {
        if (newval[0] != '\0') strcat_s(newval, sizeof(newval), " & ");
        strcat_s(newval, sizeof(newval), after);
    }

    if (newval[0] == '\0') {
        RegDeleteValueA(hkey, "AutoRun");
    } else {
        RegSetValueExA(hkey, "AutoRun", 0, REG_SZ,
                       (const BYTE *)newval, (DWORD)(strlen(newval) + 1));
    }
    RegCloseKey(hkey);
    fprintf(stderr, "[uninstall] DOSKEY macro removed from AutoRun.\n");
    return 0;
}

static int remove_profile_block_at(const char *profile_path, const char *label) {
    HANDLE hf = CreateFileA(profile_path, GENERIC_READ, FILE_SHARE_READ,
                            NULL, OPEN_EXISTING, 0, NULL);
    if (hf == INVALID_HANDLE_VALUE) return 0; /* file doesn't exist */

    DWORD file_size = GetFileSize(hf, NULL);
    if (file_size == 0 || file_size == INVALID_FILE_SIZE) {
        CloseHandle(hf);
        return 0;
    }

    char *content = (char *)malloc(file_size + 1);
    if (!content) { CloseHandle(hf); return 1; }
    DWORD read_bytes;
    ReadFile(hf, content, file_size, &read_bytes, NULL);
    content[read_bytes] = '\0';
    CloseHandle(hf);

    char *begin = strstr(content, PS_MARKER_BEGIN);
    char *end = begin ? strstr(begin, PS_MARKER_END) : NULL;
    if (!begin || !end) {
        free(content);
        return 0; /* markers not found */
    }

    end += strlen(PS_MARKER_END);
    if (*end == '\r') end++;
    if (*end == '\n') end++;

    size_t before_len = (size_t)(begin - content);
    size_t after_len = strlen(end);
    size_t new_len = before_len + after_len + 1;
    char *new_content = (char *)malloc(new_len);
    if (!new_content) { free(content); return 1; }

    memcpy(new_content, content, before_len);
    memcpy(new_content + before_len, end, after_len);
    new_content[before_len + after_len] = '\0';
    free(content);

    hf = CreateFileA(profile_path, GENERIC_WRITE, 0, NULL,
                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) {
        free(new_content);
        return 1;
    }
    DWORD written;
    WriteFile(hf, new_content, (DWORD)strlen(new_content), &written, NULL);
    CloseHandle(hf);
    free(new_content);

    fprintf(stderr, "[uninstall] %s cleaned.\n", label);
    return 0;
}

/* Remove the Jump function block from the Git Bash startup file. */
static int remove_bash_profile(void) {
    char bashrc_path[MAX_PATH];
    if (get_bashrc_path(bashrc_path, sizeof(bashrc_path)) != 0) return 0;
    return remove_profile_block_at(bashrc_path, "Git Bash profile");
}

/* ======================================================================= */

int jump_install(const char *tc_panel) {
    const char *panel = (tc_panel && tc_panel[0]) ? tc_panel : "L";
    int errors = 0;

    fprintf(stderr, "[install] Starting Jump installation...\n");

    /* Step 1: Check JUMPS env var */
    check_jumps_env();

    /* Step 2: Detect Total Commander */
    char tc_path[MAX_PATH] = {0};
    int tc_found = (tc_find_path(tc_path, sizeof(tc_path)) == 0);
    if (tc_found) {
        fprintf(stderr, "[install] Total Commander found: %s\n", tc_path);
    } else {
        fprintf(stderr, "[install] Total Commander not found (TC integration skipped).\n");
    }

    /* Step 2b: Write wrapper batch file next to jc.exe */
    char exe_dir[MAX_PATH];
    if (get_exe_dir(exe_dir, sizeof(exe_dir)) == 0) {
        errors += write_wrapper_cmd(exe_dir);
    } else {
        fprintf(stderr, "[install] Failed to determine exe directory.\n");
        errors++;
    }

    /* Step 3: DOSKEY AutoRun */
    errors += setup_autorun();

    /* Step 4: PowerShell profile */
    errors += setup_ps_profile(tc_found ? tc_path : NULL, panel);

    /* Step 4b: Git Bash profile (~/.bashrc) */
    errors += setup_bash_profile();

    /* Step 5: Add to PATH */
    if (exe_dir[0]) {
        errors += add_to_path(exe_dir);
    }

    /* Broadcast environment change */
    broadcast_env_change();

    fprintf(stderr, "[install] Installation %s.\n",
            errors ? "completed with errors" : "complete");
    return errors ? 1 : 0;
}

int jump_uninstall(void) {
    int errors = 0;

    fprintf(stderr, "[uninstall] Starting Jump uninstallation...\n");

    /* Step 1: Remove DOSKEY from AutoRun */
    errors += remove_autorun();

    /* Step 2: Remove PS profile block (both PS 5 and pwsh 7+) */
    {
        char profile_path[MAX_PATH];
        if (get_ps_profile_path("WindowsPowerShell", profile_path,
                                sizeof(profile_path)) == 0) {
            errors += remove_profile_block_at(profile_path, "PowerShell profile");
        }
        if (get_ps_profile_path("PowerShell", profile_path,
                                sizeof(profile_path)) == 0) {
            errors += remove_profile_block_at(profile_path, "PowerShell profile");
        }
    }

    /* Step 2c: Remove Git Bash profile block */
    errors += remove_bash_profile();

    /* Step 2b: Remove wrapper batch file */
    char exe_dir[MAX_PATH];
    if (get_exe_dir(exe_dir, sizeof(exe_dir)) == 0) {
        delete_wrapper_cmd(exe_dir);
    }

    /* Step 3: Remove from PATH */
    if (get_exe_dir(exe_dir, sizeof(exe_dir)) == 0) {
        errors += remove_from_path(exe_dir);
    }

    /* Step 4: Broadcast */
    broadcast_env_change();

    fprintf(stderr, "[uninstall] Uninstallation %s.\n",
            errors ? "completed with errors" : "complete");
    return errors ? 1 : 0;
}
