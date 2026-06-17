# Active Context

## Current State
- Project is at v1.0 (initial release)
- All core features implemented and working
- Test suite comprehensive (~53 unit tests across 6 test modules)
- Coverage gate set at 85%

## Recent Changes
- setup_dev.ps1 MSVC workload fix (2026-06-17) — the VS step now verifies MSVC C++ headers (via `Test-MsvcHeaders`, mirroring `build_env.ps1`) instead of only checking for `vswhere.exe`. When headers are missing it installs the `visualstudio2022-workload-vctools` package, so a Build Tools install lacking the C++ workload is repaired automatically instead of failing later with "No Visual Studio installation with MSVC C++ headers found."
- CI version fix (2026-06-13) — `ci.yml` no longer hard-codes `1.0.` as the major.minor; both version steps now parse `MAJOR.MINOR` from `CMakeLists.txt` and append `github.run_number` as the patch. Previously a `1.1.0` bump shipped as `1.0.31`.
- Diagnostic logging (2026-05-21) — `--log` flag enables one-shot logging to `%TEMP%\jump.log` for debugging
- Memory bank created (2026-05-11) — comprehensive project documentation
- Multi-word alias support (2026-05-11) — `jump_main()` tries longest multi-word alias first, falls back to single word
- .cmd/.bat EXEC fix (2026-05-11) — batch file shortcuts now wrapped with `cmd.exe /c` since `CreateProcessA` cannot execute them directly
- MAX_ALIASES_PER_SHORTCUT raised to 20 (2026-05-27) — real-world INI files exceeded the old limit of 8, causing aliases to be silently dropped and wrong shortcuts to resolve
- CACHE_VERSION bumped to 2 (2026-05-27) — struct layout change requires cache rebuild
- Win+R Run dialog CD support (2026-05-27) — when no console is attached, CD shortcuts open a new cmd.exe window at the target path

## Current Focus
- Hardening `setup_dev.ps1` so a bare/partial Windows machine reaches a working build (auto-install MSVC C++ workload when missing)

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
