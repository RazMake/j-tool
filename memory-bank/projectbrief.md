# Project Brief

## Project Name
**Jump** (J-Tool)

## Purpose
A fast, lightweight directory/URL/program launcher for Windows. Users define aliases in INI files and jump to directories, open URLs, or launch programs with a single short command (`j <alias>`).

## Core Features
- **Directory navigation** — `j work` to `cd` into a mapped directory
- **URL opening** — Aliases open websites in the default browser
- **Program execution** — Launch programs with parameter substitution (`{1}`, `{2}`, …)
- **INI-based configuration** — Human-readable config with `[Include]` support
- **Constants & environment variables** — `{{CONSTANT}}` and `{{ENV:VARNAME}}` placeholders
- **Binary caching** — Configs compiled to binary cache, re-parsed only when source changes
- **OSD overlay** — Transparent on-screen popup confirms which shortcut was activated
- **Fuzzy suggestions** — Mistyped aliases produce closest-match suggestions (Levenshtein distance)
- **Shell integration** — CMD (DOSKEY macro), PowerShell (`$PROFILE` function), Total Commander panel switching
- **Portable** — Statically linked, no runtime dependencies

## Two Executables
| Binary   | Subsystem | Purpose |
|----------|-----------|---------|
| `j.exe`  | Windows   | Silent operation + OSD display (no console window) |
| `jc.exe` | Console   | Interactive use, prints output to the terminal |

## Usage
```
j  <alias> [params...]        Resolve alias and perform action
jc <alias> [params...]        Same, with console output
jc --install [--tc-panel=L|R] Install shell integration
jc --uninstall                Remove shell integration
jc --list                     List all defined aliases
j  --osd "text"               Show OSD overlay (internal)
```

## Target Platform
Windows only (MSVC, Win32 APIs)

## Current Version
v1.0 — Initial Release
