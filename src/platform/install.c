#include "install.h"
#include "tc.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <shlobj.h>

#define DOSKEY_CMD "doskey j=jc.exe $* $T call %TEMP%\\jump_cd.cmd"

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

    if (rc == ERROR_SUCCESS && stristr(existing, DOSKEY_CMD)) {
        fprintf(stderr, "[install] DOSKEY macro already in AutoRun, skipping.\n");
        RegCloseKey(hkey);
        return 0;
    }

    char newval[4096];
    if (rc == ERROR_SUCCESS && existing[0] != '\0') {
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

static int setup_ps_profile(const char *tc_path, const char *panel) {
    char profile_path[MAX_PATH];
    if (ExpandEnvironmentStringsA(
            "%USERPROFILE%\\Documents\\WindowsPowerShell\\Microsoft.PowerShell_profile.ps1",
            profile_path, MAX_PATH) == 0) {
        fprintf(stderr, "[install] Failed to expand profile path.\n");
        return 1;
    }

    /* Build the PS block */
    char block[2048];
    if (tc_path && tc_path[0]) {
        snprintf(block, sizeof(block),
                 "%s\n"
                 "function j {\n"
                 "    $path = & jc.exe @args\n"
                 "    if ($path) {\n"
                 "        & \"%s\" /O /S /%s=\"$path\"\n"
                 "        Set-Location $path\n"
                 "    }\n"
                 "}\n"
                 "%s\n",
                 PS_MARKER_BEGIN, tc_path, panel, PS_MARKER_END);
    } else {
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
    }

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

    fprintf(stderr, "[install] PowerShell profile updated: %s\n", profile_path);
    return 0;
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

    /* Find and remove our doskey command */
    const char *pos = stristr(existing, DOSKEY_CMD);
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
    const char *after = pos + strlen(DOSKEY_CMD);

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

static int remove_ps_profile(void) {
    char profile_path[MAX_PATH];
    if (ExpandEnvironmentStringsA(
            "%USERPROFILE%\\Documents\\WindowsPowerShell\\Microsoft.PowerShell_profile.ps1",
            profile_path, MAX_PATH) == 0) {
        return 1;
    }

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

    fprintf(stderr, "[uninstall] PowerShell profile cleaned.\n");
    return 0;
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

    /* Step 3: DOSKEY AutoRun */
    errors += setup_autorun();

    /* Step 4: PowerShell profile */
    errors += setup_ps_profile(tc_found ? tc_path : NULL, panel);

    /* Step 5: Add to PATH */
    char exe_dir[MAX_PATH];
    if (get_exe_dir(exe_dir, sizeof(exe_dir)) == 0) {
        errors += add_to_path(exe_dir);
    } else {
        fprintf(stderr, "[install] Failed to determine exe directory.\n");
        errors++;
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

    /* Step 2: Remove PS profile block */
    errors += remove_ps_profile();

    /* Step 3: Remove from PATH */
    char exe_dir[MAX_PATH];
    if (get_exe_dir(exe_dir, sizeof(exe_dir)) == 0) {
        errors += remove_from_path(exe_dir);
    }

    /* Step 4: Broadcast */
    broadcast_env_change();

    fprintf(stderr, "[uninstall] Uninstallation %s.\n",
            errors ? "completed with errors" : "complete");
    return errors ? 1 : 0;
}
