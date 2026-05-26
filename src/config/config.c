/*
 * config.c – Load, parse, validate and expand Jump shortcut configuration.
 */

#include "config.h"
#include "ini_parser.h"
#include "cache.h"
#include "error.h"
#include "log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <windows.h>

/* ── helpers ─────────────────────────────────────────────────────────── */

static int str_icmp(const char *a, const char *b) {
    return _stricmp(a, b);
}

static void str_trim(char *s) {
    char *start = s;
    char *end;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) {
        size_t len = strlen(start);
        memmove(s, start, len + 1);
    }
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
}

static void str_upper(char *dst, const char *src, size_t max) {
    size_t i;
    for (i = 0; i < max - 1 && src[i]; i++)
        dst[i] = (char)toupper((unsigned char)src[i]);
    dst[i] = '\0';
}

/* Read a file into a malloc'd buffer. Returns bytes read, 0 on error. */
static DWORD read_file_bytes(const wchar_t *path, char **out) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD sz, rd;
    if (h == INVALID_HANDLE_VALUE) return 0;
    sz = GetFileSize(h, NULL);
    if (sz == INVALID_FILE_SIZE || sz == 0) { CloseHandle(h); return 0; }
    *out = (char *)malloc(sz + 1);
    if (!*out) { CloseHandle(h); return 0; }
    if (!ReadFile(h, *out, sz, &rd, NULL) || rd == 0) {
        free(*out); *out = NULL; CloseHandle(h); return 0;
    }
    (*out)[rd] = '\0';
    CloseHandle(h);
    return rd;
}

/* Convert raw file bytes to UTF-8 (malloc'd). Handles BOM detection. */
static char *to_utf8(const char *raw, DWORD len, DWORD *out_len) {
    /* UTF-8 BOM */
    if (len >= 3 && (unsigned char)raw[0] == 0xEF &&
        (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF) {
        DWORD n = len - 3;
        char *r = (char *)malloc(n + 1);
        if (!r) return NULL;
        memcpy(r, raw + 3, n);
        r[n] = '\0';
        *out_len = n;
        return r;
    }
    /* UTF-16 LE BOM */
    if (len >= 2 && (unsigned char)raw[0] == 0xFF &&
        (unsigned char)raw[1] == 0xFE) {
        const wchar_t *ws = (const wchar_t *)(raw + 2);
        int wchars = (int)(len - 2) / (int)sizeof(wchar_t);
        int need = WideCharToMultiByte(CP_UTF8, 0, ws, wchars, NULL, 0, NULL, NULL);
        char *r = (char *)malloc(need + 1);
        if (!r) return NULL;
        WideCharToMultiByte(CP_UTF8, 0, ws, wchars, r, need, NULL, NULL);
        r[need] = '\0';
        *out_len = (DWORD)need;
        return r;
    }
    /* System codepage → wide → UTF-8 */
    {
        int wn = MultiByteToWideChar(CP_ACP, 0, raw, (int)len, NULL, 0);
        wchar_t *wb = (wchar_t *)malloc(wn * sizeof(wchar_t));
        int need; char *r;
        if (!wb) return NULL;
        MultiByteToWideChar(CP_ACP, 0, raw, (int)len, wb, wn);
        need = WideCharToMultiByte(CP_UTF8, 0, wb, wn, NULL, 0, NULL, NULL);
        r = (char *)malloc(need + 1);
        if (!r) { free(wb); return NULL; }
        WideCharToMultiByte(CP_UTF8, 0, wb, wn, r, need, NULL, NULL);
        r[need] = '\0';
        free(wb);
        *out_len = (DWORD)need;
        return r;
    }
}

/* Extract directory part of a wide path into dir_buf. */
static void get_directory(const wchar_t *path, wchar_t *dir_buf, size_t max) {
    const wchar_t *last_sep = NULL, *p;
    for (p = path; *p; p++) {
        if (*p == L'\\' || *p == L'/') last_sep = p;
    }
    if (last_sep) {
        size_t n = (size_t)(last_sep - path);
        if (n >= max) n = max - 1;
        wcsncpy_s(dir_buf, max, path, n);
    } else {
        dir_buf[0] = L'\0';
    }
}

/* Add constants from an IniFile into cfg. Checks for duplicates. */
static int merge_constants(JumpConfig *cfg, const IniFile *ini) {
    const IniSection *sec = ini_find_section(ini, "Constants");
    int i, j;
    if (!sec) return 0;
    for (i = 0; i < sec->entry_count; i++) {
        /* check dup */
        for (j = 0; j < cfg->constant_count; j++) {
            if (str_icmp(cfg->constants[j].name, sec->entries[i].key) == 0) {
                error_report("Duplicate constant '%s'\n",
                        sec->entries[i].key);
                return -1;
            }
        }
        if (cfg->constant_count >= MAX_CONSTANTS) {
            error_report("Too many constants (max %d)\n", MAX_CONSTANTS);
            return -1;
        }
        str_upper(cfg->constants[cfg->constant_count].name,
                  sec->entries[i].key, MAX_ALIAS_LEN);
        strncpy_s(cfg->constants[cfg->constant_count].value, MAX_PATH_LEN,
                  sec->entries[i].value, _TRUNCATE);
        cfg->constant_count++;
    }
    return 0;
}

/* Extract shortcuts from an IniFile into cfg. */
static int extract_shortcuts(JumpConfig *cfg, const IniFile *ini,
                             const char *source_file) {
    int s;
    for (s = 0; s < ini->section_count; s++) {
        const IniSection *sec = &ini->sections[s];
        const char *jumps_val, *label_val, *path_val, *open_val, *exec_val;
        Shortcut *sc;
        char jumps_copy[INI_MAX_VALUE_LEN];
        char *tok, *ctx;

        if (str_icmp(sec->name, "Constants") == 0 ||
            str_icmp(sec->name, "Include") == 0)
            continue;

        jumps_val = ini_find_value(sec, "Jumps");
        path_val  = ini_find_value(sec, "Path");
        open_val  = ini_find_value(sec, "Open");
        exec_val  = ini_find_value(sec, "Execute");

        if (!jumps_val || (!path_val && !open_val && !exec_val))
            continue;

        if (cfg->shortcut_count >= MAX_SHORTCUTS) {
            error_report("Too many shortcuts (max %d)\n", MAX_SHORTCUTS);
            return -1;
        }

        sc = &cfg->shortcuts[cfg->shortcut_count];
        memset(sc, 0, sizeof(Shortcut));

        strncpy_s(sc->source_file, MAX_LABEL_LEN, source_file, _TRUNCATE);
        strncpy_s(sc->source_section, MAX_LABEL_LEN, sec->name, _TRUNCATE);

        label_val = ini_find_value(sec, "Label");
        if (label_val)
            strncpy_s(sc->label, MAX_LABEL_LEN, label_val, _TRUNCATE);
        else
            strncpy_s(sc->label, MAX_LABEL_LEN, sec->name, _TRUNCATE);

        if (path_val) {
            sc->type = SHORTCUT_CD;
            strncpy_s(sc->target, MAX_PATH_LEN, path_val, _TRUNCATE);
        } else if (open_val) {
            sc->type = SHORTCUT_OPEN;
            strncpy_s(sc->target, MAX_PATH_LEN, open_val, _TRUNCATE);
        } else {
            const char *hide_val = ini_find_value(sec, "HideConsole");
            sc->type = SHORTCUT_EXEC;
            strncpy_s(sc->target, MAX_PATH_LEN, exec_val, _TRUNCATE);
            if (hide_val && (str_icmp(hide_val, "true") == 0 ||
                             str_icmp(hide_val, "yes") == 0 ||
                             str_icmp(hide_val, "1") == 0)) {
                sc->hide_console = 1;
            }
        }

        strncpy_s(jumps_copy, sizeof(jumps_copy), jumps_val, _TRUNCATE);
        tok = strtok_s(jumps_copy, ",", &ctx);
        while (tok && sc->alias_count < MAX_ALIASES_PER_SHORTCUT) {
            str_trim(tok);
            if (tok[0]) {
                strncpy_s(sc->aliases[sc->alias_count], MAX_ALIAS_LEN,
                          tok, _TRUNCATE);
                sc->alias_count++;
            }
            tok = strtok_s(NULL, ",", &ctx);
        }

        cfg->shortcut_count++;
    }
    return 0;
}

/* ── expand_constant_values ───────────────────────────────────────────
 * Iteratively expand {{CONSTANT}} references within constant values
 * so that constants can reference other constants. Detects circular
 * references after expansion stabilises. */
static int expand_constant_values(JumpConfig *cfg) {
    int changed = 1;
    int passes = 0;
    const int max_passes = 10;
    int i;

    while (changed && passes < max_passes) {
        changed = 0;
        passes++;
        for (i = 0; i < cfg->constant_count; i++) {
            if (strstr(cfg->constants[i].value, "{{") != NULL) {
                char expanded[MAX_PATH_LEN];
                if (config_expand(cfg, cfg->constants[i].value,
                                  expanded, sizeof(expanded)) == 0) {
                    if (strcmp(cfg->constants[i].value, expanded) != 0) {
                        strncpy_s(cfg->constants[i].value, MAX_PATH_LEN,
                                  expanded, _TRUNCATE);
                        changed = 1;
                    }
                }
            }
        }
    }

    /* Check for unresolved constant self/circular references */
    for (i = 0; i < cfg->constant_count; i++) {
        const char *p = cfg->constants[i].value;
        while ((p = strstr(p, "{{")) != NULL) {
            const char *close = strstr(p + 2, "}}");
            if (!close) break;
            if (_strnicmp(p + 2, "ENV:", 4) != 0) {
                char token[MAX_ALIAS_LEN];
                size_t tlen = (size_t)(close - (p + 2));
                int j;
                if (tlen < sizeof(token)) {
                    memcpy(token, p + 2, tlen);
                    token[tlen] = '\0';
                    for (j = 0; j < cfg->constant_count; j++) {
                        if (str_icmp(cfg->constants[j].name, token) == 0) {
                            error_report(
                                "Circular constant reference: "
                                "'%s' references '%s'\n",
                                cfg->constants[i].name, token);
                            return -1;
                        }
                    }
                }
            }
            p = close + 2;
        }
    }

    return 0;
}

/* ── config_expand ───────────────────────────────────────────────────── */

int config_expand(const JumpConfig *cfg, const char *input,
                  char *out_buf, size_t out_size) {
    char tmp_buf[MAX_PATH_LEN];
    const char *src = input;
    int pass;
    const int max_passes = 10;

    for (pass = 0; pass < max_passes; pass++) {
        const char *p = src;
        size_t wp = 0;  /* write position */

        while (*p) {
            if (p[0] == '{' && p[1] == '{') {
                const char *close = strstr(p + 2, "}}");
                if (close) {
                    char token[MAX_PATH_LEN];
                    size_t tlen = (size_t)(close - (p + 2));
                    if (tlen >= sizeof(token)) tlen = sizeof(token) - 1;
                    memcpy(token, p + 2, tlen);
                    token[tlen] = '\0';

                    if (_strnicmp(token, "ENV:", 4) == 0) {
                        /* environment variable */
                        char env_val[MAX_PATH_LEN];
                        DWORD r = GetEnvironmentVariableA(token + 4, env_val,
                                                         (DWORD)sizeof(env_val));
                        if (r == 0) {
                            log_write("CFG40", "ENV var '%s' not set, expanding to empty", token + 4);
                            env_val[0] = '\0';
                        }
                        if (wp + r >= out_size) {
                            log_write("CFG41", "Buffer too small expanding ENV:'%s'", token + 4);
                            return -1; /* buffer too small */
                        }
                        memcpy(out_buf + wp, env_val, r);
                        wp += r;
                    } else {
                        /* constant lookup */
                        char upper[MAX_ALIAS_LEN];
                        int i, found = 0;
                        str_upper(upper, token, MAX_ALIAS_LEN);
                        for (i = 0; i < cfg->constant_count; i++) {
                            if (str_icmp(cfg->constants[i].name, upper) == 0) {
                                size_t vlen = strlen(cfg->constants[i].value);
                                if (wp + vlen >= out_size) return -1;
                                memcpy(out_buf + wp, cfg->constants[i].value, vlen);
                                wp += vlen;
                                found = 1;
                                break;
                            }
                        }
                        if (!found) {
                            log_write("CFG42", "Unknown constant '%s'", token);
                            error_report("Unknown constant '%s'\n", token);
                            return -1;
                        }
                    }
                    p = close + 2;
                    continue;
                }
            }
            if (wp + 1 >= out_size) return -1;
            out_buf[wp++] = *p++;
        }
        if (wp >= out_size) return -1;
        out_buf[wp] = '\0';

        /* If no more {{…}} remain, we're done */
        if (strstr(out_buf, "{{") == NULL)
            return 0;

        /* Copy result to tmp_buf for the next pass */
        strncpy_s(tmp_buf, sizeof(tmp_buf), out_buf, _TRUNCATE);
        src = tmp_buf;
    }

    return 0;
}

/* ── config_validate ─────────────────────────────────────────────────── */

int config_validate(const JumpConfig *cfg) {
    int i, j, k, m;

    /* Check duplicate aliases across shortcuts */
    for (i = 0; i < cfg->shortcut_count; i++) {
        for (k = 0; k < cfg->shortcuts[i].alias_count; k++) {
            for (j = i; j < cfg->shortcut_count; j++) {
                int start = (j == i) ? k + 1 : 0;
                for (m = start; m < cfg->shortcuts[j].alias_count; m++) {
                    if (str_icmp(cfg->shortcuts[i].aliases[k],
                                 cfg->shortcuts[j].aliases[m]) == 0) {
                        error_report(
                                "Duplicate alias '%s'\n"
                                "  [%s] in %s\n"
                                "  [%s] in %s\n",
                                cfg->shortcuts[i].aliases[k],
                                cfg->shortcuts[i].source_section,
                                cfg->shortcuts[i].source_file,
                                cfg->shortcuts[j].source_section,
                                cfg->shortcuts[j].source_file);
                        return J_EXIT_CONFIG_ERROR;
                    }
                }
            }
        }
    }

    /* Check duplicate constant names */
    for (i = 0; i < cfg->constant_count; i++) {
        for (j = i + 1; j < cfg->constant_count; j++) {
            if (str_icmp(cfg->constants[i].name,
                         cfg->constants[j].name) == 0) {
                error_report("Duplicate constant '%s'\n",
                        cfg->constants[i].name);
                return J_EXIT_CONFIG_ERROR;
            }
        }
    }

    return 0;
}

/* ── config_load ─────────────────────────────────────────────────────── */

int config_load(JumpConfig *cfg) {
    char jumps_path_a[MAX_PATH];
    wchar_t jumps_path[MAX_PATH];
    wchar_t cache_path[MAX_PATH];
    wchar_t root_dir[MAX_PATH];
    char *raw = NULL;
    char *utf8 = NULL;
    DWORD raw_len, utf8_len;
    IniFile *ini = NULL;
    const IniSection *inc_sec;
    CacheSourceFile sources[MAX_SOURCE_FILES];
    int source_count = 0;
    int i, ret;
    WIN32_FILE_ATTRIBUTE_DATA fattr;

    memset(cfg, 0, sizeof(JumpConfig));
    log_write("CFG01", "config_load started");

    /* 1. Read JUMPS env var */
    if (GetEnvironmentVariableA("JUMPS", jumps_path_a, MAX_PATH) == 0) {
        log_write("CFG02", "JUMPS env var not set (err=%lu)", GetLastError());
        error_report("JUMPS environment variable not set\n");
        return J_EXIT_CONFIG_ERROR;
    }
    log_write("CFG03", "JUMPS='%s'", jumps_path_a);
    MultiByteToWideChar(CP_ACP, 0, jumps_path_a, -1, jumps_path, MAX_PATH);

    /* 2. Cache path */
    if (cache_get_default_path(cache_path, MAX_PATH) != 0) {
        log_write("CFG04", "Cannot determine cache path");
        error_report("Cannot determine cache path\n");
        return J_EXIT_CONFIG_ERROR;
    }

    /* 3. Try cache */
    if (cache_is_fresh(cache_path)) {
        log_write("CFG05", "Cache is fresh, attempting cache load");
        if (cache_load(cache_path, cfg) == 0) {
            log_write("CFG06", "Cache loaded: %d shortcuts, %d constants",
                      cfg->shortcut_count, cfg->constant_count);
            return 0;
        }
        log_write("CFG07", "Cache load failed despite being fresh, re-parsing");
    } else {
        log_write("CFG08", "Cache is stale or missing, will parse INI files");
    }

    /* 4. Read root INI file */
    raw_len = read_file_bytes(jumps_path, &raw);
    if (raw_len == 0) {
        log_write("CFG09", "Cannot read config file at JUMPS path");
        error_report("Cannot read config file\n");
        return J_EXIT_CONFIG_ERROR;
    }
    log_write("CFG10", "Read root INI file: %lu bytes", (unsigned long)raw_len);

    utf8 = to_utf8(raw, raw_len, &utf8_len);
    free(raw);
    if (!utf8) {
        log_write("CFG11", "Encoding conversion failed");
        error_report("Encoding conversion failed\n");
        return J_EXIT_CONFIG_ERROR;
    }
    log_write("CFG12", "Converted to UTF-8: %lu bytes", (unsigned long)utf8_len);

    /* 5. Parse root */
    ini = (IniFile *)calloc(1, sizeof(IniFile));
    if (!ini) {
        log_write("CFG13", "Failed to allocate IniFile for root");
        free(utf8);
        return J_EXIT_CONFIG_ERROR;
    }

    if (ini_parse(utf8, utf8_len, ini) != 0) {
        log_write("CFG14", "Root INI parse failed: %s", ini->error_msg);
        error_report("Parse failed: %s\n", ini->error_msg);
        free(utf8); free(ini);
        return J_EXIT_CONFIG_ERROR;
    }
    log_write("CFG15", "Root INI parsed: %d sections", ini->section_count);
    free(utf8);

    /* Record root source file */
    wcsncpy_s(sources[0].path, MAX_PATH, jumps_path, _TRUNCATE);
    if (GetFileAttributesExW(jumps_path, GetFileExInfoStandard, &fattr))
        sources[0].mtime = fattr.ftLastWriteTime;
    source_count = 1;

    /* 6. Extract constants from root */
    if (merge_constants(cfg, ini) != 0) {
        log_write("CFG16", "merge_constants failed for root file");
        free(ini);
        return J_EXIT_CONFIG_ERROR;
    }
    log_write("CFG17", "Root constants merged: %d total", cfg->constant_count);

    /* Expand constant-in-constant references among root constants so
     * that [Include] paths can use derived constants. */
    if (expand_constant_values(cfg) != 0) {
        log_write("CFG17b", "expand_constant_values failed for root constants");
        free(ini);
        return J_EXIT_CONFIG_ERROR;
    }

    /* Get root directory for resolving includes */
    get_directory(jumps_path, root_dir, MAX_PATH);

    /* 7. Process [Include] section */
    inc_sec = ini_find_section(ini, "Include");
    if (inc_sec) {
        log_write("CFG18", "Processing [Include] section: %d entries",
                  inc_sec->entry_count);
        for (i = 0; i < inc_sec->entry_count; i++) {
            const char *inc_file_raw = inc_sec->entries[i].value;
            char inc_file_expanded[MAX_PATH_LEN];
            const char *inc_file;
            wchar_t inc_path[MAX_PATH];
            wchar_t inc_file_w[MAX_PATH];
            IniFile *inc_ini;

            /* Expand constants (e.g. {{KBROOT}}) in include paths */
            if (config_expand(cfg, inc_file_raw, inc_file_expanded,
                              sizeof(inc_file_expanded)) == 0) {
                inc_file = inc_file_expanded;
            } else {
                inc_file = inc_file_raw;
            }

            log_write("CFG19", "Including file: '%s'", inc_file);
            MultiByteToWideChar(CP_UTF8, 0, inc_file, -1,
                                inc_file_w, MAX_PATH);

            /* If the expanded path is absolute, use it directly;
               otherwise resolve it relative to the root INI directory. */
            if ((inc_file_w[0] && inc_file_w[1] == L':') ||
                (inc_file_w[0] == L'\\' && inc_file_w[1] == L'\\')) {
                wcsncpy_s(inc_path, MAX_PATH, inc_file_w, _TRUNCATE);
            } else {
                _snwprintf_s(inc_path, MAX_PATH, _TRUNCATE, L"%s\\%s",
                             root_dir, inc_file_w);
            }

            raw_len = read_file_bytes(inc_path, &raw);
            if (raw_len == 0) {
                char inc_path_a[MAX_PATH];
                WideCharToMultiByte(CP_ACP, 0, inc_path, -1,
                                    inc_path_a, MAX_PATH, NULL, NULL);
                log_write("CFG20", "Cannot read include file '%s' (resolved='%s', err=%lu)",
                          inc_file, inc_path_a, GetLastError());
                error_report("Cannot read include file '%s'\n",
                        inc_file);
                free(ini);
                return J_EXIT_CONFIG_ERROR;
            }
            log_write("CFG21", "Read include file '%s': %lu bytes",
                      inc_file, (unsigned long)raw_len);

            utf8 = to_utf8(raw, raw_len, &utf8_len);
            free(raw);
            if (!utf8) {
                log_write("CFG22", "Encoding conversion failed for '%s'", inc_file);
                free(ini);
                return J_EXIT_CONFIG_ERROR;
            }

            inc_ini = (IniFile *)calloc(1, sizeof(IniFile));
            if (!inc_ini) {
                log_write("CFG23", "Failed to allocate IniFile for '%s'", inc_file);
                free(utf8); free(ini);
                return J_EXIT_CONFIG_ERROR;
            }

            if (ini_parse(utf8, utf8_len, inc_ini) != 0) {
                log_write("CFG24", "Parse failed in '%s': %s",
                          inc_file, inc_ini->error_msg);
                error_report("Parse failed in '%s': %s\n",
                        inc_file, inc_ini->error_msg);
                free(utf8); free(inc_ini); free(ini);
                return J_EXIT_CONFIG_ERROR;
            }
            log_write("CFG25", "Parsed include '%s': %d sections",
                      inc_file, inc_ini->section_count);
            free(utf8);

            /* 8. Merge constants from included file */
            if (merge_constants(cfg, inc_ini) != 0) {
                log_write("CFG26", "merge_constants failed for '%s'", inc_file);
                free(inc_ini); free(ini);
                return J_EXIT_CONFIG_ERROR;
            }

            /* 9. Extract shortcuts from included file */
            if (extract_shortcuts(cfg, inc_ini, inc_file) != 0) {
                log_write("CFG27", "extract_shortcuts failed for '%s'", inc_file);
                free(inc_ini); free(ini);
                return J_EXIT_CONFIG_ERROR;
            }

            /* Record source */
            if (source_count < MAX_SOURCE_FILES) {
                wcsncpy_s(sources[source_count].path, MAX_PATH,
                          inc_path, _TRUNCATE);
                if (GetFileAttributesExW(inc_path, GetFileExInfoStandard,
                                         &fattr))
                    sources[source_count].mtime = fattr.ftLastWriteTime;
                source_count++;
            } else {
                log_write("CFG28", "Max source files reached (%d), cannot track '%s'",
                          MAX_SOURCE_FILES, inc_file);
            }

            free(inc_ini);
        }
    } else {
        log_write("CFG29", "No [Include] section found");
    }

    /* 9 cont. Extract shortcuts from root file */
    if (extract_shortcuts(cfg, ini, jumps_path_a) != 0) {
        log_write("CFG30", "extract_shortcuts failed for root file");
        free(ini);
        return J_EXIT_CONFIG_ERROR;
    }
    free(ini);
    log_write("CFG31", "All shortcuts extracted: %d total", cfg->shortcut_count);

    /* Expand constant-in-constant references for constants from included
     * files that may reference root constants or each other. */
    if (expand_constant_values(cfg) != 0) {
        log_write("CFG31b", "expand_constant_values failed after includes");
        return J_EXIT_CONFIG_ERROR;
    }

    /* 10. Validate */
    ret = config_validate(cfg);
    if (ret != 0) {
        log_write("CFG32", "config_validate failed with %d", ret);
        return ret;
    }
    log_write("CFG33", "Config validated successfully");

    /* 11-12. Save cache */
    if (cache_save(cache_path, cfg, sources, source_count) != 0) {
        log_write("CFG34", "cache_save failed (non-fatal)");
    } else {
        log_write("CFG35", "Cache saved: %d source files tracked", source_count);
    }

    log_write("CFG36", "config_load completed: %d shortcuts, %d constants",
              cfg->shortcut_count, cfg->constant_count);
    return 0;
}
