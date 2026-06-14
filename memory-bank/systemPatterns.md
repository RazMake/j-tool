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
| `platform/log.c` | One-shot diagnostic logging to `%TEMP%\\jump.log` with tagged IDs |

## Key Data Structures (types.h)

- `ShortcutType` — enum: `SHORTCUT_CD`, `SHORTCUT_OPEN`, `SHORTCUT_EXEC`
- `Shortcut` — aliases[8] + label + target + type
- `Constant` — name + value pair for template expansion
- `JumpConfig` — up to 256 shortcuts + 64 constants

## Command Dispatch Order
- `jump_main()` dispatches CLI flags in two phases:
  1. **Config-independent** commands first: `--log`, `--osd`, `--install`, `--uninstall`, `--version`, `--update`
  2. **Config-dependent** commands after `config_load()`: `--list`, alias resolution
- **Rule:** when adding a new command, verify whether it needs the INI configuration. If it does not, place it before the config-loading section so it remains functional even when INI files are broken/corrupted.

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

## Versioning & Changelog Discipline

### Changelog is mandatory for every change
- **Every** behavior-affecting change MUST be recorded in `CHANGELOG.md`, summarizing the change in behavior (what the user can now do or what changed), not the implementation detail.
- While a change is unreleased, add it under the `## Unreleased` section (create it if missing) using a bullet of the form `- **Short title** — one-sentence summary of the behavior change.`
- Group entries under a subsection like `### Features`, `### Fixes`, or `### Changes` as appropriate.

### How to bump the version
The single source of truth for the version is `CMakeLists.txt`. It flows into `src/core/version.h.in` → generated `version.h` at configure time, and `release.ps1` reads it back from `CMakeLists.txt`.

To increase the version (e.g. to `1.1`):
1. **`CMakeLists.txt`** — update `project(Jump VERSION X.Y.Z LANGUAGES C)`. (Note: `release.ps1 -Version X.Y.Z` can also write this for you.)
2. **`CHANGELOG.md`** — rename the `## Unreleased` heading to `## vX.Y` (or add a new released section) so the accumulated entries become the release notes. Keep older release sections below it.
3. **`memory-bank/projectbrief.md`** — update the `## Current Version` line.
4. Do **not** hand-edit the generated `build/generated/version.h`; it is regenerated from `version.h.in` on the next configure.


## Template Expansion
- `{{CONSTANT}}` → looked up from merged constants (case-insensitive)
- `{{ENV:VARNAME}}` → looked up from environment variables
- Constants can reference other constants: `WORK={{ROOT}}\Work`
- Constant values are expanded iteratively after merging (up to 10 passes)
- Circular constant references (A→B→A) are detected and reported as errors
- Expansion runs twice: after root constants merge (so include paths can use derived constants) and after all includes are processed
- Single-pass expansion for shortcut targets at resolve time (no nesting)

## Parameter Substitution (EXEC type)
- `{1}`, `{2}`, … replaced with command-line arguments (1-indexed)
- Extra params beyond highest placeholder are appended
- If no placeholders, all params appended space-separated

## Limits
| Constraint | Value |
|-----------|-------|
| Max shortcuts | 256 |
| Max constants | 64 |
| Max aliases per shortcut | 20 |
| Max sections (INI parser) | 64 |
| Max entries per section | 32 |
| Max key length | 64 bytes |
| Max value length | 2048 bytes |
