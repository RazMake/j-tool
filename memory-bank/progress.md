# Progress

## What Works
- [x] INI parsing with section/key lookup, whitespace trimming, keyless entries, error handling
- [x] Binary cache with save/load and mtime-based freshness validation
- [x] Config loading with include processing, constant merging, encoding detection
- [x] Template expansion (`{{CONSTANT}}` and `{{ENV:VAR}}`)
- [x] Config validation (duplicate aliases and constants)
- [x] Alias resolution with case-insensitive lookup and multi-word alias support
- [x] Parameter substitution for EXEC shortcuts (`{1}`, `{2}`, …)
- [x] Fuzzy suggestions via Levenshtein distance
- [x] OSD overlay (transparent, auto-closing)
- [x] Total Commander integration (detection, navigation, process tree walk)
- [x] Shell integration installer/uninstaller (CMD, PowerShell, PATH)
- [x] Alias listing (`--list`)
- [x] Dual executables (`j.exe` GUI, `jc.exe` console)
- [x] Static linking (portable, no runtime deps)
- [x] Comprehensive test suite (~59 tests, 6→7 test modules)
- [x] Coverage gate (85% threshold)
- [x] EXEC shortcut support for .cmd/.bat batch files (cmd.exe /c wrapper)\n- [x] Diagnostic logging (`--log` one-shot flag, `%TEMP%\\jump.log`)

## What's Left to Build
- Nothing planned — v1.0 is complete

## Change Log
| Date | Change |
|------|--------|
| 2026-05-21 | Added `--log` diagnostic logging: one-shot flag file + `%TEMP%\jump.log` with tagged IDs (JMP/CFG/CAC/RES/TC_) |
| 2026-05-11 | Memory bank created |
| 2026-05-11 | Added multi-word alias support (greedy longest-match in jump_main) |
| 2026-05-11 | Fixed .cmd/.bat EXEC shortcuts: CreateProcessA can't run batch files directly, added cmd.exe /c wrapper with exec_needs_cmd_wrapper() |
| 2026-05-27 | Increased MAX_ALIASES_PER_SHORTCUT from 8 to 20 — real-world INI files had up to 14 aliases per shortcut, causing silent truncation and wrong alias resolution |
| 2026-05-27 | Bumped CACHE_VERSION to 2 (binary cache layout changed due to Shortcut struct size increase) |
