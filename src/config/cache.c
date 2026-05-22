/* Binary cache for parsed Jump configuration.
 * Serializes JumpConfig to a flat file (%TEMP%\jump.cache) with
 * source-file mtimes so stale caches are detected and rebuilt. */
#include "cache.h"
#include "log.h"
#include <string.h>
#include <stdio.h>

int cache_get_default_path(wchar_t *path, size_t max_chars) {
    DWORD len = GetTempPathW((DWORD)max_chars, path);
    if (len == 0 || len >= max_chars) return -1;
    wcsncat_s(path, max_chars, L"jump.cache", _TRUNCATE);
    return 0;
}

int cache_save(const wchar_t *cache_path, const JumpConfig *cfg,
               const CacheSourceFile *source_files, int source_count) {
    HANDLE hFile;
    CacheHeader header;
    DWORD written;

    log_write("CAC01", "cache_save: %d shortcuts, %d constants, %d sources",
              cfg ? cfg->shortcut_count : -1,
              cfg ? cfg->constant_count : -1, source_count);

    if (!cfg || source_count < 0 || source_count > MAX_SOURCE_FILES) {
        log_write("CAC02", "cache_save: invalid arguments");
        return -1;
    }

    hFile = CreateFileW(cache_path, GENERIC_WRITE, 0, NULL,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        log_write("CAC03", "cache_save: cannot create file (err=%lu)", GetLastError());
        return -1;
    }

    memset(&header, 0, sizeof(header));
    header.magic = CACHE_MAGIC;
    header.version = CACHE_VERSION;
    header.source_file_count = source_count;
    header.shortcut_count = cfg->shortcut_count;
    header.constant_count = cfg->constant_count;
    for (int i = 0; i < source_count; i++)
        header.source_files[i] = source_files[i];

    if (!WriteFile(hFile, &header, sizeof(header), &written, NULL) ||
        written != sizeof(header)) {
        CloseHandle(hFile);
        return -1;
    }

    if (cfg->shortcut_count > 0) {
        DWORD sc_size = (DWORD)(cfg->shortcut_count * sizeof(Shortcut));
        if (!WriteFile(hFile, cfg->shortcuts, sc_size, &written, NULL) ||
            written != sc_size) {
            CloseHandle(hFile);
            return -1;
        }
    }

    if (cfg->constant_count > 0) {
        DWORD ct_size = (DWORD)(cfg->constant_count * sizeof(Constant));
        if (!WriteFile(hFile, cfg->constants, ct_size, &written, NULL) ||
            written != ct_size) {
            CloseHandle(hFile);
            return -1;
        }
    }

    CloseHandle(hFile);
    return 0;
}

int cache_load(const wchar_t *cache_path, JumpConfig *cfg) {
    HANDLE hFile;
    CacheHeader header;
    DWORD bytesRead;

    log_write("CAC04", "cache_load: attempting to load cache");

    if (!cfg) {
        log_write("CAC05", "cache_load: cfg is NULL");
        return -1;
    }
    memset(cfg, 0, sizeof(JumpConfig));

    hFile = CreateFileW(cache_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        log_write("CAC06", "cache_load: cannot open cache file (err=%lu)", GetLastError());
        return -1;
    }

    if (!ReadFile(hFile, &header, sizeof(header), &bytesRead, NULL) ||
        bytesRead != sizeof(header)) {
        log_write("CAC07", "cache_load: cannot read header (read=%lu, expected=%lu)",
                  (unsigned long)bytesRead, (unsigned long)sizeof(header));
        CloseHandle(hFile);
        return -1;
    }

    if (header.magic != CACHE_MAGIC || header.version != CACHE_VERSION) {
        log_write("CAC08", "cache_load: magic/version mismatch (magic=0x%08X, ver=%u)",
                  header.magic, header.version);
        CloseHandle(hFile);
        return -1;
    }

    if (header.shortcut_count < 0 || header.shortcut_count > MAX_SHORTCUTS ||
        header.constant_count < 0 || header.constant_count > MAX_CONSTANTS) {
        log_write("CAC09", "cache_load: invalid counts (shortcuts=%d, constants=%d)",
                  header.shortcut_count, header.constant_count);
        CloseHandle(hFile);
        return -1;
    }

    if (header.shortcut_count > 0) {
        DWORD sc_size = (DWORD)(header.shortcut_count * sizeof(Shortcut));
        if (!ReadFile(hFile, cfg->shortcuts, sc_size, &bytesRead, NULL) ||
            bytesRead != sc_size) {
            CloseHandle(hFile);
            return -1;
        }
    }
    cfg->shortcut_count = header.shortcut_count;

    if (header.constant_count > 0) {
        DWORD ct_size = (DWORD)(header.constant_count * sizeof(Constant));
        if (!ReadFile(hFile, cfg->constants, ct_size, &bytesRead, NULL) ||
            bytesRead != ct_size) {
            CloseHandle(hFile);
            return -1;
        }
    }
    cfg->constant_count = header.constant_count;

    CloseHandle(hFile);
    return 0;
}

int cache_is_fresh(const wchar_t *cache_path) {
    HANDLE hFile;
    CacheHeader header;
    DWORD bytesRead;

    log_write("CAC10", "cache_is_fresh: checking cache freshness");

    hFile = CreateFileW(cache_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        log_write("CAC11", "cache_is_fresh: cache file not found (err=%lu)", GetLastError());
        return 0;
    }

    if (!ReadFile(hFile, &header, sizeof(header), &bytesRead, NULL) ||
        bytesRead != sizeof(header)) {
        log_write("CAC12", "cache_is_fresh: cannot read header");
        CloseHandle(hFile);
        return 0;
    }
    CloseHandle(hFile);

    if (header.magic != CACHE_MAGIC || header.version != CACHE_VERSION) {
        log_write("CAC13", "cache_is_fresh: magic/version mismatch");
        return 0;
    }

    log_write("CAC14", "cache_is_fresh: checking %d source files", header.source_file_count);
    for (int i = 0; i < header.source_file_count; i++) {
        WIN32_FILE_ATTRIBUTE_DATA fdata;
        if (!GetFileAttributesExW(header.source_files[i].path,
                                  GetFileExInfoStandard, &fdata)) {
            log_write("CAC15", "cache_is_fresh: source file %d not found", i);
            return 0;
        }
        if (CompareFileTime(&fdata.ftLastWriteTime,
                            &header.source_files[i].mtime) != 0) {
            log_write("CAC16", "cache_is_fresh: source file %d has changed", i);
            return 0;
        }
    }

    log_write("CAC17", "cache_is_fresh: cache is fresh");
    return 1;
}
