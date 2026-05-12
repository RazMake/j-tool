/* INI file parser — lightweight, zero-allocation parser that works on
 * an in-memory text buffer. Produces an array of sections, each with
 * an array of key/value entries. */
#ifndef INI_PARSER_H
#define INI_PARSER_H

#include <stddef.h>

#define INI_MAX_SECTIONS    64
#define INI_MAX_ENTRIES     32
#define INI_MAX_KEY_LEN     64
#define INI_MAX_VALUE_LEN   2048
#define INI_MAX_NAME_LEN    128
#define INI_MAX_ERROR_LEN   256

typedef struct {
    char key[INI_MAX_KEY_LEN];
    char value[INI_MAX_VALUE_LEN];
} IniEntry;

typedef struct {
    char     name[INI_MAX_NAME_LEN];
    IniEntry entries[INI_MAX_ENTRIES];
    int      entry_count;
} IniSection;

typedef struct {
    IniSection sections[INI_MAX_SECTIONS];
    int        section_count;
    char       error_msg[INI_MAX_ERROR_LEN];
} IniFile;

/*
 * Parse INI text buffer into structured sections and entries.
 *
 * Rules:
 *  - Lines starting with ';' or '#' are comments (skipped).
 *  - Blank lines are skipped.
 *  - [SectionName] starts a new section.
 *  - key=value lines: key and value are trimmed of leading/trailing whitespace.
 *  - Lines without '=' inside a section: key is empty string, value is trimmed line.
 *  - Content before the first section header is ignored.
 *  - Keys are stored as-is (case preserved; caller compares case-insensitively).
 *
 * Returns 0 on success, non-zero on error (error_msg is filled in).
 */
int ini_parse(const char *text, size_t length, IniFile *result);

/*
 * Find a section by name (case-insensitive comparison).
 * Returns pointer to the section, or NULL if not found.
 */
const IniSection *ini_find_section(const IniFile *file, const char *name);

/*
 * Find a value by key within a section (case-insensitive comparison).
 * Returns pointer to the value string, or NULL if not found.
 */
const char *ini_find_value(const IniSection *section, const char *key);

#endif /* INI_PARSER_H */
