#include "update.h"
#include "version.h"
#include "types.h"

#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "winhttp.lib")

/* ------------------------------------------------------------------ */
/*  Helpers: version parsing & comparison                              */
/* ------------------------------------------------------------------ */

static int parse_version(const char *str, int out[3])
{
    out[0] = out[1] = out[2] = 0;
    if (*str == 'v' || *str == 'V') str++;
    if (sscanf_s(str, "%d.%d.%d", &out[0], &out[1], &out[2]) < 2)
        return -1;
    return 0;
}

/* Returns >0 if a > b, 0 if equal, <0 if a < b */
static int compare_versions(const int a[3], const int b[3])
{
    if (a[0] != b[0]) return a[0] - b[0];
    if (a[1] != b[1]) return a[1] - b[1];
    return a[2] - b[2];
}

/* ------------------------------------------------------------------ */
/*  Helpers: minimal JSON string extraction                            */
/* ------------------------------------------------------------------ */

/*
 * Find "key":"value" in json and copy value into buf.
 * search_from lets the caller scope where to start looking.
 * Returns pointer past the closing quote, or NULL on failure.
 */
static const char *json_get_string(const char *json, const char *search_from,
                                   const char *key, char *buf, size_t buf_sz)
{
    char pattern[128];
    const char *p, *start, *end;
    size_t len;

    if (strlen(key) + 4 > sizeof(pattern)) return NULL;
    sprintf_s(pattern, sizeof(pattern), "\"%s\"", key);

    p = strstr(search_from, pattern);
    if (!p) return NULL;

    p += strlen(pattern);

    /* skip optional whitespace and colon */
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != ':') return NULL;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '"') return NULL;
    p++; /* past opening quote */

    start = p;
    /* find closing quote, handle escaped quotes */
    while (*p && !(*p == '"' && *(p - 1) != '\\')) p++;
    if (*p != '"') return NULL;
    end = p;

    len = (size_t)(end - start);
    if (len >= buf_sz) len = buf_sz - 1;
    memcpy(buf, start, len);
    buf[len] = '\0';

    (void)json;
    return end + 1;
}

/*
 * Find the "assets" array, then locate the asset whose "name" contains
 * the given name_contains and ends with suffix. For that asset extract
 * browser_download_url.  Returns 0 on success.
 */
static int find_asset_url(const char *json, const char *name_contains,
                          const char *suffix, char *url, size_t url_sz)
{
    const char *assets_start, *cursor;
    char name_buf[256];

    assets_start = strstr(json, "\"assets\"");
    if (!assets_start) return -1;
    assets_start = strchr(assets_start, '[');
    if (!assets_start) return -1;

    cursor = assets_start;

    while (*cursor) {
        const char *after;
        after = json_get_string(json, cursor, "name", name_buf, sizeof(name_buf));
        if (!after) break;

        /* Check if this asset matches our criteria */
        if (strstr(name_buf, name_contains) != NULL) {
            size_t nlen = strlen(name_buf);
            size_t slen = strlen(suffix);
            if (nlen >= slen && strcmp(name_buf + nlen - slen, suffix) == 0) {
                /* Found the right asset — now get its download URL */
                const char *url_after;
                url_after = json_get_string(json, after, "browser_download_url",
                                            url, url_sz);
                if (url_after) return 0;
                return -1;
            }
        }

        cursor = after;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/*  Helpers: WinHTTP GET request                                       */
/* ------------------------------------------------------------------ */

/*
 * Crack a URL string into wide-char host, path, and https flag.
 * Returns 0 on success.
 */
static int crack_url(const char *url, wchar_t *host, size_t host_sz,
                     wchar_t *path, size_t path_sz, int *use_ssl)
{
    wchar_t wide_url[2048];
    URL_COMPONENTS uc;

    MultiByteToWideChar(CP_UTF8, 0, url, -1, wide_url,
                        (int)(sizeof(wide_url) / sizeof(wide_url[0])));

    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = (DWORD)host_sz;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = (DWORD)path_sz;

    if (!WinHttpCrackUrl(wide_url, 0, 0, &uc)) return -1;

    *use_ssl = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    return 0;
}

/*
 * HTTP GET into a malloc'd buffer.  Caller frees *out_data.
 * Returns 0 on success.
 */
static int winhttp_get(const char *url, char **out_data, DWORD *out_size)
{
    wchar_t host[256], path[2048];
    int use_ssl = 0;
    HINTERNET session = NULL, connect = NULL, request = NULL;
    DWORD total = 0, cap = 0;
    char *buf = NULL;
    int ret = -1;

    *out_data = NULL;
    *out_size = 0;

    if (crack_url(url, host, 256, path, 2048, &use_ssl) != 0)
        return -1;

    session = WinHttpOpen(L"Jump-Updater/1.0",
                          WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                          WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) goto cleanup;

    connect = WinHttpConnect(session, host,
                             use_ssl ? INTERNET_DEFAULT_HTTPS_PORT
                                     : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!connect) goto cleanup;

    request = WinHttpOpenRequest(connect, L"GET", path, NULL,
                                 WINHTTP_NO_REFERER,
                                 WINHTTP_DEFAULT_ACCEPT_TYPES,
                                 use_ssl ? WINHTTP_FLAG_SECURE : 0);
    if (!request) goto cleanup;

    /* Enable automatic redirect following (up to 10) */
    {
        DWORD opt = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY,
                         &opt, sizeof(opt));
    }

    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS,
                            0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
        goto cleanup;

    if (!WinHttpReceiveResponse(request, NULL))
        goto cleanup;

    /* Check HTTP status */
    {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(request,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &status, &sz, WINHTTP_NO_HEADER_INDEX);
        if (status != 200) {
            fprintf(stderr, "  HTTP %lu\n", status);
            goto cleanup;
        }
    }

    /* Read response body */
    cap = 65536;
    buf = (char *)malloc(cap);
    if (!buf) goto cleanup;

    for (;;) {
        DWORD available = 0, read = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) goto cleanup;
        if (available == 0) break;

        if (total + available + 1 > cap) {
            while (total + available + 1 > cap) cap *= 2;
            buf = (char *)realloc(buf, cap);
            if (!buf) goto cleanup;
        }
        if (!WinHttpReadData(request, buf + total, available, &read))
            goto cleanup;
        total += read;
    }

    buf[total] = '\0';
    *out_data = buf;
    *out_size = total;
    buf = NULL; /* prevent free */
    ret = 0;

cleanup:
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
    free(buf);
    return ret;
}

/*
 * Download a URL to a file on disk.  Returns 0 on success.
 */
static int winhttp_download(const char *url, const char *dest_path)
{
    wchar_t host[256], path[2048];
    int use_ssl = 0;
    HINTERNET session = NULL, connect = NULL, request = NULL;
    HANDLE file = INVALID_HANDLE_VALUE;
    int ret = -1;

    if (crack_url(url, host, 256, path, 2048, &use_ssl) != 0)
        return -1;

    session = WinHttpOpen(L"Jump-Updater/1.0",
                          WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                          WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) goto cleanup;

    connect = WinHttpConnect(session, host,
                             use_ssl ? INTERNET_DEFAULT_HTTPS_PORT
                                     : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!connect) goto cleanup;

    request = WinHttpOpenRequest(connect, L"GET", path, NULL,
                                 WINHTTP_NO_REFERER,
                                 WINHTTP_DEFAULT_ACCEPT_TYPES,
                                 use_ssl ? WINHTTP_FLAG_SECURE : 0);
    if (!request) goto cleanup;

    {
        DWORD opt = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY,
                         &opt, sizeof(opt));
    }

    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS,
                            0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
        goto cleanup;

    if (!WinHttpReceiveResponse(request, NULL))
        goto cleanup;

    {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(request,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &status, &sz, WINHTTP_NO_HEADER_INDEX);
        if (status != 200) {
            fprintf(stderr, "  HTTP %lu\n", status);
            goto cleanup;
        }
    }

    file = CreateFileA(dest_path, GENERIC_WRITE, 0, NULL,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) goto cleanup;

    for (;;) {
        DWORD available = 0, read = 0;
        char buf[8192];
        DWORD written;

        if (!WinHttpQueryDataAvailable(request, &available)) goto cleanup;
        if (available == 0) break;

        while (available > 0) {
            DWORD chunk = (available > sizeof(buf)) ? (DWORD)sizeof(buf)
                                                    : available;
            if (!WinHttpReadData(request, buf, chunk, &read)) goto cleanup;
            if (!WriteFile(file, buf, read, &written, NULL)) goto cleanup;
            available -= read;
        }
    }

    ret = 0;

cleanup:
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
    return ret;
}

/* ------------------------------------------------------------------ */
/*  Helpers: temp directory, extraction, file replacement              */
/* ------------------------------------------------------------------ */

static int make_temp_dir(char *dir_buf, size_t sz)
{
    char tmp[MAX_PATH];
    char name[MAX_PATH];

    if (!GetTempPathA((DWORD)sizeof(tmp), tmp)) return -1;
    if (!GetTempFileNameA(tmp, "jmp", 0, name)) return -1;

    /* GetTempFileNameA creates a 0-byte file; delete it, make a dir */
    DeleteFileA(name);
    if (!CreateDirectoryA(name, NULL)) return -1;

    if (strlen(name) >= sz) return -1;
    strcpy_s(dir_buf, sz, name);
    return 0;
}

static int extract_with_tar(const char *zip_path, const char *dest_dir)
{
    char cmdline[2048];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD exit_code;

    sprintf_s(cmdline, sizeof(cmdline), "tar -xf \"%s\" -C \"%s\" j.exe jc.exe", zip_path, dest_dir);

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "  Failed to run tar (error %lu)\n", GetLastError());
        return -1;
    }

    WaitForSingleObject(pi.hProcess, 30000);
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exit_code != 0) {
        fprintf(stderr, "  tar exited with code %lu\n", exit_code);
        return -1;
    }
    return 0;
}

static int get_exe_directory(char *dir_buf, size_t sz)
{
    char path[MAX_PATH];
    char *last_sep;

    if (!GetModuleFileNameA(NULL, path, MAX_PATH)) return -1;
    last_sep = strrchr(path, '\\');
    if (!last_sep) return -1;
    *(last_sep + 1) = '\0'; /* keep trailing backslash */

    if (strlen(path) >= sz) return -1;
    strcpy_s(dir_buf, sz, path);
    return 0;
}

static int replace_executable(const char *exe_dir, const char *temp_dir,
                              const char *exe_name)
{
    char old_path[MAX_PATH], new_src[MAX_PATH], bak_path[MAX_PATH];

    sprintf_s(old_path, sizeof(old_path), "%s%s", exe_dir, exe_name);
    sprintf_s(new_src, sizeof(new_src), "%s\\%s", temp_dir, exe_name);
    sprintf_s(bak_path, sizeof(bak_path), "%s%s.old", exe_dir, exe_name);

    /* Check new file exists */
    if (GetFileAttributesA(new_src) == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "  %s not found in downloaded archive\n", exe_name);
        return -1;
    }

    /* Remove any leftover .old from a previous update */
    DeleteFileA(bak_path);

    /* Rename current → .old (may fail if it doesn't exist yet, that's OK) */
    if (GetFileAttributesA(old_path) != INVALID_FILE_ATTRIBUTES) {
        if (!MoveFileExA(old_path, bak_path, MOVEFILE_REPLACE_EXISTING)) {
            fprintf(stderr, "  Cannot rename %s → %s.old (error %lu)\n",
                    exe_name, exe_name, GetLastError());
            return -1;
        }
    }

    /* Move new file into place */
    if (!MoveFileExA(new_src, old_path, MOVEFILE_REPLACE_EXISTING)) {
        fprintf(stderr, "  Cannot move new %s into place (error %lu)\n",
                exe_name, GetLastError());
        /* Try to restore backup */
        MoveFileExA(bak_path, old_path, MOVEFILE_REPLACE_EXISTING);
        return -1;
    }

    /* Schedule .old for deletion on reboot (best-effort) */
    MoveFileExA(bak_path, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Helpers: cleanup                                                   */
/* ------------------------------------------------------------------ */

static void cleanup_path(const char *path)
{
    if (path[0]) {
        DeleteFileA(path);
        RemoveDirectoryA(path);
    }
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

#define GITHUB_API_URL \
    "https://api.github.com/repos/RazMake/j-tool/releases/latest"

int jump_update(void)
{
    char *response = NULL;
    DWORD resp_size = 0;
    char tag[64] = {0};
    char asset_url[1024] = {0};
    int cur_ver[3], latest_ver[3];
    char temp_dir[MAX_PATH] = {0};
    char zip_path[MAX_PATH] = {0};
    char exe_dir[MAX_PATH] = {0};
    int result = J_EXIT_RUNTIME_ERROR;

    fprintf(stderr, "Checking for updates...\n");

    /* 1. Fetch latest release info */
    if (winhttp_get(GITHUB_API_URL, &response, &resp_size) != 0) {
        fprintf(stderr, "  Failed to contact GitHub.\n");
        goto done;
    }

    /* 2. Extract tag_name */
    if (!json_get_string(response, response, "tag_name", tag, sizeof(tag))) {
        fprintf(stderr, "  Could not parse release info.\n");
        goto done;
    }

    /* 3. Compare versions */
    if (parse_version(JUMP_VERSION, cur_ver) != 0) {
        fprintf(stderr, "  Invalid current version string.\n");
        goto done;
    }
    if (parse_version(tag, latest_ver) != 0) {
        fprintf(stderr, "  Invalid release tag: %s\n", tag);
        goto done;
    }
    if (compare_versions(cur_ver, latest_ver) >= 0) {
        fprintf(stderr, "Already up to date (v%d.%d.%d).\n",
                cur_ver[0], cur_ver[1], cur_ver[2]);
        result = J_EXIT_OK;
        goto done;
    }

    fprintf(stderr, "New version available: %s\n", tag);

    /* 4. Find zip asset URL */
    if (find_asset_url(response, "jump-", ".zip",
                       asset_url, sizeof(asset_url)) != 0) {
        fprintf(stderr, "  No matching .zip asset found in release.\n");
        goto done;
    }

    /* 5. Prepare temp directory & download */
    if (make_temp_dir(temp_dir, sizeof(temp_dir)) != 0) {
        fprintf(stderr, "  Cannot create temp directory.\n");
        goto done;
    }

    sprintf_s(zip_path, sizeof(zip_path), "%s\\update.zip", temp_dir);

    fprintf(stderr, "Downloading %s ...\n", asset_url);
    if (winhttp_download(asset_url, zip_path) != 0) {
        fprintf(stderr, "  Download failed.\n");
        goto done;
    }

    /* 6. Extract j.exe and jc.exe */
    fprintf(stderr, "Extracting...\n");
    if (extract_with_tar(zip_path, temp_dir) != 0) {
        fprintf(stderr, "  Extraction failed.\n");
        goto done;
    }

    /* 7. Replace executables */
    if (get_exe_directory(exe_dir, sizeof(exe_dir)) != 0) {
        fprintf(stderr, "  Cannot determine executable directory.\n");
        goto done;
    }

    fprintf(stderr, "Replacing executables...\n");
    if (replace_executable(exe_dir, temp_dir, "j.exe") != 0)
        goto done;
    if (replace_executable(exe_dir, temp_dir, "jc.exe") != 0)
        goto done;

    fprintf(stderr, "Update complete! Restart to use the new version.\n");
    result = J_EXIT_OK;

done:
    free(response);
    /* Clean up temp files */
    if (zip_path[0]) DeleteFileA(zip_path);
    if (temp_dir[0]) {
        /* Remove any remaining extracted files */
        {
            char tmp[MAX_PATH];
            sprintf_s(tmp, sizeof(tmp), "%s\\j.exe", temp_dir);
            cleanup_path(tmp);
            sprintf_s(tmp, sizeof(tmp), "%s\\jc.exe", temp_dir);
            cleanup_path(tmp);
        }
        RemoveDirectoryA(temp_dir);
    }
    return result;
}
