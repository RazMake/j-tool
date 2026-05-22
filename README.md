# J-Tool (aka. Jump)

A fast, lightweight shortcut tool for Windows. It allows the user to define shortcuts that are invoked with `j <shortcut>` to perform the action.  
Possible actions are:  
- Change to a specific folder (_on any drive on the computer_)
- Open a specific URL with the default browser
- Start a program (_with a specific command line_)

## Features

- **INI-based configuration** — Human-readable config with `[Include]` support to split shortcuts across multiple files (_for easier management_).
- **Constants & environment variables** — Use `{{CONSTANT}}` and `{{ENV:VARNAME}}` placeholders in targets.
- **Binary caching** — Configs are compiled to a binary cache and only re-parsed when source files change.
- **OSD overlay** — A brief, transparent on-screen popup confirms which shortcut was activated.
- **Fuzzy suggestions** — Mistype an alias and Jump suggests the closest matches (_Levenshtein distance_).
- **Shell integration** — Automatic setup for CMD (DOSKEY macro), PowerShell (`$PROFILE` function), and _**Total Commander**_ panel switching.
- **Portable** — Statically linked, no runtime dependencies.

## Usage

Two executables are produced:

| Binary | Subsystem | Purpose |
|--------|-----------|---------|
| `j.exe` | Windows | Silent operation + OSD display (no console window) |
| `jc.exe` | Console | Interactive use, prints output to the terminal |

```
j  <alias> [params...]        Resolve alias and perform action
jc <alias> [params...]        Same, with console output
jc --install [--tc-panel=L|R] Install shell integration
jc --uninstall                Remove shell integration
jc --list                     List all defined aliases
jc --update                   Pulls the latest version from the GitHub repository.
jc --log                      Enable diagnostic logging for the next command
j  --osd "text"               Show OSD overlay (internal)
```

## Configuration

Set the `JUMPS` environment variable to the path of your root INI file. The installer (`jc --install`) will prompt for this if it is not already set.

### INI format

It is possible to split the config into a machine specific file and then general one. This would help for multi machine configs:
- Create one INI file for each machine (stored locally), which define the specific locations as constants.
- Create files for the common jumps in a shared location (_i.e. in OneDrive_) and use the constants in those files.  

Then on each machine the jumps (shortcuts) will work the same way, even if there are slight differences in the folder configuration.

```ini
; Root config (or machine specific config)
[Constants]
PROJECTS=C:\Projects
TOOLS=D:\Tools

; These files contain specific jumps
[Include]
work.ini
personal.ini
```

```ini
; work.ini
[Work Projects]
Label=Work Projects Folder
Jumps=work,w
Path={{PROJECTS}}\Work

[Company Site]
Label=Company Website
Jumps=company,comp
Open=https://company.example.com

[Editor]
Label=Open Editor
Jumps=edit,ed
Execute={{TOOLS}}\editor.exe {1} {2}
```

Each section (other than `[Constants]` and `[Include]`) defines a shortcut:

| Key | Description |
|-----|-------------|
| `Jumps` | Comma-separated list of aliases (up to 8 per shortcut) |
| `Label` | Text shown in the OSD overlay and `--list` output |
| `Path` | Directory to `cd` into |
| `Open` | URL or file to open via the system handler |
| `Execute` | Command line to run. `{1}`, `{2}`, … are replaced with extra arguments. Supports `.exe`, `.cmd`/`.bat`, `.ps1`, and `.vbs` script targets (auto-wrapped in the appropriate host: `cmd.exe /c`, `pwsh`/`powershell -File`, or `cscript`/`wscript`). |
| `HideConsole` | EXEC-only. When `true` (or `yes`/`1`) the spawned process runs without a visible console window. For `.vbs` targets this also selects `wscript.exe` over `cscript.exe`. |

`{{CONSTANT}}` references are expanded from the merged `[Constants]` sections.
`{{ENV:VARNAME}}` references are expanded from environment variables.

## Debugging

Run `jc --log` to enable one-shot diagnostic logging. The **next** command you run will write a detailed log to `%TEMP%\jump_YYYYMMDD_HHMMSS.log` (timestamped), after which the flag is automatically cleared. This is useful for diagnosing configuration loading, cache, alias resolution, and action execution issues.

```
jc --log              # Enable logging (does NOT read config or cache)
j  myalias            # This run is logged to %TEMP%\jump_20260521_143022.log
j  myalias            # This run is NOT logged (flag was cleared)
```

The `--log` flag intentionally does **not** read config or touch the cache, so it can be used to debug first-run / bootstrap problems.

Every log line contains a unique identifier (e.g. `[JMP01]`, `[CFG03]`, `[CAC10]`) that maps to the exact call site in the source code.

| Prefix | Source file |
|--------|------------|
| `LOG` | `log.c` — logging infrastructure |
| `JMP` | `jump.c` — CLI dispatch & action execution |
| `CFG` | `config.c` — INI loading, expansion, validation |
| `CAC` | `cache.c` — binary cache read/write/freshness |
| `RES` | `resolver.c` — alias lookup & parameter substitution |
| `TC_` | `tc.c` — Total Commander integration |

## Developer Setup

### New machine (automated)

Run `setup_dev.ps1` from an **elevated (Administrator)** PowerShell prompt. It will:

1. Install prerequisites via [Chocolatey](https://chocolatey.org/) — Git, CMake, OpenCppCoverage, and Visual Studio Build Tools 2022 (skips any already installed).
2. Auto-detect the MSVC toolset and Windows SDK versions and generate `build_env.ps1`.
3. Configure the CMake project.
4. Build in Debug mode.
5. Run all tests.
6. Run the code coverage gate (85% on testable code).

### Prerequisites (manual)

If you prefer to set things up yourself, you need:

- **Visual Studio 2022** (or Build Tools) with the C++ desktop workload
- **CMake** ≥ 3.20
- **OpenCppCoverage** (`choco install opencppcoverage -y`) — for local coverage checks
- **Git**

## Building

Use `build_env.ps1` to set up the MSVC environment, then configure and build (by running both of the following commands in this order):

```
.\build_env.ps1 cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
.\build_env.ps1 cmake --build build
```

The test framework ([cmocka](https://cmocka.org/)) is fetched automatically during configuration.

### Running tests

```
.\build_env.ps1 ctest --test-dir build --output-on-failure
```

### Code coverage

Run `check_coverage.ps1` to measure line coverage and enforce the 85% gate:

```
.\check_coverage.ps1
```

The following files are excluded from coverage measurement:

| File | Reason |
|------|--------|
| `osd.c` | Win32 GUI code (creates a layered window, paints with GDI, runs a message loop) — cannot be exercised in a headless test harness. |
| `main.c` | Thin entry point (`main` → `jump_main`); no logic to test. |
| `main_win.c` | Thin entry point (`WinMain` → `jump_main`); no logic to test. |
| `jump.c` | Top-level dispatch: spawns processes, calls `ShellExecute`, writes temp files, and interacts with the console handle — all side-effects that require a live Windows session. |
| `install.c` | Modifies the registry, `$PROFILE`, `PATH`, and broadcasts `WM_SETTINGCHANGE` — destructive system-level side-effects unsuitable for unit tests. |
| `log.c` | File I/O to `%TEMP%` and flag-file management — platform glue not exercisable in the test harness. |

The same exclusions are applied in the CI pipeline.

## Installation

Run `jc --install` to set up shell integration. This:

1. Sets the `JUMPS` environment variable (if not already defined).
2. Creates a DOSKEY macro for CMD (`j` → runs `jc.exe` and executes a temp script).
3. Appends a PowerShell function to `$PROFILE`.
4. Adds the executable directory to `PATH`.
5. Broadcasts `WM_SETTINGCHANGE` so other applications pick up the new environment.

All operations are idempotent — safe to re-run. Use `jc --uninstall` to reverse.

## Project Structure

```
src/
  core/       Entry points (main.c, main_win.c) and dispatch logic (jump.c)
  config/     INI parsing, config loading & merging, binary cache
  resolve/    Alias resolution and fuzzy suggestion
  platform/   OSD overlay, Total Commander integration, install/uninstall
tests/        Unit tests (cmocka) with INI fixture files
```

## License

See repository for license details.
