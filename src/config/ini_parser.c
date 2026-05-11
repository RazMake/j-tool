#include "ini_parser.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

static const char *skip_whitespace(const char *s) {
    while (*s && (*s == ' ' || *s == '\t' || *s == '\r'))
        s++;
    return s;
}

static void trim_trailing(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r'))
        len--;
    s[len] = '\0';
}

int ini_parse(const char *text, size_t length, IniFile *result) {
    memset(result, 0, sizeof(IniFile));

    if (!text || length == 0)
        return 0;

    IniSection *cur_section = NULL;
    const char *pos = text;
    const char *end = text + length;

    while (pos < end) {
        /* Extract one line */
        const char *line_end = pos;
        while (line_end < end && *line_end != '\n')
            line_end++;

        /* Copy line into buffer */
        size_t line_len = (size_t)(line_end - pos);
        char line_buf[INI_MAX_VALUE_LEN + INI_MAX_KEY_LEN + 16];
        if (line_len >= sizeof(line_buf))
            line_len = sizeof(line_buf) - 1;
        memcpy(line_buf, pos, line_len);
        line_buf[line_len] = '\0';

        /* Advance past the newline */
        pos = (line_end < end) ? line_end + 1 : line_end;

        /* Trim trailing whitespace (including \r) */
        trim_trailing(line_buf);

        /* Skip leading whitespace */
        const char *trimmed = skip_whitespace(line_buf);

        /* Skip blank lines and comments */
        if (*trimmed == '\0' || *trimmed == ';' || *trimmed == '#')
            continue;

        /* Section header */
        if (*trimmed == '[') {
            const char *close = strchr(trimmed, ']');
            if (!close) {
                snprintf(result->error_msg, INI_MAX_ERROR_LEN,
                         "Unclosed '[' in section header");
                return 1;
            }
            if (result->section_count >= INI_MAX_SECTIONS) {
                snprintf(result->error_msg, INI_MAX_ERROR_LEN,
                         "Too many sections (max %d)", INI_MAX_SECTIONS);
                return 1;
            }
            cur_section = &result->sections[result->section_count++];
            size_t name_len = (size_t)(close - trimmed - 1);
            if (name_len >= INI_MAX_NAME_LEN)
                name_len = INI_MAX_NAME_LEN - 1;
            memcpy(cur_section->name, trimmed + 1, name_len);
            cur_section->name[name_len] = '\0';
            trim_trailing(cur_section->name);
            continue;
        }

        /* Key=value or keyless entry — only if inside a section */
        if (!cur_section)
            continue;

        if (cur_section->entry_count >= INI_MAX_ENTRIES) {
            snprintf(result->error_msg, INI_MAX_ERROR_LEN,
                     "Too many entries in section [%s] (max %d)",
                     cur_section->name, INI_MAX_ENTRIES);
            return 1;
        }

        IniEntry *entry = &cur_section->entries[cur_section->entry_count++];

        const char *eq = strchr(trimmed, '=');
        if (eq) {
            /* key = value */
            size_t key_len = (size_t)(eq - trimmed);
            if (key_len >= INI_MAX_KEY_LEN)
                key_len = INI_MAX_KEY_LEN - 1;
            memcpy(entry->key, trimmed, key_len);
            entry->key[key_len] = '\0';
            trim_trailing(entry->key);

            const char *val = skip_whitespace(eq + 1);
            size_t val_len = strlen(val);
            if (val_len >= INI_MAX_VALUE_LEN)
                val_len = INI_MAX_VALUE_LEN - 1;
            memcpy(entry->value, val, val_len);
            entry->value[val_len] = '\0';
            trim_trailing(entry->value);
        } else {
            /* Keyless entry */
            entry->key[0] = '\0';
            size_t val_len = strlen(trimmed);
            if (val_len >= INI_MAX_VALUE_LEN)
                val_len = INI_MAX_VALUE_LEN - 1;
            memcpy(entry->value, trimmed, val_len);
            entry->value[val_len] = '\0';
        }
    }

    return 0;
}

const IniSection *ini_find_section(const IniFile *file, const char *name) {
    if (!file || !name)
        return NULL;
    for (int i = 0; i < file->section_count; i++) {
#ifdef _WIN32
        if (_stricmp(file->sections[i].name, name) == 0)
#else
        if (strcasecmp(file->sections[i].name, name) == 0)
#endif
            return &file->sections[i];
    }
    return NULL;
}

const char *ini_find_value(const IniSection *section, const char *key) {
    if (!section || !key)
        return NULL;
    for (int i = 0; i < section->entry_count; i++) {
#ifdef _WIN32
        if (_stricmp(section->entries[i].key, key) == 0)
#else
        if (strcasecmp(section->entries[i].key, key) == 0)
#endif
            return section->entries[i].value;
    }
    return NULL;
}
