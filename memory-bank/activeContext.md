# Active Context

## Current State
- Project is at v1.3.1
- All core features implemented and working
- Test suite comprehensive (~53 unit tests across 6 test modules)
- Coverage gate set at 85%

## Recent Changes
- CI coverage installation (2026-10-19) - replaced Chocolatey with the pinned official OpenCppCoverage 0.9.9.0 x64 installer. The failed run received HTTP 503 from Chocolatey's feed and installed nothing despite a successful step status. CI now checks the installer exit code and executable, then exports its runner-temp directory through GITHUB_PATH. Version bumped to 1.3.1 and release notes recorded with user approval.
- Configuration capacity (2026-10-19) - increased from 64 to 300 sections per file and from 256 to 280 total shortcuts at the user's request. Version bumped to 1.3.0 with approval. Tests cover both boundaries and caching 280 shortcuts. Test configs moved to the heap because the larger JumpConfig exceeds the default Windows stack size. Cache format stays v2: serialization stores active records, not the full fixed-size array.
- j.exe shell-integration message (2026-06-17) — when `j <cd-alias>` runs from a terminal where the `j` function/DOSKEY macro is NOT loaded, `j.exe` (GUI subsystem, no console of its own) used to spawn a stray `cmd.exe`. It now calls `AttachConsole(ATTACH_PARENT_PROCESS)`: if the parent owns a console it attaches and prints a "shell integration not active" message (target + fix hint) instead of popping a window. Win+R Run dialog / Explorer (no parent console) still open a new cmd window. Root cause observed: a user's PowerShell profile shim keyed on `$env:COMPUTERNAME` never dot-sourced the real profile, so the `j` function was never defined.
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
- Restoring reliable CI coverage setup without weakening the 85% coverage gate.
- Validation: official installer download verified; workflow script passed simulated success, download-failure, installer-failure, and missing-executable checks (including paths with spaces). Rebuilt both executables; `jc --version` reports 1.3.1. All 123 tests passed under OpenCppCoverage with 87.7% coverage. Hosted CI still needs a run after pushing.

## Known Limitations
| Limitation | Notes |
|-----------|-------|
| Max 280 shortcuts, 64 constants | Hard-coded limits in types.h |
| Max 300 sections per INI file | Includes special sections; fixed parser storage uses about 19.4 MiB per IniFile |
| Single-pass template expansion | No nested `{{...}}` support |
| No circular include detection | Include chains not validated |
| OSD timeout hard-coded at 1.5s | Not configurable |
| Binary cache format v1 only | No migration strategy for format changes |
| Windows-only | No Unix/macOS support |

## Open Questions
- None currently documented
