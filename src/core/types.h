/* Core type definitions and limits for the Jump configuration model.
 * Defines Shortcut (alias-to-target mappings), Constant (reusable
 * path fragments), and JumpConfig (the aggregate parsed config). */
#ifndef JUMP_TYPES_H
#define JUMP_TYPES_H

/* --- Size limits for all config arrays and strings --- */
#define MAX_ALIAS_LEN            40
#define MAX_PATH_LEN             2048
#define MAX_LABEL_LEN            128
#define MAX_ALIASES_PER_SHORTCUT 8
#define MAX_SHORTCUTS            256
#define MAX_CONSTANTS            64
#define MAX_SOURCE_FILES         32

/* --- Process exit codes (used by shells to detect errors) --- */
#define J_EXIT_OK                0
#define J_EXIT_NOT_FOUND         1
#define J_EXIT_CONFIG_ERROR      2
#define J_EXIT_RUNTIME_ERROR     3

typedef enum {
    SHORTCUT_CD,
    SHORTCUT_OPEN,
    SHORTCUT_EXEC
} ShortcutType;

typedef struct {
    char aliases[MAX_ALIASES_PER_SHORTCUT][MAX_ALIAS_LEN];
    int  alias_count;
    char label[MAX_LABEL_LEN];
    char target[MAX_PATH_LEN];
    ShortcutType type;
} Shortcut;

typedef struct {
    char name[MAX_ALIAS_LEN];
    char value[MAX_PATH_LEN];
} Constant;

typedef struct {
    Shortcut shortcuts[MAX_SHORTCUTS];
    int      shortcut_count;
    Constant constants[MAX_CONSTANTS];
    int      constant_count;
} JumpConfig;

#endif /* JUMP_TYPES_H */
