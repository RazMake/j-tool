# Changelog

## Unreleased

### Features

- **VBS script execution** — `Execute=` targets ending in `.vbs` are auto-launched through the Windows Script Host (`cscript.exe`, or `wscript.exe` when `HideConsole=true`).
- **`HideConsole` flag** — EXEC shortcuts may set `HideConsole=true` to suppress the console window of the spawned process. Applies to `.exe`, `.cmd`/`.bat`, `.ps1`, and `.vbs` targets.

## v1.0 — Initial Release

### Features

- **Directory jumping** — Resolve aliases to directories and `cd` into them instantly.
- **URL opening** — Aliases can open URLs in the default browser via the system handler.
- **Program execution** — Launch programs with positional parameter substitution (`{1}`, `{2}`, …).
- **INI-based configuration** — Human-readable config files with `[Include]` support for splitting shortcuts across multiple files.
- **Constants** — Define reusable `{{CONSTANT}}` placeholders in `[Constants]` sections.
- **Environment variable expansion** — Use `{{ENV:VARNAME}}` placeholders in targets.
- **Binary caching** — Configs are compiled to a binary cache and only re-parsed when source INI files change.
- **OSD overlay** — A brief, transparent on-screen popup confirms which shortcut was activated.
- **Fuzzy suggestions** — Mistyped aliases produce closest-match suggestions using Levenshtein distance.
- **Shell integration installer** — `jc --install` sets up CMD (DOSKEY macro), PowerShell (`$PROFILE` function), PATH, and the `JUMPS` environment variable. `jc --uninstall` reverses all changes.
- **Total Commander integration** — Optional panel switching via `--tc-panel=L|R` during install.
- **Alias listing** — `jc --list` displays all defined aliases and their targets.
- **Dual executables** — `j.exe` (Windows subsystem, silent + OSD) and `jc.exe` (console, interactive output).
- **Portable** — Statically linked with no runtime dependencies.
