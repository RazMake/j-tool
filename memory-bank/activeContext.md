# Active Context

## Current State
- Project is at v1.0 (initial release)
- All core features implemented and working
- Test suite comprehensive (~53 unit tests across 6 test modules)
- Coverage gate set at 85%

## Recent Changes
- Diagnostic logging (2026-05-21) — `--log` flag enables one-shot logging to `%TEMP%\jump.log` for debugging
- Memory bank created (2026-05-11) — comprehensive project documentation
- Multi-word alias support (2026-05-11) — `jump_main()` tries longest multi-word alias first, falls back to single word
- .cmd/.bat EXEC fix (2026-05-11) — batch file shortcuts now wrapped with `cmd.exe /c` since `CreateProcessA` cannot execute them directly
- MAX_ALIASES_PER_SHORTCUT raised to 20 (2026-05-27) — real-world INI files exceeded the old limit of 8, causing aliases to be silently dropped and wrong shortcuts to resolve
- CACHE_VERSION bumped to 2 (2026-05-27) — struct layout change requires cache rebuild

## Current Focus
- Bug fix: aliases beyond 8th position silently dropped, now supports up to 20

## Known Limitations
| Limitation | Notes |
|-----------|-------|
| Max 256 shortcuts, 64 constants | Hard-coded limits in types.h |
| Single-pass template expansion | No nested `{{...}}` support |
| No circular include detection | Include chains not validated |
| OSD timeout hard-coded at 1.5s | Not configurable |
| Binary cache format v1 only | No migration strategy for format changes |
| Windows-only | No Unix/macOS support |

## Open Questions
- None currently documented
