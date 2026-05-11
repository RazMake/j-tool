#include "tc.h"
#include <windows.h>
#include <string.h>
#include <stdio.h>

int tc_find_path(char *tc_path, size_t tc_path_size) {
    HKEY hkey;
    LONG rc;
    char install_dir[MAX_PATH];
    DWORD dir_size = sizeof(install_dir);
    DWORD type;
    char full_path[MAX_PATH];

    rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
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

int tc_build_cd_command(const char *tc_path, const char *panel,
                        const char *directory, char *cmd_buf, size_t cmd_size) {
    const char *side = (panel[0] == 'R') ? "/R=" : "/L=";
    int n = snprintf(cmd_buf, cmd_size, "\"%s\" /O /S %s\"%s\"",
                     tc_path, side, directory);
    if (n < 0 || (size_t)n >= cmd_size)
        return -1;
    return 0;
}
