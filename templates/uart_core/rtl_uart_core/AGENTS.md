MIT License

Copyright (c) 2026 Rovshan Rustamov

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

# AGENTS.md

## Purpose

This repository contains synthesizable RTL. It may be either:

- a reusable RTL IP repository, or
- a system-wide RTL integration repository that composes multiple reusable RTL dependencies.

Agents working in this repo must treat the documentation under `./docs` as the source of truth before editing RTL, wrappers, scripts, constraints, or build files.

This file is intentionally generic and copyable across RTL repositories. Project-specific architecture, project classification, module names, source-file names, interfaces, parameters, dependency lists, system composition, and behavior belong in the project docs, not in this file.

---

## Project classification

Read `./docs/design_guide.md` before deciding how the repository should be structured or built. The design guide must identify the project type near its beginning.

Recommended project-type values:

```text
reusable_ip
system_integration
```

Meaning:

- `reusable_ip`: a reusable RTL block intended to be instantiated or imported by larger RTL projects.
- `system_integration`: a system-wide or board-level RTL project that owns integration wrappers, top-level composition, clocks/resets, routing, and connections between reusable RTL dependencies.

Do not infer the project type only from the repository name. Apply project-type-specific rules only after reading the design guide.

If the project classification is missing or conflicts with the repository structure, flag the mismatch. Do not silently rewrite the project around an assumption.

---

## Required reading order

Before making implementation or structural changes, read:

```text
./docs/design_guide.md
./docs/rtl_design_guide.md
```

Use `./docs/design_guide.md` for project classification, project-specific architecture, required modules, source-file split, interfaces, parameters, address maps, protocol behavior, wrapper policy, external dependency list, dependency order, system composition, and implementation details.

Use `./docs/rtl_design_guide.md` for RTL coding style. Follow it strictly.

Also inspect the current repository tree before editing, especially:

```text
./src
./ext
./CMakeLists.txt
./scripts/cmake/rtl_dependency_helpers.cmake        if present
./scripts/cmake/questa_modules.cmake                if present
./scripts/questa/**/*.cmake                         if present
./scripts/vivado/read_sources.tcl                   if present
```

For a reusable IP, `./scripts/cmake/questa_modules.cmake` is normally its local RTL export/import manifest.

For a system-integration project, local Questa source ownership may instead be defined in subsystem scripts such as:

```text
./scripts/questa/sys/sim_questa_init_sys.cmake
./scripts/questa/sys/sim_questa_compile_sys.cmake
./scripts/questa/sys/sim_questa_run_sys.cmake
./scripts/questa/sys/sim_questa_run_sys_gui.cmake
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

Keep source-file names, helper modules, wrappers, top-level modules, and build lists synchronized. If a local module is added under `./src`, update every relevant source list that compiles or imports that RTL.

Do not leave stale source references copied from older projects. If a file does not exist under this repository or is not part of the current design, remove it from build/import lists unless the user explicitly requests otherwise.

Source ordering matters. List packages and low-level helpers before modules that use them, cores before wrappers, and top-level wrappers last unless the local design guide says otherwise.

### Reusable-IP source ownership

For a `reusable_ip` project:

- `./src` contains RTL owned by that reusable IP.
- External reusable RTL remains under `./ext`.
- The local `RTL_SOURCES` list contains only this repository's owned RTL unless the design guide explicitly requires a flattened build.
- `./scripts/cmake/questa_modules.cmake` normally exports only this repository's owned RTL to parent projects.

### System-integration source ownership

For a `system_integration` project:

- `./src` contains system-local integration RTL, board wrappers, system wrappers, clock/reset blocks, routing wrappers, and top-level composition owned by this repository.
- Reusable child IP remains under `./ext/<dependency>`.
- Do not copy child-IP RTL into the system project's `./src`.
- Do not manually duplicate child-IP source-file paths inside the system project's local source list when the dependency already provides a standard import manifest.
- Local system RTL must compile after all external libraries it depends on are available.

---

## CMakeLists.txt requirements

Keep `./CMakeLists.txt` and any included CMake manifest scripts synchronized with the real repository structure.

Project-specific variables such as the RTL top module name, simulation top, paired simulation module name, logical library names, source lists, and build paths must match the current project and the project docs. Do not preserve copied names from another project.

When new local source files are created, update the ordered local RTL source list used by the relevant flow. The order should be dependency-safe:

```text
packages/common helpers
core or integration modules
wrappers
board/system top-level modules
```

If the project defines a Verilator lint source list such as:

```cmake
set(VERILATOR_SOURCES ${RTL_SOURCES})
```

then keep that lint source mechanism ordered and complete for the intended lint scope.

For a reusable IP, source and dependency setup may live directly in the root `CMakeLists.txt`.

For a system-integration project, the root `CMakeLists.txt` may define shared paths and include subsystem-specific manifests under `./scripts/questa/<subsystem>`. In that case, edit the subsystem manifest that owns the simulation top and dependency libraries instead of forcing all details into the root file.

---

## External RTL submodule dependency policy

RTL dependencies are reusable RTL repositories already checked out under `./ext` by the user.

The agent must not add or fetch submodules on its own. Unless the user explicitly asks for repository-management work, do not:

- run `git submodule add`,
- edit `.gitmodules`,
- initialize or update submodules,
- change submodule revisions,
- commit changes inside dependency repositories.

When the user says dependencies are already imported under `./ext`, inspect them and wire them into the build only.

Standard dependency helper:

```text
./scripts/cmake/rtl_dependency_helpers.cmake
```

Standard dependency export/import file inside each dependency:

```text
./ext/<dependency>/scripts/cmake/questa_modules.cmake
```

Use the plural filename `questa_modules.cmake` for the standard flow. Do not introduce new singular `questa_module.cmake` references in new RTL repositories.

The standard helper intentionally requires the plural filename. If an older dependency exposes only:

```text
scripts/cmake/questa_module.cmake
```

then it is a legacy dependency that has not yet been migrated to the standard interface. Do not silently bypass the helper, create an alias inside the submodule, or rename a file inside the dependency unless the user explicitly asks for that migration. Report the mismatch. A working manual include may remain temporarily until the dependency repository itself provides `questa_modules.cmake`.

Before changing build files for external dependencies:

- Inspect `./ext` and confirm each named dependency directory exists.
- Confirm each dependency is already checked out and contains real RTL sources.
- Inspect each dependency's `scripts/cmake/questa_modules.cmake` file.
- Inspect what CMake variables the dependency import file expects, especially its logical library variable and whether it appends commands to `QUESTA_INIT_COMMANDS`.
- Read `./docs/design_guide.md` for the required dependency list, dependency purpose, and compile/import order.
- If the design guide does not mention a requested dependency, update the guide or flag the mismatch.

External dependency wiring rules:

- Keep this repository's owned RTL under `./src`.
- Do not copy external submodule RTL files into `./src`.
- Do not add external RTL files directly to this repository's owned local `RTL_SOURCES` list unless the design guide explicitly requires a flattened or vendor-import build.
- Import standardized dependencies through `rtl_import_module(<dependency>)` or `rtl_import_modules(...)` from `./scripts/cmake/rtl_dependency_helpers.cmake`.
- Import dependencies before compiling local RTL that uses them.
- Order dependencies from lowest-level helpers to shared/core IP and then higher-level wrappers.
- Define all logical-library variables expected by dependency import files before calling `rtl_import_module(s)`.
- Create and map external Questa logical libraries before executing `${QUESTA_INIT_COMMANDS}`.
- Add required external logical libraries to local `vlog` and `vsim` commands with `-L <library>`.
- Keep local system or IP wrappers compiled after dependency libraries are available.
- Do not hardcode copied library names from another repository. Derive names from the current dependency import files and `./docs/design_guide.md`.

If `./scripts/cmake/rtl_dependency_helpers.cmake` does not exist and the user asks to wire standardized `./ext` dependencies, create it from the standard template helper before editing the build flow. The helper must validate `EXT_DIR`, check dependency directories, require `scripts/cmake/questa_modules.cmake`, avoid duplicate imports, and include dependency import files in caller scope.

---

## Reusable-IP dependency wiring

For a `reusable_ip` project, the common pattern is:

```cmake
set(SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")
set(EXT_DIR "${CMAKE_CURRENT_LIST_DIR}/ext")

include("${CMAKE_CURRENT_LIST_DIR}/scripts/cmake/rtl_dependency_helpers.cmake")

set(WORK_LIB "work")
set(RTL_XXX_LIB "rtl_xxx")
set(RTL_YYY_LIB "rtl_yyy")

set(WORK_LIB_DIR "${QUESTA_SIM_DIR}/${WORK_LIB}")
set(RTL_XXX_LIB_DIR "${QUESTA_SIM_DIR}/${RTL_XXX_LIB}")
set(RTL_YYY_LIB_DIR "${QUESTA_SIM_DIR}/${RTL_YYY_LIB}")

set(QUESTA_INIT_COMMANDS)

rtl_import_modules(
  rtl_xxx
  rtl_yyy
)
```

Then the Questa initialization target must:

1. create the local and external logical libraries,
2. map those logical libraries,
3. execute `${QUESTA_INIT_COMMANDS}` to compile external dependencies,
4. compile this repository's local RTL after the dependencies.

The exact source list, dependency list, library variables, and import order must come from the local design guide and dependency import files.

---

## System-integration dependency wiring

The underlying import mechanism is the same as for a reusable IP, but a system-integration project normally has additional subsystem orchestration.

A typical root structure is:

```text
CMakeLists.txt
scripts/
  cmake/
    rtl_dependency_helpers.cmake
  questa/
    sys/
      sim_questa_init_sys.cmake
      sim_questa_compile_sys.cmake
      sim_questa_run_sys.cmake
      sim_questa_run_sys_gui.cmake
```

### Root `CMakeLists.txt`

The root file should define shared source roots and include the dependency helper before including a subsystem initialization script that calls `rtl_import_module(s)`:

```cmake
set(SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/src")
set(EXT_DIR "${CMAKE_CURRENT_LIST_DIR}/ext")

include("${CMAKE_CURRENT_LIST_DIR}/scripts/cmake/rtl_dependency_helpers.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/scripts/questa/sys/sim_questa_init_sys.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/scripts/questa/sys/sim_questa_compile_sys.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/scripts/questa/sys/sim_questa_run_sys.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/scripts/questa/sys/sim_questa_run_sys_gui.cmake")
```

Do not directly include individual dependency `questa_modules.cmake` files from the root or subsystem script once those dependencies use the standard helper interface.

### System initialization manifest

The subsystem initialization file, such as `sim_questa_init_sys.cmake`, is the simulation manifest for that system top. It should own:

- the system simulation top,
- logical library names,
- physical library directories,
- ordered dependency imports,
- library creation and mapping,
- external compile-command execution,
- ordered compilation of local system RTL,
- initial `vmake` makefile generation.

Recommended dependency-import pattern inside the initialization manifest:

```cmake
# Logical libraries and corresponding *_LIB_DIR variables are defined above.

set(QUESTA_INIT_COMMANDS)

rtl_import_modules(
  rtl_low_level_common
  rtl_protocol_core
  rtl_higher_level_ip
  rtl_other_dependency
)
```

For the current AXKU5 Ethernet system, once every dependency exposes the standard plural import file, the ordered import block is:

```cmake
rtl_import_modules(
  rtl_mac_1g_common
  rtl_mac_core_1g
  rtl_mac_1g
  rtl_axis_loopback
  rtl_axis_rt
)
```

The helper only collects the dependency-provided compile commands. It does not automatically create libraries, map libraries, add `-L` switches, generate `vmake` files, or update run targets. Those remain responsibilities of the system project's Questa scripts.

Inside the initialization target, preserve this order:

```text
create logical libraries
map logical libraries
execute ${QUESTA_INIT_COMMANDS}
compile local system RTL into WORK_LIB
run vmake for WORK_LIB and every external logical library
```

Local system RTL must remain local. Example shape:

```cmake
${QUESTA_INIT_COMMANDS}

COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR}
vlog -f ${VLOG_OPTIONS}
-L ${RTL_DEP_A_LIB}
-L ${RTL_DEP_B_LIB}
-work ${WORK_LIB}
${SRC_DIR}/local_wrapper.sv

COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR}
vlog -f ${VLOG_OPTIONS}
-L ${RTL_DEP_A_LIB}
-L ${RTL_DEP_B_LIB}
-work ${WORK_LIB}
${SRC_DIR}/system_top.sv
```

### System compile and run scripts

When a dependency is added or removed, synchronize all scripts that enumerate its logical library:

```text
sim_questa_init_<subsystem>.cmake
  dependency library variable
  dependency library directory
  rtl_import_modules(...) entry
  vlib command
  vmap command
  local vlog -L options
  initial vmake generation

sim_questa_compile_<subsystem>.cmake
  incremental make -f <library>.mak command

sim_questa_run_<subsystem>.cmake
  vsim -L <library> option

sim_questa_run_<subsystem>_gui.cmake
  vsim -L <library> option
```

Compile order must remain dependency-safe. Rebuild lower-level libraries before libraries that depend on them, and rebuild `WORK_LIB` last.

Do not put dependency import logic into the compile or run scripts. Dependency manifests are collected during CMake configuration by the initialization script; compile scripts consume generated makefiles, and run scripts elaborate the configured top.

### Multiple system manifests

The standard helper tracks imported module names globally during one CMake configuration. Do not import the same dependency independently into multiple separate `QUESTA_INIT_COMMANDS` lists during the same configure and assume each list will be populated.

If one repository has multiple independently configured system manifests that need the same dependency, use one documented shared import collection or separate configuration paths/build directories. Do not silently work around the helper's duplicate-import protection.

### Synthesis and implementation source flow

A system-integration repository may also own synthesis source orchestration, for example:

```text
./scripts/vivado/read_sources.tcl
```

When an external RTL dependency is added or removed, keep the synthesis source-import list synchronized with the Questa dependency list when that dependency participates in synthesis. Prefer sourcing the dependency's own `scripts/vivado/read_sources.tcl` rather than copying its source-file list into the system project.

---

## Questa module import file

### Reusable IP or exportable integration block

If this repository is intended to be imported by a parent RTL project, keep:

```text
./scripts/cmake/questa_modules.cmake
```

synchronized with the RTL files owned by this repository under `./src`.

It should contain only this repository's local source files, in dependency order:

- packages/helpers before modules that use them,
- cores before wrappers,
- top-level wrappers last,
- no stale entries copied from another project,
- no `./ext/<dependency>/src/*.sv` paths.

Use source paths relative to the script location, normally `${CMAKE_CURRENT_LIST_DIR}/../../src/<file>.sv`.

### Top-level system project

A top-level `system_integration` repository does not require its own `./scripts/cmake/questa_modules.cmake` unless `./docs/design_guide.md` says the system itself is exported to a larger parent build.

If the system is not exported, local system RTL may be compiled directly by its subsystem initialization script, such as `sim_questa_init_sys.cmake`. Do not create a local export manifest merely because reusable child dependencies have one.

---

## Lint and verification policy

Do not run RTL simulations unless the user explicitly tells you to run a specific simulation command.

Do not invent a simulator command or construct a temporary testbench. Use only the configured project target or exact command explicitly requested by the user.

After RTL/source-list changes, run the project Verilator lint target when a configured CMake build directory and valid lint source configuration are available. Inspect `CMakeLists.txt` for the exact lint target and command style.

When lint reports issues:

- Fix clear syntax, elaboration, missing-module, bad-port, illegal-width, and definite RTL errors.
- Do not get overly aggressive chasing every warning into a design rewrite.
- If a warning appears intentional, harmless, tool-style-related, or caused by an incomplete external wrapper context, it can be left unresolved and reported.
- Never claim lint passed unless the command actually completed successfully.
- If the lint source list is empty or does not include required dependencies, report that lint was not meaningfully configured rather than claiming success.

---

## Documentation synchronization

When changing interfaces, parameters, file names, pipeline semantics, address maps, register behavior, protocol behavior, wrapper policy, dependency lists, dependency order, top-level hierarchy, clock/reset architecture, or system routing, update the relevant docs under `./docs`.

At minimum, keep the applicable files aligned:

```text
./docs/design_guide.md
./docs/rtl_design_guide.md
./CMakeLists.txt
./src/*.sv
./scripts/cmake/questa_modules.cmake               when this repo exports local RTL
./scripts/questa/**/*.cmake                        when this repo owns system manifests
./scripts/vivado/read_sources.tcl                  when synthesis sources change
```

Do not duplicate large sections of the design guide inside code comments. Keep code comments useful and local; keep architecture and integration behavior in `./docs/design_guide.md`.

---

## Agent behavior

- Inspect before editing.
- Determine the project classification before applying reusable-IP or system-integration rules.
- Make focused changes only.
- Do not import, fetch, update, or revise git submodules unless explicitly requested.
- Do not introduce vendor block-design flows, generated black boxes, or unrelated dependencies.
- Do not add simulation-only code to synthesizable RTL.
- Do not create temporary testbenches unless explicitly requested.
- Preserve existing naming and style conventions unless the docs require a change.
- Prefer readable, timing-friendly RTL over clever compact logic.
- If a reusable helper owned by this repository is needed, create it under `./src` and wire it into every applicable local build/export list.
- If a requested change conflicts with the design guide, call out the conflict.
- If a dependency exposes only a legacy singular Questa import filename, report the migration requirement rather than silently changing dependency contents.

---

## Before finishing a task

Report:

- files changed,
- project classification used,
- whether docs were consulted or updated,
- whether `CMakeLists.txt` was updated,
- whether `scripts/cmake/questa_modules.cmake` was updated or not applicable,
- whether subsystem Questa scripts were updated,
- whether synthesis source scripts were updated,
- whether external RTL dependencies under `./ext` were wired into CMake/Questa,
- whether any dependency still uses legacy singular `questa_module.cmake`,
- whether Verilator lint was run and the exact result,
- whether RTL simulation was not run because it was not requested,
- any remaining risks, assumptions, or warnings left intentionally unresolved.
