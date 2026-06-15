# AGENTS.md

## Purpose

This repository contains a synthesizable RTL IP block. Agents working in this repo must treat the documentation under `./docs` as the source of truth before editing RTL, wrappers, scripts, or build files.

This file is intentionally generic and copyable across RTL IP repositories. Project-specific architecture, module names, source-file names, interfaces, parameters, and behavior belong in the project docs, not in this file.

---

## Required reading order

Before making implementation or structural changes, read:

```text
./docs/design_guide.md
./docs/rtl_design_guide.md
```

Use `./docs/design_guide.md` for project-specific architecture, required modules, source-file split, interfaces, parameters, address maps, protocol behavior, wrapper policy, and implementation details.

Use `./docs/rtl_design_guide.md` for RTL coding style. Follow it strictly.

Also inspect the current repo tree before editing, especially:

```text
./src
./CMakeLists.txt
./scripts/cmake/questa_modules.cmake
```

If code and docs disagree, do not silently choose the code. Flag the mismatch and either update the code to match the guide or update the guide if the guide is stale.

---

## RTL coding rules

Follow `./docs/rtl_design_guide.md` strictly. Key reminders:

- Use SystemVerilog 2012.
- Put `` `default_nettype none `` at the top of RTL files.
- Do not restore `` `default_nettype wire `` at the end.
- Use `input wire` for module inputs.
- Use `logic` for outputs and internal signals.
- Use only `always_ff`, `always_comb`, and continuous `assign`.
- Do not use plain `always` or `always @(*)`.
- Use synchronous reset for `always_ff` logic unless the design guide explicitly requires otherwise.
- Code FSMs as single-process synchronous FSMs only.
- FSMs must use symbolic enum states and include a safe default branch.
- Do not add SVA assertions unless the user explicitly asks.
- Keep RTL synthesizable. Do not add testbench constructs, delays, or `initial` blocks.
- Keep edits surgical and readable. Do not rewrite unrelated files.

---

## Source-file policy

Create or update source files according to `./docs/design_guide.md`.

Keep source-file names, helper modules, wrappers, and build lists synchronized. If a helper module is added under `./src`, update every relevant source list that compiles or imports RTL.

Do not leave stale source references copied from older IPs. If a file does not exist under this repository or is not part of the current IP, remove it from build/import lists unless the user explicitly requests otherwise.

Source ordering matters. List reusable helpers before modules that instantiate them, wrappers after the core, and top-level wrappers last unless the local design guide says otherwise.

---

## CMakeLists.txt requirements

Keep `./CMakeLists.txt` synchronized with the actual RTL sources in `./src`.

Project-specific variables such as the RTL top module name, VIP/design name, library name, and source list must match the current IP and the project docs. Do not preserve copied names from another project.

When new source files are created, update the ordered RTL source list used by CMake. The order should be dependency-safe:

```text
common/helper modules
core modules
wrappers/top-level modules
```

If the project defines a Verilator lint source list such as:

```cmake
set(VERILATOR_SOURCES ${RTL_SOURCES})
```

then keep `RTL_SOURCES` ordered and complete so the `lint` target checks the intended RTL files.

---

## Questa module import file

Keep `./scripts/cmake/questa_modules.cmake` synchronized with the RTL files in `./src`.

This file is used when the IP is imported into a larger project. It should contain only the real source files for this IP, in dependency order:

- helper modules before modules that instantiate them
- core modules before wrappers
- no stale source entries copied from another project

Use source paths relative to the script location, normally `${CMAKE_CURRENT_LIST_DIR}/../../src/<file>.sv`.

Inspect `./CMakeLists.txt` for the correct local work-library variable and source naming convention. Do not hardcode library names or source files from another IP.

## Lint and verification policy

Do not run RTL simulations unless the user explicitly tells you to run a specific simulation command.

After RTL/source-list changes, run the project Verilator lint target when a configured CMake build directory is available. Inspect `CMakeLists.txt` for the exact lint target and command style.

When lint reports issues:

- Fix clear syntax, elaboration, missing-module, bad-port, illegal-width, and definite RTL errors.
- Do not get overly aggressive chasing every warning into a design rewrite.
- If a warning appears intentional, harmless, tool-style-related, or caused by an incomplete external wrapper context then it can be ignored.
- Never claim lint passed unless the command actually completed successfully.
---

## Documentation synchronization

When changing interfaces, parameters, file names, pipeline semantics, address-map behavior, register behavior, protocol behavior, or wrapper policy, update the relevant docs under `./docs`.

At minimum, keep these aligned:

```text
./docs/design_guide.md
./docs/rtl_design_guide.md
./CMakeLists.txt
./scripts/cmake/questa_modules.cmake
./src/*.sv
```

Do not duplicate large sections of the design guide inside code comments. Keep code comments useful and local; keep architectural details in `./docs/design_guide.md`.

---

## Agent behavior

- Inspect before editing.
- Make focused changes only.
- Do not introduce vendor block-design flows, generated black boxes, or unrelated dependencies.
- Do not add simulation-only code to RTL.
- Do not create temporary testbenches unless explicitly requested.
- Preserve existing naming/style conventions unless the docs require a change.
- Prefer readable, timing-friendly RTL over clever compact logic.
- If a reusable helper module is needed, create it under `./src` and wire it into CMake and Questa import lists.
- If a requested change conflicts with the design guide, call out the conflict.

---

## Before finishing a task

Report:

- files changed,
- whether docs were consulted or updated,
- whether `CMakeLists.txt` was updated,
- whether `scripts/cmake/questa_modules.cmake` was updated,
- whether Verilator lint was run and the result,
- whether RTL simulation was not run because it was not requested,
- any remaining risks, assumptions, or warnings left intentionally unresolved.
