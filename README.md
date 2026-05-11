# Jump

A fast, lightweight directory/URL/program launcher for Windows. Define aliases in INI files and jump to directories, open URLs, or launch programs with a single short command.

## Features

- **Directory navigation** — Type `j work` to `cd` into a mapped directory instantly.
- **URL opening** — Aliases can open websites in your default browser.
- **Program execution** — Launch programs with parameter substitution (`{1}`, `{2}`, …).
- **INI-based configuration** — Human-readable config with `[Include]` support to split shortcuts across multiple files.
- **Constants & environment variables** — Use `{{CONSTANT}}` and `{{ENV:VARNAME}}` placeholders in targets.
- **Binary caching** — Configs are compiled to a binary cache and only re-parsed when source files change.
- **OSD overlay** — A brief, transparent on-screen popup confirms which shortcut was activated.
- **Fuzzy suggestions** — Mistype an alias and Jump suggests the closest matches (Levenshtein distance).
- **Shell integration** — Automatic setup for CMD (DOSKEY macro), PowerShell (`$PROFILE` function), and Total Commander panel switching.
- **Portable** — Statically linked, no runtime dependencies.

## Usage

```
j  <alias> [params...]        Resolve alias and perform action
jc <alias> [params...]        Same, with console output
jc --install [--tc-panel=L|R] Install shell integration
jc --uninstall                Remove shell integration
jc --list                     List all defined aliases
j  --osd "text"               Show OSD overlay (internal)
```

Two executables are produced:

| Binary | Subsystem | Purpose |
|--------|-----------|---------|
| `j.exe` | Windows | Silent operation + OSD display (no console window) |
| `jc.exe` | Console | Interactive use, prints output to the terminal |

## Configuration

Set the `JUMPS` environment variable to the path of your root INI file. The installer (`jc --install`) will prompt for this if it is not already set.

### INI format

```ini
; Root config
[Constants]
PROJECTS=C:\Projects
TOOLS=D:\Tools

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
| `Execute` | Command line to run. `{1}`, `{2}`, … are replaced with extra arguments |

`{{CONSTANT}}` references are expanded from the merged `[Constants]` sections.
`{{ENV:VARNAME}}` references are expanded from environment variables.

## Building

Requires CMake ≥ 3.20 and an MSVC toolchain (C11).

Use `build_env.bat` to set up the MSVC environment (it calls `vcvars64.bat`), then configure with the NMake generator:

```
build_env.bat cmake -G "NMake Makefiles" -S . -B build
build_env.bat cmake --build build
```

The test framework ([cmocka](https://cmocka.org/)) is fetched automatically during configuration.

### Running tests

```
build_env.bat cmake --build build --target j_tests
cd build
build_env.bat ctest --output-on-failure
```

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
