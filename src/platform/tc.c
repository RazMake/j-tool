/* Total Commander integration — detects TC installation from the
 * registry, navigates its panels via command-line IPC, and checks
 * whether the current process was launched from within TC. */
#include "tc.h"
#include <windows.h>
#include <tlhelp32.h>
#include <string.h>
#include <stdio.h>

/* Try to read TC install dir from a single registry hive+key. */
static int tc_find_path_in_hive(HKEY root, char *tc_path, size_t tc_path_size) {
    HKEY hkey;
    LONG rc;
    char install_dir[MAX_PATH];
    DWORD dir_size = sizeof(install_dir);
    DWORD type;
    char full_path[MAX_PATH];

    rc = RegOpenKeyExA(root,
                       "Software\\Ghisler\\Total Commander",
                       0, KEY_READ | KEY_WOW64_64KEY, &hkey);
    if (rc != ERROR_SUCCESS)
        return -1;

    rc = RegQueryValueExA(hkey, "InstallDir", NULL, &type,
                          (LPBYTE)install_dir, &dir_size);
    RegCloseKey(hkey);
    if (rc != ERROR_SUCCESS || type != REG_SZ)
        return -1;

    /* Remove trailing null counted by RegQueryValueExA */
    if (dir_size > 0 && install_dir[dir_size - 1] == '\0')
        dir_size--;

    /* Try 64-bit executable first */
    snprintf(full_path, sizeof(full_path), "%s\\TOTALCMD64.EXE", install_dir);
    if (GetFileAttributesA(full_path) != INVALID_FILE_ATTRIBUTES) {
        if (strlen(full_path) + 1 > tc_path_size)
            return -1;
        strncpy_s(tc_path, tc_path_size, full_path, _TRUNCATE);
        return 0;
    }

    /* Fall back to 32-bit executable */
    snprintf(full_path, sizeof(full_path), "%s\\TOTALCMD.EXE", install_dir);
    if (GetFileAttributesA(full_path) != INVALID_FILE_ATTRIBUTES) {
        if (strlen(full_path) + 1 > tc_path_size)
            return -1;
        strncpy_s(tc_path, tc_path_size, full_path, _TRUNCATE);
        return 0;
    }

    return -1;
}

int tc_find_path(char *tc_path, size_t tc_path_size) {
    /* Try machine-wide install first (HKLM), then per-user (HKCU) */
    if (tc_find_path_in_hive(HKEY_LOCAL_MACHINE, tc_path, tc_path_size) == 0)
        return 0;
    return tc_find_path_in_hive(HKEY_CURRENT_USER, tc_path, tc_path_size);
}

int tc_build_cd_command(const char *tc_path, const char *panel,
                        const char *directory, char *cmd_buf, size_t cmd_size) {
    const char *side = (panel[0] == 'R') ? "/R=" : "/L=";
    int n = snprintf(cmd_buf, cmd_size, "\"%s\" /O /S %s\"%s\"",
                     tc_path, side, directory);
    if (n < 0 || (size_t)n >= cmd_size)
        return -1;
    return 0;
}

int tc_navigate(const char *directory) {
    char tc_path[MAX_PATH];
    char cmd_buf[MAX_PATH + MAX_PATH + 64];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    /* Only act if TC is already running */
    if (!FindWindowA("TTOTAL_CMD", NULL))
        return -1;

    /* Try registry first, then fall back to ancestor process path */
    if (tc_find_path(tc_path, sizeof(tc_path)) != 0 &&
        tc_find_ancestor_path(tc_path, sizeof(tc_path)) != 0)
        return -1;

    /* Navigate the source (active) panel */
    if (tc_build_cd_command(tc_path, "L", directory,
                            cmd_buf, sizeof(cmd_buf)) != 0)
        return -1;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    if (!CreateProcessA(NULL, cmd_buf, NULL, NULL, FALSE,
                        0, NULL, NULL, &si, &pi))
        return -1;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}

/* Walk the process tree via toolhelp snapshot to find pid's parent. */
static DWORD get_parent_pid(DWORD pid) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe;

    if (snap == INVALID_HANDLE_VALUE) return 0;

    pe.dwSize = sizeof(pe);
    if (Process32First(snap, &pe)) {
        do {
            if (pe.th32ProcessID == pid) {
                CloseHandle(snap);
                return pe.th32ParentProcessID;
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return 0;
}

int tc_find_ancestor_path(char *tc_path, size_t tc_path_size) {
    DWORD pid = GetCurrentProcessId();
    int i;

    for (i = 0; i < 10; i++) {
        char name[MAX_PATH];
        DWORD name_size = MAX_PATH;
        HANDLE proc;
        char *base;

        pid = get_parent_pid(pid);
        if (pid == 0) return -1;

        proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!proc) continue;

        if (!QueryFullProcessImageNameA(proc, 0, name, &name_size)) {
            CloseHandle(proc);
            continue;
        }
        CloseHandle(proc);

        base = strrchr(name, '\\');
        base = base ? base + 1 : name;
        if (_stricmp(base, "TOTALCMD64.EXE") == 0 ||
            _stricmp(base, "TOTALCMD.EXE") == 0) {
            if (strlen(name) + 1 > tc_path_size)
                return -1;
            strncpy_s(tc_path, tc_path_size, name, _TRUNCATE);
            return 0;
        }
    }
    return -1;
}

int tc_is_ancestor(void) {
    char tc_path[MAX_PATH];
    return tc_find_ancestor_path(tc_path, sizeof(tc_path)) == 0 ? 1 : 0;
}
