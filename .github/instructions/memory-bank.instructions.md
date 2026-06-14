---
applyTo: "**"
---

# Memory Bank Instructions

This project maintains a **memory bank** in the `memory-bank/` folder at the repository root. You MUST follow these rules for every task.

## At the Start of Every Task

1. Read ALL memory bank files before doing any work:
   - `memory-bank/projectbrief.md` — project purpose and scope
   - `memory-bank/productContext.md` — user workflow, UX decisions
   - `memory-bank/systemPatterns.md` — architecture, modules, data structures
   - `memory-bank/techContext.md` — build system, dependencies, scripts
   - `memory-bank/activeContext.md` — current state and focus
   - `memory-bank/progress.md` — what works, what's left, change log

2. Use this context to inform your work. Do not ask questions that are already answered in the memory bank.

## When the User Requests Changes

Before completing any change request, always ask the user:

1. Whether the **version of the tool needs to be incremented** (see [Versioning](#versioning)).
2. Whether the change needs to be **recorded in `CHANGELOG.md`**.

## After Every Code Change

1. **Always update** `memory-bank/activeContext.md`:
   - Update "Current Focus" to reflect what was just done
   - Update "Recent Changes" with a brief summary

2. **Always update** `memory-bank/progress.md`:
   - Add an entry to the "Change Log" table with date and description
   - Update "What Works" or "What's Left to Build" if applicable

3. **Always update** `CHANGELOG.md`:
   - Record every behavior-affecting change under the `## Unreleased` section (create it if missing)
   - Summarize the change in behavior (what the user can now do / what changed), not the implementation detail

4. **Update other files when relevant**:
   - `systemPatterns.md` — when architecture, modules, data structures, or patterns change
   - `techContext.md` — when build config, dependencies, or tooling changes
   - `productContext.md` — when user-facing behavior or UX decisions change
   - `projectbrief.md` — when project scope or features change

## Versioning

The single source of truth for the version is `CMakeLists.txt` (`project(Jump VERSION X.Y.Z ...)`),
which flows into `src/core/version.h.in` → generated `version.h` at configure time. To bump the version:

1. Update `project(Jump VERSION X.Y.Z LANGUAGES C)` in `CMakeLists.txt` (or run `release.ps1 -Version X.Y.Z`).
2. Rename the `## Unreleased` heading in `CHANGELOG.md` to `## vX.Y` so accumulated entries become release notes.
3. Update the `## Current Version` line in `memory-bank/projectbrief.md`.
4. Do **not** hand-edit the generated `build/generated/version.h`; it is regenerated on the next configure.

## Memory Bank Content Guidelines

- Include decisions that **cannot be reverse-engineered from code alone** (rationale, rejected alternatives, timing/ordering choices, UX reasoning)
- Keep entries concise — bullet points and tables over prose
- Record limitations and known issues as they are discovered
- Track version changes and migration notes
