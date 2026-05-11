# System Patterns

## Architecture Overview

```
Entry Points: main.c (console) / main_win.c (Windows GUI)
                           │
                    jump.c (CLI dispatch)
                    ┌──────┼──────────────┐
                    │      │              │
              config.c  resolver.c    install.c
              ┌──┴──┐    │  └ suggest.c   └ tc.c
         ini_parser.c  cache.c
                              osd.c  tc.c
```

## Module Responsibilities

| Module | Purpose |
|--------|---------|
| `core/jump.c` | Main dispatcher — routes to resolve/install/list/OSD modes |
| `config/ini_parser.c` | Parse INI text → structured sections and entries |
| `config/cache.c` | Binary cache save/load with mtime-based freshness |
| `config/config.c` | Orchestrate loading: env var → cache check → parse → validate → save cache |
| `resolve/resolver.c` | Alias lookup + template expansion + param substitution |
| `resolve/suggest.c` | Levenshtein distance + top-N fuzzy match suggestions |
| `platform/osd.c` | Win32 transparent overlay window |
| `platform/tc.c` | Total Commander detection, command building, navigation |
| `platform/install.c` | Shell integration setup/teardown for CMD, PowerShell, PATH |

## Key Data Structures (types.h)

- `ShortcutType` — enum: `SHORTCUT_CD`, `SHORTCUT_OPEN`, `SHORTCUT_EXEC`
- `Shortcut` — aliases[8] + label + target + type
- `Constant` — name + value pair for template expansion
- `JumpConfig` — up to 256 shortcuts + 64 constants

## Error Handling Pattern
- All functions return `int` (0 = success, nonzero = specific error code)
- Exit codes: `J_EXIT_OK`, `J_EXIT_NOT_FOUND`, `J_EXIT_CONFIG_ERROR`, `J_EXIT_RUNTIME_ERROR`
- Config errors logged to stderr with detailed messages

## Config Loading Flow
1. Read `JUMPS` env var → path to root INI file
2. Check binary cache freshness (compare mtimes)
3. If fresh → load from cache, done
4. If stale → read root INI (auto-detect encoding: UTF-8 BOM, UTF-16 LE, system codepage)
5. Parse root INI, process `[Include]` recursively
6. Merge constants (error on duplicates)
7. Extract shortcuts from all non-special sections
8. Validate (no duplicate aliases/constants)
9. Save to binary cache (`%TEMP%\jump.cache`)

## Template Expansion
- `{{CONSTANT}}` → looked up from merged constants (case-insensitive)
- `{{ENV:VARNAME}}` → looked up from environment variables
- Single-pass expansion (no nesting)

## Parameter Substitution (EXEC type)
- `{1}`, `{2}`, … replaced with command-line arguments (1-indexed)
- Extra params beyond highest placeholder are appended
- If no placeholders, all params appended space-separated

## Limits
| Constraint | Value |
|-----------|-------|
| Max shortcuts | 256 |
| Max constants | 64 |
| Max aliases per shortcut | 8 |
| Max sections (INI parser) | 64 |
| Max entries per section | 32 |
| Max key length | 64 bytes |
| Max value length | 2048 bytes |
