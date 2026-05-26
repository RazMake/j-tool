/* Binary config cache — stores parsed shortcuts and constants to avoid
 * re-parsing INI files on every invocation. Tracks source file mtimes
 * to automatically invalidate when any config file changes. */
#ifndef CACHE_H
#define CACHE_H

#include "types.h"
#include <windows.h>

#define CACHE_MAGIC   0x4A554D50  /* "JUMP" */
#define CACHE_VERSION 3

typedef struct {
    wchar_t  path[MAX_PATH];
    FILETIME mtime;
} CacheSourceFile;

typedef struct {
    DWORD           magic;
    DWORD           version;
    int             source_file_count;
    int             shortcut_count;
    int             constant_count;
    CacheSourceFile source_files[MAX_SOURCE_FILES];
} CacheHeader;

/*
 * Check if binary cache at cache_path is fresh.
 * Reads the header, compares stored mtimes against actual file mtimes.
 * Returns 1 if fresh (all files unchanged), 0 if stale or missing.
 */
int cache_is_fresh(const wchar_t *cache_path);

/*
 * Load config from binary cache file.
 * Reads header, then copies shortcuts and constants into cfg.
 * Returns 0 on success, non-zero on failure.
 */
int cache_load(const wchar_t *cache_path, JumpConfig *cfg);

/*
 * Save config to binary cache file.
 * Writes header (with source file info) followed by shortcuts and constants.
 * Returns 0 on success, non-zero on failure.
 */
int cache_save(const wchar_t *cache_path, const JumpConfig *cfg,
               const CacheSourceFile *source_files, int source_count);

/*
 * Get the default cache file path: %TEMP%\jump.cache
 * Returns 0 on success, non-zero if TEMP is not set or path too long.
 */
int cache_get_default_path(wchar_t *path, size_t max_chars);

#endif /* CACHE_H */
