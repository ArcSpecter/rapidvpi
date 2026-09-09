# VIP refactor execution specification

## Native-HDL clock control, scheduler-portable VIP behavior, and dual Questa/Verilator plugin builds

This file is an implementation specification for refactoring one existing `vip_<project>` repository at a time. The target state is the architecture proven in `vip_mac_1g`, generalized for the clocks, interfaces, agents, and build target of the current project.

The objective is to change infrastructure, not verification intent:

- externally supplied DUT clocks are generated natively by the corresponding RTL project's simulation-only `dut_wrapper.sv`;
- project verification controls those clocks through `vip_common::Clock::NativeClockCfg`;
- protocol agents synchronize to the real HDL clock waveforms but do not generate wrapper-owned clocks;
- the same VIP source and complete testcase set compile into a plugin for the existing Questa flow and into a separate Verilator-hosted plugin;
- project-specific `ext/vip_*` modules are audited and, only where necessary, minimally corrected to follow `docs/vip_design_guide.md`;
- testcase behavior, coverage, ordering, stimulus, expectations, timeouts, iteration counts, and scoreboards remain unchanged.

Treat this as an execution task. Inspect the repository, make the required changes, build both plugin flavors, and report exact results. Do not stop after producing a plan.

### Cross-project determinism mandate

This is a replication task, not an architecture-design task. Every project refactored with this file shall use the same `vip_mac_1g` infrastructure pattern. Codex must not invent an alternative implementation merely because it is functionally equivalent.

Only these project-specific substitutions are allowed:

| Category | Allowed to vary |
|---|---|
| Project identity | existing `${PROJECT_NAME}`, logical name, current library output name when already explicitly configured |
| Project sources | existing `TEST_SOURCES`, testcase `.cpp` list, used `add_subdirectory()` list |
| Existing dependencies | project-specific libraries already linked in the original build |
| Clock inventory | actual wrapper-owned clock names, exact documented `sim_*` triplets, number of independent clocks, mode-dependent presence |
| Existing public glue | established `Test` member names that testcases already use |
| Protocol-specific remediation | only the minimum internal agent fix proven necessary by the Section 9 audit |

These infrastructure choices are fixed and shall not vary:

- cache variable name `VIP_VPI_FLAVOR`;
- flavor values `EXISTING` and `VERILATOR`;
- selected include variable name `vpi_include_dir`;
- Verilator discovery and fallback block shown in Section 8.2;
- existing-flow VPI header contract through `VPI_INCLUDE_DIR`;
- external standard VPI link guarded by `VIP_VPI_FLAVOR STREQUAL "EXISTING"`;
- separate build directory `cmake-build-verilator`;
- convenience target name `verilator_<logical_name>`;
- one four-argument `vip::common::Clock` construction per wrapper-owned independent input clock;
- exact 1/64/1 registration of each enable/period/stopped triplet;
- direct use of the documented clock-control triplet in `NativeClockCfg`;
- project-owned `core::finishSimulation()` in runner `after_all`.

Do not replace the canonical snippets with a helper `.cmake` module, CMake function/macro, preset-only solution, automatic simulator detector, differently named option, clock factory, clock-manager abstraction, subclass of `vip_common::Clock`, or project-local backend wrapper.

If the canonical structure is technically incompatible with a project, stop and report the exact incompatibility. Do not silently develop a third implementation. An exception requires explicit user approval and must then be added back to this common specification if it is intended for more than one project.

---

## 1. Sources of truth and precedence

Before changing code, read these files completely when they exist:

1. repository `AGENTS.md` and any applicable nested `AGENTS.md` files;
2. `docs/vip_design_guide.md`;
3. the project-local DUT/user guide, normally `docs/user_guide.md`;
4. the root `CMakeLists.txt` and every included CMake file;
5. `src/pindefs.hpp`, `src/init.cpp`, `src/test.hpp`, and `src/test.cpp`;
6. the CMake and public documentation of project-specific `ext/vip_*` modules selected for audit below.

Use the project-local DUT/user guide to obtain the `dut_wrapper` clock ownership map and exact `sim_*` names. Do not guess clock ownership from signal names and do not copy the `mac_1g` clock list into another project.

The corresponding RTL refactor is a prerequisite. This task does not authorize changes to a sibling `rtl_*` repository. The target VIP repository must already document a simulation top named `dut_wrapper` that exposes native clock control/status nets. If that contract is absent, incomplete, or ambiguous, stop and report that the RTL wrapper refactor must be completed first.

---

## 2. Non-negotiable scope boundaries

Make only changes required for native clock integration, dual-flavor plugin compilation, simulation completion, and directly relevant scheduler-portability defects in project-specific reusable VIP modules.

Do not:

- modify DUT RTL or any sibling `rtl_*` repository;
- modify `flow_mgmt`, its project registry, or its synchronization/update scripts;
- fetch, update, initialize, add, remove, or retarget Git submodules unless explicitly requested separately;
- commit or push anything;
- discard, overwrite, or reformat pre-existing user changes;
- refactor, optimize, modernize, clean up, rename, or reorganize unrelated code or APIs;
- change a testcase merely to make the DUT or a simulator pass;
- change testcase stimulus, expected values, iteration counts, stress seeds, timing intent, timeout budgets, coverage, or case order;
- remove testcase `.cpp` files from CMake or remove/comment `register_tc_*()` calls;
- add simulator-name branches, scheduler-specific offsets, or tiny arbitrary delays;
- create project-local replacements for `vip_common`, `vip_axis`, `vip_axil`, `vip_gmii`, or `vip_rgmii`;
- replace the canonical Section 8 CMake structure with a different but allegedly equivalent implementation;
- create a new CMake helper module, preset, function, macro, or auto-detection layer for this migration;
- wrap or subclass `vip_common::Clock` through a new project-local clock abstraction;
- run an RTL simulation from the VIP repository.

The normal `.so` must continue to contain the complete existing testcase implementation and registration set. Test selection remains an execution-plan concern only.

If the worktree is dirty, preserve all unrelated changes. Inspect `git status --short` before editing and again at handoff.

---

## 3. Required final state

The refactor is complete only when all applicable statements are true:

1. The RapidVPI DUT name is `dut_wrapper`, not the synthesizable DUT module.
2. Every wrapper-owned external/free-running DUT input clock has exactly one project-owned `vip::common::Clock` object using the four-argument native constructor.
3. Every such clock uses its real waveform net, a unique RapidVPI task name, and its matching enable/period/stopped triplet.
4. The actual waveform net and all three control/status nets are registered in `initNets()` with widths `1`, `64`, and `1` for the triplet.
5. DUT-generated output clocks are observation-only and have no `Clock` controller.
6. No testcase or protocol agent directly toggles a wrapper-owned clock.
7. No duplicate/manual `clk_run()` task registration or testcase launch remains.
8. Existing testcase clock-control calls continue through the same public project API wherever possible; the general `clk` controller should retain an existing member name such as `clock` if tests already use it.
9. `sim_keepalive`, when documented by the RTL wrapper, is not registered, read, written, or used by VIP code.
10. The existing/Questa plugin build remains the default and preserves its current include/link environment.
11. A `VERILATOR` build flavor obtains Verilator's `vpi_user.h`, compiles the same source graph, and does not link the external standard `vpi` host library.
12. The Verilator plugin is configured and built in a separate build tree so its cache cannot contaminate the existing flow.
13. Every object library that directly or indirectly includes VPI headers receives the selected `${vpi_include_dir}`.
14. Project-local runtime cleanup calls `core::finishSimulation()` once from the runner `after_all` hook, after any existing final cleanup.
15. Every applicable project-specific `ext/vip_*` module outside the exclusion list passes the audit in Section 9, or its directly relevant defects are minimally corrected and documented.
16. Both plugin flavors compile successfully without deleting tests, weakening checks, or adding simulator-specific C++ behavior.
17. The root CMake uses the canonical Section 8 structure with no unapproved alternative variable names, flavor values, helper modules, presets, auto-detection, or build-tree naming.
18. Cross-project differences are limited to the allowed substitution table in the determinism mandate.

---

## 4. Preflight and baseline capture

### 4.1 Record repository state

Run read-only inventory first:

```bash
git status --short
git submodule status
rg --files -g 'AGENTS.md' -g 'CMakeLists.txt' -g '*.cmake'
rg --files src ext docs
```

Do not attempt to clean a dirty tree. Identify which existing changes overlap the intended files and work around them carefully.

### 4.2 Identify exact project placeholders

Determine and record these values from the repository; do not invent them:

| Placeholder | Meaning |
|---|---|
| `<VIP_TARGET>` | Exact existing shared-library CMake target; it must not be renamed |
| `<LOGICAL_NAME>` | Project name without the `vip_` prefix, used for `verilator_<logical_name>` |
| `<DUT_TOP>` | Must become `dut_wrapper` for this architecture |
| `<EXISTING_BUILD_DIR>` | Existing configured build tree used by the known-good plugin flow |
| `<VERILATOR_BUILD_DIR>` | Fixed separate tree: `cmake-build-verilator` |
| `<PROJECT_CLOCK>` | One wrapper-owned external DUT input clock |
| `<CLOCK_TASK>` | Unique long-lived RapidVPI task name for that clock |
| `<CLOCK_ENABLE>` | Matching `sim_<clock>_enable` net |
| `<CLOCK_PERIOD>` | Matching 64-bit `sim_<clock>_period_ticks` net |
| `<CLOCK_STOPPED>` | Matching `sim_<clock>_stopped` net |

### 4.3 Preserve a behavioral membership baseline

Before editing, capture:

```bash
rg -n 'register_tc_[A-Za-z0-9_]+' src
rg -n 'tc_[A-Za-z0-9_]+\.cpp' src -g 'CMakeLists.txt'
rg -n '(set_plan|emplace_back|set_plan_tagged)' src
```

The same normal testcase sources and registrations must remain after the refactor. Do not change the plan for a compile-only migration.

If the existing plugin already builds, record the exact successful command and target before changes. A baseline compile failure that is unrelated to this task is not authorization to repair unrelated code.

---

## 5. Build the project clock-ownership inventory

Create an internal working table before editing:

| Clock net | Direction at synthesizable DUT | Wrapper ownership | Independently controllable | Control triplet | VIP action |
|---|---|---|---|---|---|
| project-specific | input/output | wrapper/DUT/protocol | yes/no | exact names or N/A | `Clock`, observe only, or protocol-owned |

Classify every clock-like signal into exactly one of these categories.

### 5.1 Wrapper-owned external/free-running DUT input clock

Use this classification only when:

- the synthesizable DUT receives the clock as an input;
- the simulation wrapper generates it internally;
- verification may start, stop, change, or rephase it;
- the wrapper exposes a matching `sim_*_enable`, `sim_*_period_ticks`, and `sim_*_stopped` triplet.

Required action: create one native `vip_common::Clock` controller in the project `Test` object.

### 5.2 DUT-generated output clock

Use this classification when the synthesizable DUT produces the clock.

Required action: observe its actual edges through `CommonUtils` or a monitor. Do not create a `Clock`, do not invent a `sim_*` group, and do not drive it.

### 5.3 Transaction-generated protocol clock

Some protocols contain a transaction clock that is intentionally generated by a protocol initiator, such as an SPI-like serial clock. Do not blindly move every signal containing `clk` into `vip_common::Clock`.

If the protocol clock is not a wrapper-owned free-running DUT clock and the protocol specification makes the initiator responsible for it, it may remain protocol-agent-owned. Document this classification. The agent must still follow the safe write/sample/retirement rules in `vip_design_guide.md`.

### 5.4 Mutually exclusive build-mode clocks

If compile-time configuration selects one of several mutually exclusive wrapper clocks:

- register only nets that exist in the elaborated mode;
- use the same typed compile-time configuration already used by project glue;
- an active-mode `std::unique_ptr<vip::common::Clock>` is acceptable;
- do not register absent-mode control groups;
- do not change the build-mode selection or testcase applicability.

### 5.5 Stop on ambiguity

If ownership, direction, clock-control names, widths, time scale, or build-mode presence cannot be proved from project documentation and existing wrapper integration, stop and request the missing RTL/user-guide contract. Do not infer it from `mac_1g`.

---

## 6. Refactor project clock wiring

The project `Test` owns clock controllers. Reusable protocol agents normally receive or name the real waveform clock only.

Use the literal construction/registration pattern in this section, repeated mechanically for the number of clocks in the current project. Do not introduce a factory, loop-driven registry, map-based clock manager, derived class, new backend-selection flag, or project-local wrapper around `vip::common::Clock`. The only permitted structural variation is `std::unique_ptr<vip::common::Clock>` for a genuinely compile-time-selected clock whose net does not exist in other modes.

### 6.1 Select the wrapper as the VPI top

Update project glue so RapidVPI scopes registered short net names beneath:

```cpp
setDutName("dut_wrapper");
```

Preserve the project's current method/constant style. Do not rename unrelated top-level signals. The actual clocks and `sim_*` controls may be internal VPI-visible nets beneath `dut_wrapper`; they do not need to be external wrapper ports.

### 6.2 Add typed pin-name constants

In `src/pindefs.hpp`, define the exact project-specific names for each wrapper-controlled clock:

```cpp
inline constexpr char clk[] = "clk";
inline constexpr char sim_clk_enable[] = "sim_clk_enable";
inline constexpr char sim_clk_period_ticks[] = "sim_clk_period_ticks";
inline constexpr char sim_clk_stopped[] = "sim_clk_stopped";
```

Repeat for every independently controlled external clock using the names documented by the project. Preserve established typed-constant style and existing public names used by tests. Do not introduce `#define` macros for ordinary C++ configuration.

Do not add `sim_*` constants for DUT-generated output clocks.

### 6.3 Register all four nets per native clock

In `src/init.cpp` / `initNets()`, register:

```cpp
addNet(clk, 1);
addNet(sim_clk_enable, 1);
addNet(sim_clk_period_ticks, 64);
addNet(sim_clk_stopped, 1);
```

The actual waveform clock remains mandatory because agents synchronize to its HDL-generated edges even though native `Clock` never writes it.

For every active clock group, verify:

- actual clock: width 1;
- enable: width 1;
- full period: width 64;
- stopped status: width 1.

Register only the active compile-time interface where mutually exclusive nets do not coexist.

Never register `sim_keepalive` as part of normal VIP code.

### 6.4 Own one controller per independent external clock

In `src/test.hpp`, include the existing `vip_common` clock header and own one controller per always-present wrapper clock:

```cpp
vip::common::Clock clock;
vip::common::Clock tx_clock;
vip::common::Clock rx_clock;
```

Names above are illustrative. Preserve existing member names used by testcases. In particular, if testcases use `test.clock`, keep that API unless a change is unavoidable.

Use a dynamic owner only for a genuinely mode-dependent clock:

```cpp
std::unique_ptr<vip::common::Clock> mode_clock;
```

Do not put `Clock` objects inside individual testcases or reusable protocol VIPs.

### 6.5 Construct the native backend

In `src/test.cpp`, change each old three-argument controller to the four-argument native constructor:

```cpp
, clock(*this,
        clk,
        "clk_run",
        vip::common::Clock::NativeClockCfg{
            .enable_net = sim_clk_enable,
            .period_ticks_net = sim_clk_period_ticks,
            .stopped_net = sim_clk_stopped,
        })
```

For additional clocks:

```cpp
, tx_clock(*this,
           tx_clk,
           "tx_clk_run",
           vip::common::Clock::NativeClockCfg{
               .enable_net = sim_tx_clk_enable,
               .period_ticks_net = sim_tx_clk_period_ticks,
               .stopped_net = sim_tx_clk_stopped,
           })
```

Each task name must be unique for the lifetime of the `Test` object.

Use the deterministic task naming rule:

```text
actual clock `clk`       -> task `clk_run`
actual clock `<name>`    -> task `<name>_run`
```

Preserve an already published task name only when changing it would break existing project code. Do not invent stylistic alternatives such as `<name>_clock_task`, `run_<name>`, or generated numeric task names.

The `Clock` constructor registers its long-lived task. Remove any separate/manual registration or testcase launch of the same `clk_run()` task. Do not remove unrelated agent task registration.

### 6.6 Preserve the public testcase API

Both clock backends expose the same control calls:

```cpp
co_await test.clock.start<test::ns>(period_ns);
co_await test.clock.stop();
co_await test.clock.wait_stopped();
co_await test.clock.stop_and_wait();
co_await test.clock.set_period<test::ns>(period_ns);
co_await test.clock.start_after<test::ns>(period_ns, delay_ns);
co_await test.clock.start_at<test::ns>(period_ns, first_rise_tick);
```

Do not rewrite testcase calls just because the backend changed. The migration changes where repetitive waveform edges are generated, not the scenario expressed by the testcase.

Preserve these semantics:

- `start()` requests operation and does not promise the caller has already observed a first edge;
- `stop()` requests a stop but is not a physical park-low barrier;
- `wait_stopped()` observes only;
- `stop_and_wait()` provides the deterministic stopped/parked barrier;
- `set_period()` does not re-anchor phase;
- deterministic rephase requires `stop_and_wait()` followed by `start_at()` or `start_after()`;
- `is_running()` reports requested state, not proof of current physical edges.

Do not rephase clocks between tests merely to hide a scheduler-sensitive driver defect.

### 6.7 Keep reset and cleanup clocks running

All destination clocks required by synchronous reset, reset CDC, graceful disable, quiescence, or acknowledgement logic must run while those operations progress.

Do not stop required clocks before between-case cleanup. Preserve existing reset sequencing and clock periods. A stress testcase that stops/rephases clocks remains responsible for restoring the clocks needed by later cleanup, with its original intent unchanged.

### 6.8 Ensure end-of-run completion

The project layer, not a testcase or reusable agent, owns simulation completion. Ensure the runner has one `after_all` hook that preserves current final cleanup and calls `core::finishSimulation()` last:

```cpp
runner.set_after_all_hook([this]() {
    // Preserve the project's existing final cleanup here.
    core::finishSimulation();
});
```

Adapt the body to the existing project; do not invent cleanup. If no cleanup exists:

```cpp
runner.set_after_all_hook([]() {
    core::finishSimulation();
});
```

Include `core.hpp` if required. Do not call raw `vpi_control()`. Do not place the finish request in `after_case`, a `tc_*`, `runner.set_plan()`, `vip_common::Runner`, or another reusable VIP.

### 6.9 Timeprecision contract

The standard wrapper uses:

```systemverilog
timeunit 1ns;
timeprecision 1ps;
```

Its `sim_*_period_ticks` therefore represents 1 ps units. `vip_common::Clock` forwards RapidVPI raw ticks into the 64-bit period control, so the effective VPI precision must represent the same duration.

Do not add per-test conversions. Preserve the runner's startup precision report and require the later simulator qualification log to show the documented precision, normally:

```text
vpi_precision_exp10=-12
tick_unit=1ps
```

A mismatch is an infrastructure/launch error, not a testcase failure. The VIP repository should report it; it must not modify RTL launch scripts under this task.

---

## 7. Remove competing clock generation without changing protocol intent

Search project glue, testcases, and agents for direct writes or generators associated with wrapper-owned clocks:

```bash
rg -n '(registerTest|clk_run|clock_run|drive_clk|drive_clock|toggle|period_ticks)' src ext
rg -n 'getCoWrite|\.write\(' src ext
rg -n '(clk|clock|rxc|txc|sclk)' src ext
```

Manually correlate each hit with the clock-ownership table. Text matching alone is not proof of a defect.

For every wrapper-owned clock:

- remove old per-edge VPI generation from project-local code;
- remove duplicate/manual task registration;
- configure any reusable protocol driver not to drive the clock;
- keep the driver synchronized to the actual HDL waveform;
- keep its data/control timing, queueing, ticket, and scoreboard behavior unchanged.

Prefer an existing additive agent setting when available. The `vip_mac_1g` RGMII pattern is illustrative:

```cpp
Driver::TimingCfg cfg;
cfg.drive_rxc = false;
driver.set_timing_cfg(cfg);
```

Do not copy this exact API into an unrelated module unless it matches that module's design. If a project-specific reusable VIP always drives a clock that must now be wrapper-owned and offers no external-clock mode, add the smallest additive configuration needed to disable clock driving while preserving existing users. The project must select external-clock mode explicitly.

Do not remove a protocol agent's legitimate transaction-generated clock solely because its name contains `clk`.

---

## 8. Add dual existing/Verilator plugin compilation

The build flavor selects the VPI header/link host environment only. It must not select different testcase code, protocol behavior, source membership, or simulator-specific scheduling logic.

### 8.0 Canonical CMake replication rule

Sections 8.1 through 8.5 are the canonical cross-project implementation. Copy their structure into every VIP root `CMakeLists.txt` and substitute only:

1. the existing `${PROJECT_NAME}` value;
2. `<logical_name>` in the convenience target;
3. existing project source/subdirectory lists;
4. existing project-specific libraries that must remain linked in both flavors;
5. an already configured non-default library output name, if the project had one before this refactor.

After replacing those allowed project tokens, the infrastructure blocks should be line-for-line comparable across repositories. Differences in indentation or surrounding legacy CMake are acceptable; differences in the flavor logic, discovery flow, variable names, guards, build-tree name, or target structure are not.

The following are not permitted substitutions:

- another cache variable name;
- `QUESTA`/`VVP`/`AUTO` or additional flavor values;
- another Verilator build-directory name;
- another convenience-target naming scheme;
- a CMake preset instead of the target;
- a separate top-level CMake file for Verilator;
- environment-based automatic flavor selection;
- compiler definitions that make C++ source behavior simulator-dependent;
- moving the logic into a shared helper module as part of this per-project task.

Place the flavor-selection and `vpi_include_dir` resolution block after the project's basic `project()`/C++ setup and before any `add_library()` or `add_subdirectory()` whose sources need VPI headers. Place the conditional external VPI link beside the existing RapidVPI link. Place the `verilator_<logical_name>` custom target after the complete shared-library composition.

If the existing CMake cannot accept this literal structure for a proven technical reason, stop and report it. Do not implement an alternative.

### 8.1 Preserve the current build as the default

At the root of the VIP `CMakeLists.txt`, define a cache selection whose default is the current flow:

```cmake
set(VIP_VPI_FLAVOR "EXISTING" CACHE STRING
        "VPI build flavor: EXISTING or VERILATOR")
set_property(CACHE VIP_VPI_FLAVOR PROPERTY STRINGS EXISTING VERILATOR)

if (NOT VIP_VPI_FLAVOR STREQUAL "EXISTING" AND
        NOT VIP_VPI_FLAVOR STREQUAL "VERILATOR")
    message(FATAL_ERROR
            "Unsupported VIP_VPI_FLAVOR='${VIP_VPI_FLAVOR}'; expected EXISTING or VERILATOR.")
endif ()
```

Use `EXISTING`, not `QUESTA`, because this branch preserves the repository's established `VPI_INCLUDE_DIR` and standard VPI host-link contract without unnecessarily encoding simulator behavior into C++.

Do not change the project's C++ standard, compiler flags, warning policy, RapidVPI package target, or unrelated libraries as part of this migration.

### 8.2 Resolve Verilator's standard VPI header

Use the installed `verilator` executable as the source of `VERILATOR_ROOT`, with the proven fallback used by `vip_mac_1g`:

```cmake
if (VIP_VPI_FLAVOR STREQUAL "VERILATOR")
    find_program(VERILATOR_EXECUTABLE NAMES verilator REQUIRED)

    execute_process(
            COMMAND "${VERILATOR_EXECUTABLE}" --getenv VERILATOR_ROOT
            RESULT_VARIABLE _verilator_getenv_result
            OUTPUT_VARIABLE VERILATOR_ROOT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
    )

    if (NOT _verilator_getenv_result EQUAL 0 OR
            NOT EXISTS "${VERILATOR_ROOT}/include/vltstd/vpi_user.h")
        execute_process(
                COMMAND "${VERILATOR_EXECUTABLE}" -V
                RESULT_VARIABLE _verilator_version_result
                OUTPUT_VARIABLE _verilator_version_output
                ERROR_VARIABLE _verilator_version_error
        )
        set(_verilator_version_text
                "${_verilator_version_output}\n${_verilator_version_error}")
        string(REGEX MATCH
                "VERILATOR_ROOT[ \t]*=[ \t]*([^\r\n]+)"
                _verilator_root_match
                "${_verilator_version_text}")
        set(VERILATOR_ROOT "${CMAKE_MATCH_1}")
        string(STRIP "${VERILATOR_ROOT}" VERILATOR_ROOT)
    endif ()

    if (NOT EXISTS "${VERILATOR_ROOT}/include/vltstd/vpi_user.h")
        message(FATAL_ERROR
                "Could not find Verilator's standard VPI header under VERILATOR_ROOT='${VERILATOR_ROOT}'.")
    endif ()

    set(vpi_include_dir "${VERILATOR_ROOT}/include/vltstd" CACHE PATH
            "Path to the VPI include directory" FORCE)
    message(STATUS "Verilator VPI header: ${vpi_include_dir}/vpi_user.h")
else ()
    set(vpi_include_dir "$ENV{VPI_INCLUDE_DIR}" CACHE PATH
            "Path to the VPI include directory")

    if (NOT vpi_include_dir)
        message(FATAL_ERROR
                "VPI_INCLUDE_DIR environment variable must be set or provided manually.")
    endif ()
endif ()
```

Integrate this exact resolution structure into the existing root CMake rather than duplicating a second top-level build definition. Replace any project-local alternative Verilator VPI-header selection with this canonical block unless it is unrelated and used for a different target. Do not preserve a different implementation merely because it appears equivalent or more elaborate.

### 8.3 Propagate the selected include directory to every object target

The shared-library target's private includes do not automatically compile independently declared object libraries. Every object target containing project cases, `vip_common`, or another compiled `vip_*` component must include:

```cmake
target_include_directories(${OBJECT_TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/ext
        ${vpi_include_dir}
)
```

Preserve its existing RapidVPI include paths and local include directories. Add only missing paths.

Each object target contributing to the plugin must retain:

```cmake
set_property(TARGET ${OBJECT_TARGET}
        PROPERTY POSITION_INDEPENDENT_CODE ON)

target_sources(${PROJECT_NAME} PRIVATE
        $<TARGET_OBJECTS:${OBJECT_TARGET}>)
```

Do not add unused `ext/vip_*` modules merely because they exist in the tree. Keep the current source graph and `add_subdirectory()` membership unless a module is already used but accidentally omitted.

### 8.4 Select the correct host-link contract

Preserve the project's RapidVPI link in both flavors:

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE rapidvpi::rapidvpi.vpi)
```

Preserve project libraries such as compression or utility libraries in both flavors.

Link the existing external standard VPI library only for the existing flavor:

```cmake
if (VIP_VPI_FLAVOR STREQUAL "EXISTING")
    target_link_libraries(${PROJECT_NAME} PRIVATE vpi)
endif ()
```

The guard structure above is fixed. If the pre-refactor project already spells the external VPI library target differently, the only permitted substitution is replacing the token `vpi` inside that same guard with the exact existing target/library token. Do not change the guard, flavor values, or link ownership.

The Verilator host executable supplies the standard VPI implementation when it loads the plugin. Do not force-link the existing external VPI host library into the Verilator-flavor `.so`.

### 8.5 Build Verilator flavor in a separate tree

Add the custom target only to non-Verilator configurations so it does not recursively create itself:

```cmake
if (NOT VIP_VPI_FLAVOR STREQUAL "VERILATOR")
    set(VERILATOR_BUILD_DIR
            "${CMAKE_SOURCE_DIR}/cmake-build-verilator")

    add_custom_target(verilator_<logical_name>
            COMMAND "${CMAKE_COMMAND}"
                    -S "${CMAKE_SOURCE_DIR}"
                    -B "${VERILATOR_BUILD_DIR}"
                    -G "${CMAKE_GENERATOR}"
                    -DCMAKE_BUILD_TYPE:STRING=Release
                    -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
                    -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
                    -DVIP_VPI_FLAVOR:STRING=VERILATOR
            COMMAND "${CMAKE_COMMAND}"
                    --build "${VERILATOR_BUILD_DIR}"
                    --target ${PROJECT_NAME}
                    --parallel
            BYPRODUCTS
                    "${VERILATOR_BUILD_DIR}/lib${PROJECT_NAME}.so"
            USES_TERMINAL
            VERBATIM
    )
endif ()
```

Replace `<logical_name>` with the repository's logical project name. For `PROJECT_NAME=vip_foo`, the convenience target must be `verilator_foo`, the separate directory must be `cmake-build-verilator`, and the byproduct must be:

```text
cmake-build-verilator/libvip_foo.so
```

Only when the project explicitly configured a different library output name before this refactor may the `BYPRODUCTS` filename use that existing name. Do not introduce a new output name or output directory. The Verilator build directory itself remains exactly `cmake-build-verilator`.

Do not configure `EXISTING` and `VERILATOR` alternately in the same CMake build tree.

---

## 9. Audit project-specific reusable VIP modules under `./ext`

### 9.1 Exclusion list

Do not rework these known shared modules as part of this per-project task:

```text
ext/vip_common
ext/vip_axis
ext/vip_axil
ext/vip_gmii
ext/vip_rgmii
```

They are the known-good shared baseline. Do not copy them from `vip_mac_1g` and do not edit them merely for style consistency. If the current project has a stale version that lacks required APIs such as `Clock::NativeClockCfg`, report the stale dependency instead of recreating the API locally.

Modules not present are irrelevant; do not add them.

### 9.2 Select audit candidates

Audit every existing `ext/vip_*` directory not in the exclusion list:

```bash
for d in ext/vip_*; do
    case "$d" in
        ext/vip_common|ext/vip_axis|ext/vip_axil|ext/vip_gmii|ext/vip_rgmii)
            continue
            ;;
    esac
    printf '%s\n' "$d"
done
```

Do not audit unrelated `ext/rtl_*` or non-VIP libraries under this task.

### 9.3 Audit policy

For each selected module, inspect its full compiled source graph, public agent headers, CMake, and timing/phase documentation. Do not judge only by filenames or a final build result.

Record a compact result table:

| Module | Clock ownership | Phase discipline | Handshake/retirement | CMake portability | Action |
|---|---|---|---|---|---|
| `ext/vip_xxx` | pass/fail/N/A | pass/fail | pass/fail/N/A | pass/fail | unchanged or exact files fixed |

### 9.4 Clock-ownership checks

Verify:

- the reusable VIP does not instantiate a project free-running clock controller;
- it does not directly toggle any clock classified as wrapper-owned;
- it accepts/uses the actual clock net for edge synchronization;
- a DUT-generated clock is sampled only;
- a legitimate protocol-generated transaction clock remains clearly owned by the protocol initiator;
- no clock is driven by both `vip_common::Clock` and a protocol agent;
- no testcase manually launches an agent's clock task;
- agent clock-driving modes, if any, are explicit configuration rather than hidden behavior.

If a module needs an external-clock mode, add it additively and preserve stable APIs where practical. Do not move project-specific clock names into a reusable VIP.

### 9.5 RapidVPI read/write phase checks

Treat all of these as observation/read-only boundaries:

```cpp
co_await test.getCoRead();
co_await test.getCoRead<test::ns>(...);
co_await test.getCoChange(...);
co_await utils.clock(...);
```

Verify that a later write is not claimed to affect the edge/time slot already observed.

Required rules:

- queue writes before awaiting the write object;
- use a fresh zero-delay `getCoWrite()` for each deliberate source-state update;
- after RO observation, move to the protocol's future legal drive phase before attributing the write to a sampling/acceptance edge;
- avoid bus-equality `getCoChange()` waits; sample buses and test required bits with masks;
- use numeric VPI access only for values up to 64 bits;
- use string access for wider vectors;
- do not add simulator-specific delays or callback-order branches;
- helper comments/API docs state whether the caller may invoke them from arbitrary phase or must already own the first legal post-edge write phase.

Preferred edge-sampled status pattern:

```cpp
while (true) {
    auto rd = test.getCoRead();
    rd.read(status_net);
    co_await rd;

    const auto status = rd.getNum(status_net);

    co_await utils.clock(1, 1);

    if ((status & DONE_MASK) != 0ULL) {
        break;
    }
}

auto wr = test.getCoWrite();
wr.write(command_net, 1);
co_await wr;
```

Adapt the future edge to the real protocol. The invariant is causality: a write must target a future legal phase, never the edge already observed.

### 9.6 Valid-ready source checks

For AXI/AXIS-like or other valid-ready sources, verify this exact semantic sequence:

1. cross a known active edge while the new beat/request is not yet presented;
2. drive all payload fields and VALID from one fresh post-edge write phase;
3. hold payload and VALID stable while stalled;
4. sample READY in the setup phase preceding the candidate accepting edge;
5. count `VALID && READY` only on the immediately following active edge;
6. retire or replace the accepted source state in the first legal post-handshake write phase;
7. never infer acceptance from READY observed later;
8. never clear VALID before the sampled accepting edge;
9. never add an unnecessary extra barrier that permits the terminal beat to be accepted twice.

Scheduler-portable source skeleton:

```cpp
co_await utils.clock(1, 1);

{
    auto wr = test.getCoWrite();
    write_payload(wr, payload);
    wr.write(valid_net, 1);
    co_await wr;
}

while (true) {
    co_await utils.clock(1, 0);

    auto rd = test.getCoRead();
    rd.read(ready_net);
    co_await rd;
    const bool ready = (rd.getNum(ready_net) & 1ULL) != 0ULL;

    co_await utils.clock(1, 1);

    if (ready) {
        break;
    }
}

{
    auto wr = test.getCoWrite();
    wr.write(valid_net, 0);
    co_await wr;
}
```

Do not mechanically paste this over a protocol with different sampling edges. Match its semantics to the protocol while preserving the invariant.

For AXI-Lite writes, AW and W acceptance are independent. Retire each accepted VALID separately while preserving the still-stalled channel. For B/R responses in a one-outstanding helper, capture VALID/payload before asserting READY, cross the known acceptance edge, then return READY low in the next legal write phase.

For a slave/responder that drives READY, verify the complementary contract:

- current-cycle acceptance is determined from the VALID and READY values that were actually present at the sampled active edge;
- the agent must not compute a new READY value and then retroactively use that new value to classify the edge already observed;
- process the current accepted transfer first, then compute `ready_next` for a future cycle;
- drive `ready_next` once from the first legal post-edge write phase established for that future cycle;
- keep READY stable through the candidate accepting edge;
- keep any `ready_applied` or equivalent software bookkeeping synchronized with the value actually written, not merely the requested policy;
- reset gating drives READY to its documented safe value and clears stall/bubble runtime state without creating a same-edge race;
- periodic, scripted, or random backpressure counts protocol cycles/accepted transfers, not coroutine wakeups, and random modes retain their existing deterministic seed behavior.

Monitors must count transfers only at real sampled `VALID && READY` edges and must never drive either side of the interface.

### 9.7 Non-valid-ready sampled protocol checks

For serial, byte-per-clock, DDR, GMII-like, or other sampled protocols:

- drive each symbol before its sampling edge;
- hold it through that edge;
- advance only in the legal post-sample write phase;
- retire the final symbol immediately in the first legal post-sample phase;
- do not add a second edge/barrier that duplicates the last symbol;
- make bit-center, rising/falling, or DDR-edge choice explicit;
- do not mark a ticket done before physical delivery and required idle retirement complete.

### 9.8 Completion and ticket checks

Verify that:

- queue insertion is not treated as physical completion;
- `done` cannot assert before the final required sample/handshake;
- where returning idle is part of completion, `done` does not precede the idle write;
- zero-gap chaining replaces the accepted terminal state only in the same legal post-edge update window and cannot duplicate it;
- exact-cycle tests use acceptance/sample timestamps, not enqueue or coroutine-wakeup time.

### 9.9 Explicit time-unit checks

Use current RapidVPI forms:

```cpp
test.getCoWrite();
test.getCoWrite<test::ns>(10.0);
test.getCoWrite<test::ticks>(10000);
test.getCoRead();
test.getCoRead<test::ns>(10.0);
rd.getTime<test::ticks>();
```

Do not introduce or retain old ambiguous forms in code being directly repaired:

```cpp
test.getCoWrite(0);
test.getCoRead(0);
test.getCoWrite(10.0);
rd.getTime();
```

Do not perform a repository-wide mechanical API rewrite outside the selected module and directly required project glue.

### 9.10 CMake checks for each selected module

Verify:

- object-target names are globally unique within the parent build;
- object targets list their actual `.cpp` sources;
- `${CMAKE_SOURCE_DIR}/ext` and `${vpi_include_dir}` reach compiled sources;
- position-independent code is enabled;
- objects contribute to `${PROJECT_NAME}`;
- the module does not build a second `vip_common`;
- the module does not create a simulator executable or launch target;
- no hardcoded Icarus/Questa/Verilator VPI include path bypasses `vpi_include_dir`;
- no module-local simulator flavor selects different protocol source or behavior.

### 9.11 Minimal remediation policy

When a selected module passes, leave it unchanged.

When it fails:

1. fix the reusable mechanism rather than weakening a testcase;
2. preserve public APIs where practical; prefer additive config/setter methods;
3. change only files required for the proven defect;
4. update that module's timing/ownership README only if its contract changes;
5. compile through the normal parent plugin target in both flavors;
6. report changes inside a Git submodule separately from parent-repository changes;
7. do not commit, push, fetch, or change the submodule pointer.

If a necessary correction would materially redesign the module or change stable behavior outside the current project's needs, stop and report the design decision instead of silently expanding scope.

---

## 10. Testcase transparency gate

The native clock backend should normally be transparent to `tc_*` code.

After editing, inspect:

```bash
git diff -- src/cases
```

Expected result: no testcase changes.

An exception is permitted only if old per-edge clock generation is literally implemented in a testcase or a project-local testcase helper. In that case:

- move clock mechanics behind the project-owned `Clock`/setup API;
- preserve the same periods, start/stop ordering, phase targets, and call-site intent;
- do not alter traffic, checks, budgets, expected outcomes, or plan membership;
- report every exceptional testcase/helper edit explicitly.

Do not rewrite tests to use simulator-specific behavior. Do not reduce CDC/phase stress. Independently startable/stoppable clocks must remain independently controllable for existing phase-stress cases.

---

## 11. Static verification before compiling

### 11.1 Verify the required `vip_common` API exists

```bash
rg -n 'NativeClockCfg|LegacyVpi|NativeHdl|stop_and_wait|start_at|start_after' \
    ext/vip_common/agents/clock
```

If it does not exist, report a stale `vip_common` dependency. Do not implement a project-local clone.

### 11.2 Verify native project wiring

For every clock group, correlate four locations:

| Contract item | `pindefs.hpp` | `initNets()` | `Test` ownership | Constructor |
|---|---|---|---|---|
| actual clock | exact constant | width 1 | controller or observer | real waveform net |
| enable | exact constant | width 1 | N/A | `enable_net` |
| period | exact constant | width 64 | N/A | `period_ticks_net` |
| stopped | exact constant | width 1 | N/A | `stopped_net` |
| task | N/A | N/A | one controller | globally unique name |

No row may be partially wired.

### 11.3 Search for forbidden direct clock writes

Search all hits and manually classify them:

```bash
rg -n '(getCoWrite|\.write\(|\.force\().*(clk|clock|rxc|txc|sclk)' src ext
rg -n '(clk|clock|rxc|txc|sclk).*(getCoWrite|\.write\(|\.force\()' src ext
```

Wrapper-owned clocks must have no project/test/agent waveform writes. Writes to `sim_*_enable` and `sim_*_period_ticks` belong inside `vip_common::Clock`, not project testcases.

### 11.4 Verify no duplicate task registration

```bash
rg -n '(registerTest|register_tasks|clk_run|clock_run)' src ext
```

Each project `Clock` constructor owns exactly one unique task. Do not manually register or launch it elsewhere.

### 11.5 Verify complete testcase membership

Repeat the baseline commands:

```bash
rg -n 'register_tc_[A-Za-z0-9_]+' src
rg -n 'tc_[A-Za-z0-9_]+\.cpp' src -g 'CMakeLists.txt'
rg -n '(set_plan|emplace_back|set_plan_tagged)' src
```

Compare with the captured baseline. Any difference must be reverted unless explicitly required by the user's request.

---

## 12. Compile verification

This repository workflow compiles VIP plugins only. Do not launch Questa, Verilator RTL simulation, or any sibling RTL scripts.

### 12.1 Existing/Questa-side plugin

Prefer the repository's already configured build directory and known command:

```bash
cmake --build <EXISTING_BUILD_DIR> --target <VIP_TARGET> --parallel
```

If no configured build tree exists, configure it using the repository's established generator, compilers, toolchain, and environment. A generic fallback is:

```bash
cmake -S . -B cmake-build-release \
    -DCMAKE_BUILD_TYPE=Release \
    -DVIP_VPI_FLAVOR=EXISTING

cmake --build cmake-build-release \
    --target <VIP_TARGET> \
    --parallel
```

Do not overwrite a user's differently configured build tree.

### 12.2 Verilator-hosted plugin

From the existing configured tree, build the convenience target:

```bash
cmake --build <EXISTING_BUILD_DIR> \
    --target verilator_<logical_name> \
    --parallel
```

This must configure and build the separate tree with:

```text
VIP_VPI_FLAVOR=VERILATOR
```

The expected artifact is normally:

```text
cmake-build-verilator/lib<VIP_TARGET>.so
```

For diagnosis, the equivalent direct commands are:

```bash
cmake -S . -B cmake-build-verilator \
    -DCMAKE_BUILD_TYPE=Release \
    -DVIP_VPI_FLAVOR=VERILATOR

cmake --build cmake-build-verilator \
    --target <VIP_TARGET> \
    --parallel
```

Do not treat unresolved standard VPI symbols in a loadable plugin as a reason to link the existing external `vpi` host library into the Verilator flavor; the Verilator host supplies that implementation when loading the plugin. Build/link errors must be diagnosed from the actual target and dependency graph.

### 12.3 Build-result requirements

Both builds must:

- compile the same project glue, testcase sources, and reusable VIP object sources;
- produce the same logical plugin target;
- differ only in the selected VPI header/host-link environment and output tree;
- retain all existing project libraries in both flavors;
- show no missing `vpi_user.h`, duplicate target, non-PIC relocation, or accidental external `vpi` link error;
- require no source-level `#ifdef VERILATOR` behavior branch.

If a build fails, fix only errors introduced by or directly blocking this refactor. Report unrelated pre-existing failures without broad cleanup.

---

## 13. Downstream simulation qualification handoff

Do not execute these simulations from the VIP workflow. Provide the following requirements to the project owner or the separate RTL simulation workflow:

1. Elaborate `dut_wrapper`, not the synthesizable DUT top.
2. Confirm the startup precision report matches the wrapper tick scale, normally 1 ps.
3. Run the smallest focused testcase that exercises the relevant clocks/protocol agent on Verilator.
4. Run the unchanged complete applicable regression on Verilator.
5. Run the same unchanged complete applicable regression on Questa.
6. Compare semantic protocol outcomes, case order, scoreboard classification, transactions, final state, statistics, and errors.

Do not require identical:

- absolute timestamps for every C++ coroutine action;
- counts of redundant polling reads;
- interleaving of independent-clock debug messages;
- visibility of superseded intermediate states with identical final protocol behavior.

Do require identical meaningful accepted/read/write/event sequences after collapsing redundant consecutive polling observations.

No simulator-specific delay or branch may be added to make one side pass.

---

## 14. Failure diagnosis map

| Symptom | Likely cause | Required response |
|---|---|---|
| `NativeClockCfg` is missing | stale `vip_common` | stop and report dependency mismatch; do not clone locally |
| `dut_wrapper.<clock>` cannot be found | wrong DUT name, absent clock registration, wrong build mode, or incomplete RTL wrapper | verify project user guide and wrapper contract; do not guess |
| `sim_*_period_ticks` width mismatch | incorrect `initNets()` width or wrapper contract mismatch | correct VIP registration only if docs prove width 64; otherwise stop |
| clock never starts | missing/incorrect enable net, task collision, or clock not explicitly started | correlate constructor/triplet/task and setup API |
| clock stop never completes | wrong stopped net or destination clock semantics mismatch | verify exact triplet; do not substitute a delay for `wait_stopped()` |
| clock has two drivers | protocol agent or old VPI task still toggles wrapper-owned net | disable/remove competing generator, preserve agent data/control path |
| final beat/byte appears twice on Verilator | terminal retirement delayed by an extra barrier/edge | fix first legal post-edge retirement in responsible reusable agent |
| first valid-ready transfer is skipped | READY sampled too late or source presented in wrong phase | implement setup-sample then immediately following acceptance edge |
| existing build now uses Verilator headers | cache/tree contamination or forced `vpi_include_dir` in wrong flavor | restore separate build trees and `EXISTING` default |
| Verilator plugin links wrong `vpi` library | unconditional external VPI link | keep external host VPI link under `EXISTING` only |
| object module cannot find `vpi_user.h` | `${vpi_include_dir}` did not reach its object target | add selected include to that target, not a hardcoded simulator path |
| duplicate CMake target | non-unique reusable VIP object target name | minimally make the internal object target globally unique |
| simulation exits while all clocks stopped | RTL wrapper keepalive/host issue | report to RTL flow; do not register or manipulate `sim_keepalive` in VIP |
| testcase plan or registration changed | migration exceeded scope | restore original sources, registrations, and execution plan |

---

## 15. Final diff and acceptance review

### 15.1 Inspect the complete diff

```bash
git status --short
git diff -- CMakeLists.txt src docs ext
git diff --check
```

Do not format unrelated files. Do not include build artifacts in version-controlled changes.

### 15.2 Expected parent-project changes

Normally limited to:

```text
CMakeLists.txt
src/pindefs.hpp
src/init.cpp
src/test.hpp
src/test.cpp
possibly a project-local README/user-facing build note
```

Selected project-specific `ext/vip_*` files may change only when the Section 9 audit proves a directly relevant defect. Known-good excluded modules should remain untouched.

### 15.3 Mandatory acceptance checklist

- [ ] Read repository rules and complete project VIP/user guides.
- [ ] Preserved all unrelated user changes.
- [ ] Proved `dut_wrapper` and exact clock-control contract from project documentation.
- [ ] Classified every clock-like signal by ownership.
- [ ] Added one native `Clock` per independently controlled wrapper-owned input clock.
- [ ] Used unique task names and removed duplicate/manual clock task registration.
- [ ] Registered each real clock and exact 1/64/1 control triplet.
- [ ] Kept DUT-generated output clocks observation-only.
- [ ] Removed or disabled competing clock generation in protocol agents.
- [ ] Preserved existing testcase clock-control APIs and behavior.
- [ ] Did not register or use `sim_keepalive`.
- [ ] Kept the existing build flavor as default.
- [ ] Used the exact `VIP_VPI_FLAVOR` / `EXISTING` / `VERILATOR` naming contract.
- [ ] Resolved Verilator's `vpi_user.h` from installed Verilator.
- [ ] Propagated `${vpi_include_dir}` to every compiled object target.
- [ ] Kept external standard VPI linking under `EXISTING` only.
- [ ] Used the exact `cmake-build-verilator` separate-tree name.
- [ ] Added the exact `verilator_<logical_name>` convenience-target naming pattern.
- [ ] Added no alternate CMake helper, preset, auto-detection scheme, option name, or simulator-dependent C++ definition.
- [ ] Used direct four-argument `vip::common::Clock` construction with no new project-local clock abstraction.
- [ ] Preserved all testcase CMake membership, registrations, and plan entries.
- [ ] Ensured `core::finishSimulation()` occurs once in runner `after_all`, after cleanup.
- [ ] Audited every non-excluded project-specific `ext/vip_*` module.
- [ ] Left passing modules unchanged and minimally repaired only proven defects.
- [ ] Existing/Questa-side plugin target builds successfully.
- [ ] Verilator-side plugin target builds successfully in its separate tree.
- [ ] No RTL simulation was run from the VIP workflow.
- [ ] Final diff contains only required changes.

---

## 16. Required final report from Codex

Return a compact evidence-based handoff containing:

1. **Clock map**
   - each wrapper-owned actual clock;
   - its exact enable/period/stopped triplet;
   - its `Clock` member and unique task name;
   - every DUT-generated observation-only clock;
   - every legitimate protocol-generated transaction clock.

2. **Files changed**
   - one line per file with the exact reason;
   - separate parent-repository and nested-submodule changes.

3. **CMake result**
   - existing target name and successful command;
   - Verilator convenience target and successful command;
   - exact artifact paths;
   - confirmation that the Verilator flavor omits the external standard `vpi` link.

4. **External VIP audit table**
   - every audited non-excluded `ext/vip_*` module;
   - pass/fail by clock, phase/handshake, and CMake;
   - exact remediation or `unchanged`.

5. **Behavior-preservation evidence**
   - testcase source membership unchanged;
   - `register_tc_*` set unchanged;
   - execution plan unchanged;
   - no stimulus/expected-result/budget/iteration/seed changes.

6. **Remaining handoff**
   - focused and full Verilator simulation to run in the RTL flow;
   - unchanged full Questa regression to run in the RTL flow;
   - any genuine blocker or unverified assumption.

7. **Canonical-conformance statement**
   - list the project-specific substitutions made from the allowed substitution table;
   - state `canonical infrastructure deviations: none` when the specification was followed literally;
   - if any deviation was unavoidable, do not claim completion: report the exact incompatibility and the user decision still required.

Do not report success merely because the final `.so` exists. Success requires the wiring, ownership, source-membership, audit, and dual-build evidence above.

---

## 17. Reference pattern from `vip_mac_1g`

Use this only as an architectural reference, never as a clock-name template for another project.

`vip_mac_1g` demonstrates:

- `dutName("dut_wrapper")`;
- always-present native controllers for `clk` and `gtx_clk`;
- one mode-dependent native receive-clock controller for `gmii_rx_clk` or `rgmii_rxc`;
- `gmii_gtx_clk` and `rgmii_txc` as DUT-generated observation-only clocks;
- actual clock plus `sim_*_enable`, 64-bit `sim_*_period_ticks`, and `sim_*_stopped` registration;
- unique auto-registered task names;
- RGMII data/control driving with RX clock generation disabled inside the protocol driver;
- unchanged testcase-facing `start()`, `stop_and_wait()`, and `start_at()` calls;
- an existing-default CMake flavor and a separate-tree Verilator flavor;
- conditional omission of the external standard `vpi` link for Verilator;
- `core::finishSimulation()` in project runner `after_all` after final cleanup;
- no VIP interaction with `sim_keepalive`.

The reusable result in another project must match these ownership and build principles while using that project's real interfaces, clocks, target names, and dependencies.
