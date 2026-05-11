# Product Context

## Why This Project Exists
Power users and developers frequently navigate between many directories, URLs, and tools. Typing full paths or clicking through file explorers is slow. Jump provides instant access via short aliases.

## User Workflow
1. User sets `JUMPS` environment variable pointing to a root INI file
2. User defines shortcuts (aliases → directories/URLs/programs) in INI files
3. User runs `jc --install` to set up shell integration
4. From any terminal, user types `j work` to instantly navigate to their work directory

## Configuration Model
- **Root INI** — Contains `[Constants]` and `[Include]` sections
- **Included INIs** — Each defines shortcut sections with `Label`, `Jumps`, and one of `Path`/`Open`/`Execute`
- **Constants** — Reusable values like `PROJECTS=C:\Projects` referenced as `{{PROJECTS}}`
- **Environment variables** — Referenced as `{{ENV:VARNAME}}`

## Three Shortcut Types
| Type     | Key       | Action |
|----------|-----------|--------|
| CD       | `Path=`   | Change directory (writes to stdout + temp cmd file + TC panel) |
| OPEN     | `Open=`   | Open URL/file via ShellExecuteA |
| EXEC     | `Execute=`| Launch program via CreateProcessA with param substitution |

## Shell Integration
- **CMD**: DOSKEY macro `j=jc.exe $* $T call %TEMP%\jump_cd.cmd`
- **PowerShell**: Function in `$PROFILE` that pipes jc output to `Set-Location`
- **Total Commander**: Detects TC via process tree, navigates source panel

## UX Decisions
- OSD auto-closes after 1.5 seconds (hard-coded)
- Fuzzy suggestions shown when alias not found (Levenshtein distance)
- Install is idempotent — safe to run multiple times
- Alias lookup is case-insensitive
