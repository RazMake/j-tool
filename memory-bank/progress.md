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
- [x] Comprehensive test suite (~53 tests)
- [x] Coverage gate (85% threshold)

## What's Left to Build
- Nothing planned — v1.0 is complete

## Change Log
| Date | Change |
|------|--------|
| 2026-05-11 | Memory bank created |
| 2026-05-11 | Added multi-word alias support (greedy longest-match in jump_main) |
