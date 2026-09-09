# RTL refactor execution specification

## Standard `dut_wrapper.sv`, native controllable clocks, and dual Questa/Verilator simulation flows

This file is a complete implementation specification for refactoring one existing `rtl_<project>` repository at a time. The required target state is the architecture proven in `rtl_mac_1g`, generalized only for the real synthesizable sources, dependencies, parameters, ports, build modes, and clocks of the project currently being reworked.

The objective is infrastructure parity, not RTL redesign:

- the real synthesizable RTL top remains the product top and remains usable by synthesis;
- a root-level, simulation-only `dut_wrapper.sv` instantiates that product top as `u_dut`;
- external/VIP-owned DUT input clocks are generated natively inside `dut_wrapper.sv` through the standard clock generator;
- DUT-generated clock outputs remain observation-only;
- the existing Questa flow and the new Verilator flow both elaborate the same `dut_wrapper` hierarchy;
- the Verilator flow builds a native timed executable with VPI enabled and loads the separately built sibling VIP plugin;
- the Questa flow continues loading the existing sibling VIP plugin;
- source order, compile definitions, interface-selection mode, time precision, DUT parameters, and VPI-visible names agree across both flows;
- a temporary root `user_guide.md`, copied in by the user from the paired `vip_<project>/docs/`, is patched in place with the final `dut_wrapper.sv` integration contract for the later VIP refactor;
- existing Questa waveform setup files are updated when the simulation top or hierarchy changes, including `wave.tcl` when present;
- functional RTL and project behavior are not otherwise refactored, renamed, optimized, or modernized.

Treat this as an execution task. Inspect the repository, make the required changes, configure and build both simulator compile targets, and report exact results. Do not stop after producing a plan.

---

## Cross-project determinism mandate

This is a replication task, not an architecture-design task. Every project refactored with this file shall use the same `rtl_mac_1g` infrastructure pattern. Codex must not invent an alternative implementation merely because it appears functionally equivalent.

### Allowed project-specific substitutions

Only the following facts may vary because they describe the actual project:

| Category | Allowed to vary |
|---|---|
| Project identity | Existing RTL project name/version, real synthesizable top name, sibling VIP project name, and VIP logical target suffix |
| Synthesizable sources | Actual local RTL files and their dependency order |
| RTL dependencies | Actual `ext/<dependency>` repositories, logical Questa library names, dependency import order, and their flat Verilator source manifests |
| Product configuration | Existing compile-time defines/undefines, include directories, package search paths, and supported mutually exclusive build modes |
| Product parameters | Exact parameter names, types, defaults, and explicit pass-through mappings of the real top |
| Product I/O | Exact non-wrapper-owned port names, types, directions, widths, dimensions, interfaces, and compile-time conditional groups of the real top |
| Clock inventory | Actual external/VIP-owned input clocks, DUT-generated output clocks, conditional clock presence, and nominal full periods in 1 ps ticks |
| Existing simulator details | Project-specific wave script and any already-required, proven Questa options that do not conflict with this contract |
| Minimal portability repairs | Only a source change required by a demonstrated Questa/Verilator compile error, with no functional or synthesis change |

### Fixed infrastructure choices

The following choices are mandatory and shall not vary between projects:

- simulation wrapper filename: `dut_wrapper.sv` at repository root;
- simulation wrapper module name: `dut_wrapper`;
- real DUT instance name: `u_dut`;
- native clock generator module name: `sim_native_clock_gen`;
- one common `sim_native_clock_gen` definition and one instance per independently controllable external clock;
- clock-control triplet names: `sim_<clock>_enable`, `sim_<clock>_period_ticks`, and `sim_<clock>_stopped`;
- generated clock net name equal to the original real-DUT clock input name `<clock>`;
- 1/64/1 control widths, initially disabled clock, 1 ps control ticks, low parked state, half-cycle stop behavior, and deterministic same-slot first rising edge;
- exact Verilator visibility annotations described in this file;
- standard `sim_keepalive` block;
- wrapper time unit/precision: `timeunit 1ns; timeprecision 1ps;`;
- `SIM_WRAPPER_SOURCE` kept separate from `RTL_SOURCES`;
- wrapper excluded from every synthesis and reusable-product source manifest;
- Questa simulation top: `dut_wrapper`;
- Verilator simulation top: `dut_wrapper`;
- Verilator build targets: `sim_verilator_compile` and `sim_verilator_run`;
- existing Questa target family: `sim_questa_init`, `sim_questa_compile`, `sim_questa_run`, and `sim_questa_run_gui`;
- batch run script `scripts/sim/questa/questa_run.tcl` containing exactly `log -r /*` followed by `run -all`;
- GUI run script whose final active simulation command is `run -all`, never a finite-duration `run <time>`;
- final active `scripts/sim/questa/vsim_options` entry: `-onfinish stop`;
- existing project waveform scripts retained and rebased to the `dut_wrapper` / `u_dut` hierarchy when applicable;
- temporary VIP handoff guide filename: root `user_guide.md`, supplied by the user before the task and left there after being patched;
- Verilator output directory layout: `${CMAKE_BINARY_DIR}/verilator/obj_dir`;
- Verilator executable name: `sim_verilator`;
- sibling VIP Verilator build directory: `../<vip_project>/cmake-build-verilator`;
- sibling VIP Verilator plugin filename: `lib<vip_project>.so`;
- Verilator plugin load syntax: `+verilator+vpi+<absolute-plugin-path>`;
- the canonical CMake structure and option ordering shown in this specification.

Do not replace these choices with:

- a project-specific wrapper name;
- a separate clock-generator `.sv` file;
- copied per-clock `always #...` generators;
- an `initial` block that starts clocks automatically;
- clock-control ports added to the public wrapper I/O;
- a clock manager, interface, macro framework, generate-time clock framework, bind file, DPI clock service, or direct per-edge VPI clock driver;
- a CMake preset-only flow, helper abstraction, automatic simulator detector, generated file list, wildcard/glob source list, or differently named targets;
- a custom Verilator C++ harness when the standard `--binary` flow works;
- synthesis conditionals inserted into the real RTL solely to support simulation.

If the canonical implementation is technically incompatible with a project, stop and report the exact incompatibility. Do not silently create a third architecture. Any exception requires explicit user approval and should be added to this common specification before it is propagated to other repositories.

---

## 1. Sources of truth and precedence

Before changing anything, read these files completely when present:

1. root `AGENTS.md` and every applicable nested `AGENTS.md`;
2. `docs/rtl_design_guide.md`, especially its standard simulation wrapper section;
3. the project-specific `docs/design_guide.md`, user guide, top-level specification, or equivalent;
4. the root `CMakeLists.txt` and every included `.cmake` file;
5. the real synthesizable top module and every package/interface it imports;
6. `scripts/sim/questa/vlog_options`, `vsim_options`, and run Tcl scripts;
7. every `ext/*/scripts/cmake/questa_modules.cmake` used by the project;
8. the project's own `scripts/cmake/questa_modules.cmake`, if it can be consumed as a dependency;
9. all Vivado, ISE, Quartus, lint, formal, packaging, and synthesis source manifests;
10. root `user_guide.md`, supplied temporarily by the user from the paired `vip_<project>/docs/user_guide.md`, as the verification-consumer guide that must be patched during this task.

The staged root `user_guide.md` is the only VIP-owned content edited by this RTL task. Do not enter, inspect, or modify sibling `vip_*` source to update it. A separately authorized/configured plugin build does not authorize VIP source inspection. If the file is absent, stop and request that the user copy the current VIP guide into the RTL repository root before continuing; do not invent a replacement from memory.

Precedence is:

1. explicit user instructions and applicable repository instructions;
2. `docs/rtl_design_guide.md` for universal RTL/wrapper rules;
3. the project-specific design guide for the real top, parameters, I/O, modes, dependencies, and clock ownership;
4. the checked-in real RTL source when documentation is incomplete;
5. existing known-good Questa behavior for project-specific options;
6. this file for the canonical migration mechanics.

Do not copy `rtl_mac_1g` project details into another IP. In particular, do not assume its module name, `MAC_1G_RGMII_SEL`, source list, library list, parameters, GMII/RGMII ports, or clock set.

If documentation and RTL disagree about the real product contract or clock ownership, stop and report the discrepancy before creating the wrapper.

---

## 2. Non-negotiable scope boundaries

Make only changes required to add the standard wrapper, preserve/normalize the Questa simulation integration, add the Verilator simulation integration, document the resulting contract, and fix directly proven cross-simulator compile blockers.

Do not:

- redesign functional RTL;
- change protocol behavior, timing intent, reset semantics, CDC architecture, register maps, FSM behavior, datapath widths, or parameter meaning;
- add, remove, rename, or reorder public product ports or parameters in the real synthesizable top;
- move simulation-only clock generation into `src/`;
- add any wrapper signal to synthesis;
- turn a DUT-generated output clock into a wrapper-driven clock;
- leave an external/VIP-owned clock driven both by a wrapper generator and by another source;
- generate reset inside the wrapper unless a separate explicit project contract requires it;
- introduce asynchronous reset behavior;
- change a testcase, expected result, timeout, coverage item, iteration count, stress seed, or verification plan;
- modify or inspect sibling `vip_*` source as part of this RTL task; editing the user-supplied root `user_guide.md` copy is the sole permitted VIP-document handoff action;
- modify `flow_mgmt` or its project registry;
- initialize, fetch, update, add, remove, or retarget submodules unless separately requested;
- use recursive submodule commands;
- commit or push;
- clean or discard pre-existing user changes;
- bulk-format unrelated files;
- fix Verilator warnings merely because they exist; `-Wno-fatal` intentionally keeps warnings non-fatal;
- add simulator-specific behavior inside synthesizable RTL to force a test to pass;
- use wildcard source discovery such as `file(GLOB ...)`.

The wrapper refactor shall normally change only:

- root `dut_wrapper.sv` — new or canonicalized simulation-only infrastructure;
- root `CMakeLists.txt` — standard dual-simulator integration and project-real manifests;
- simulator option/run and waveform files when precision, configuration parity, or the new `dut_wrapper` hierarchy requires it, including an existing `wave.tcl`;
- project documentation describing the wrapper, clocks, and build commands;
- root `user_guide.md` — the temporary VIP-facing copy supplied by the user, patched in place with the completed wrapper contract;
- a synthesizable RTL file only when an actual compiler diagnostic proves a portability defect and the smallest behavior-preserving repair is required.

Preserve unrelated dirty-worktree changes. Run `git status --short` before editing and again at handoff.

---

## 3. Mandatory final state

The refactor is complete only when every applicable statement is true:

1. The real synthesizable top remains unchanged as the product/synthesis top.
2. Root `dut_wrapper.sv` exists and contains `module dut_wrapper`.
3. `dut_wrapper` instantiates the real top exactly once as `u_dut`.
4. Wrapper parameters mirror and explicitly pass through the real top parameters required by simulation.
5. Normal non-clock I/O is preserved with the real names, widths, directions, dimensions, and conditional presence.
6. Every external/VIP-owned real-DUT input clock is internal to the wrapper and has exactly one standard control triplet and one generator instance.
7. Every DUT-generated output clock remains an ordinary wrapper output and has no `sim_*` control triplet.
8. Conditional clock/interface groups use the exact same compile-time conditions as the real top.
9. All generated clocks start disabled and parked low.
10. `sim_keepalive` exists, is not connected to `u_dut`, and is not a testcase control.
11. `RTL_SOURCES` contains only synthesizable project RTL in dependency order.
12. `SIM_WRAPPER_SOURCE` is a separate root file and is compiled only for simulation.
13. No synthesis, packaging, dependency-export, lint-as-product, or vendor source manifest contains `dut_wrapper.sv`.
14. Questa compiles dependency libraries, project RTL, then `dut_wrapper.sv`, and elaborates `dut_wrapper`.
15. Verilator compiles a flat dependency-ordered list consisting of dependency RTL, project RTL, then `dut_wrapper.sv`, and elaborates `dut_wrapper`.
16. Questa and Verilator use the same active product mode, definitions, includes, language version, and effective 1 ps simulation precision.
17. `scripts/sim/questa/questa_run.tcl` logs recursively and executes `run -all`; it contains no finite simulation duration.
18. `scripts/sim/questa/questa_run_gui.tcl` preserves project-specific GUI/wave setup but its final active simulation command is `run -all`; it contains no finite simulation duration.
19. The final active entry in `scripts/sim/questa/vsim_options` is `-onfinish stop`.
20. `sim_verilator_compile` produces `${CMAKE_BINARY_DIR}/verilator/obj_dir/sim_verilator`.
21. `sim_verilator_run` verifies the sibling VIP plugin and executable exist, then loads the plugin with `+verilator+vpi+...`.
22. The standard target names and build-directory names are unchanged.
23. The existing Questa plugin path remains `../${VIP_DESIGN_NAME}/cmake-build-release/lib${VIP_DESIGN_NAME}.so` unless the existing project already has an explicitly different approved path.
24. The Verilator plugin path is `../${VIP_DESIGN_NAME}/cmake-build-verilator/lib${VIP_DESIGN_NAME}.so`.
25. Both compile targets succeed without deleting sources, suppressing errors broadly, or changing functional behavior.
26. If simulations are authorized and their prerequisites exist, both run targets start with the same elaborated configuration and complete according to the existing test plan.
27. Cross-project differences are limited to the allowed substitution table.
28. The supplied root `user_guide.md` preserves its existing VIP usage content and accurately documents the final `dut_wrapper`, `u_dut`, wrapper-visible ports, clock ownership, native clock controls, reset ownership, compile-time mode, and simulator-plugin prerequisites needed by the later VIP refactor.
29. Every existing Questa waveform setup file that referenced the former simulation top has been updated to the `dut_wrapper` hierarchy; DUT-internal paths descend through `/dut_wrapper/u_dut`, wrapper infrastructure paths descend directly through `/dut_wrapper`, and no stale old-top path remains.

---

## 4. Preflight inventory and baseline capture

### 4.1 Record repository state

Run read-only inventory first:

```bash
git status --short
git submodule status
rg --files -g 'AGENTS.md' -g 'CMakeLists.txt' -g '*.cmake'
rg --files src ext scripts docs
test -f user_guide.md
rg --files scripts/sim/questa -g '*.tcl'
rg -n '^(module|interface|package)\b|^\s*parameter\b|^\s*(input|output|inout)\b' src
```

Do not clean a dirty worktree. Identify whether any existing user changes overlap `CMakeLists.txt`, `dut_wrapper.sv`, simulator scripts, waveform files, documentation, root `user_guide.md`, or the real top. The newly supplied root `user_guide.md` is expected to appear as a user-provided file; preserve its entire pre-existing content while patching only the integration facts made stale by this refactor.

### 4.2 Fill the project substitution record before editing

Create a working table and fill every value from checked-in evidence:

| Placeholder | Required project-real value |
|---|---|
| `<RTL_PROJECT_NAME>` | Existing root CMake project name, normally `rtl_<logical_name>` |
| `<REAL_DUT_TOP>` | Actual synthesizable top module |
| `<VIP_PROJECT_NAME>` | Paired sibling VIP project, normally `vip_<logical_name>` |
| `<VIP_LOGICAL_NAME>` | Suffix used by the VIP convenience target `verilator_<logical_name>` |
| `<VIP_VERILATOR_TARGET>` | Exact paired target; must equal `verilator_<VIP_LOGICAL_NAME>` |
| `<RTL_SOURCE_LIST>` | All local synthesizable sources in compile order |
| `<DEPENDENCY_LIST>` | Direct `ext/` dependencies in import order |
| `<QUESTA_LIBRARY_LIST>` | Logical library names and CMake variable names |
| `<VERILATOR_DEP_SOURCE_LIST>` | Flat transitive dependency sources in compile order |
| `<COMPILE_DEFINITIONS>` | Exact active product-mode definitions for the chosen elaboration |
| `<INCLUDE_PATHS>` | Required include/package paths for both compilers |
| `<REAL_TOP_PARAMETERS>` | Exact name/type/default list from the real top |
| `<REAL_TOP_PORTS>` | Exact name/type/direction/width/condition list from the real top |
| `<WRAPPER_CLOCKS>` | External/VIP-owned input clocks generated by the wrapper |
| `<DUT_CLOCK_OUTPUTS>` | DUT-generated clocks retained as outputs/observe-only |
| `<CLOCK_DEFAULT_TICKS>` | Nominal full periods converted to integer 1 ps ticks |
| `<QUESTA_WAVE_SCRIPT>` | Existing project wave script, if any |
| `<STAGED_VIP_USER_GUIDE>` | Root `user_guide.md`; mandatory temporary copy supplied by the user |

Do not edit until this record is complete. Unknown values are blockers, not invitations to guess.

### 4.3 Identify the real synthesizable top

Prove the real top from at least two of:

- synthesis scripts/project files;
- the project-specific design guide;
- the current Questa elaboration command;
- dependency/package hierarchy;
- a module that exposes the complete product I/O and instantiates the design beneath it.

Do not choose a testbench, an old wrapper, a protocol submodule, or a filename merely because it resembles the project name.

Record the real top's complete parameter and port declarations. Preserve unpacked dimensions, signedness, types, interfaces/modports, and preprocessor conditions.

### 4.4 Capture the existing Questa baseline

Record:

- current configure command and build directory;
- current `sim_questa_*` targets;
- active `vlog_options` and `vsim_options`;
- dependency libraries and compile order;
- elaborated top;
- VPI plugin location;
- active defines and parameter overrides;
- the GUI run script's waveform setup command, every sourced wave/watch Tcl file, and the hierarchy root currently used by those files;
- existing compile result, if the repository instructions permit building.

A pre-existing failure unrelated to this task does not authorize unrelated repair.

### 4.5 Inventory every source manifest

Compare at least:

- root `RTL_SOURCES`;
- project `scripts/cmake/questa_modules.cmake`;
- each dependency's `scripts/cmake/questa_modules.cmake`;
- Vivado `read_sources.tcl` files;
- ISE/Quartus/project source lists;
- packaging/lint/formal manifests;
- Verilator's new flat manifest.

For synthesizable sources, order must be deterministic and package/interface definitions must precede their consumers. The wrapper is the deliberate exception: it appears only in root simulation integration and always after the real product sources.

### 4.6 Confirm the temporary VIP user-guide handoff

Before editing RTL infrastructure, confirm that root `user_guide.md` exists and is the current copy supplied by the user from the paired VIP project's `docs/user_guide.md`.

Read it completely and record:

- its current simulation-top name and DUT hierarchy assumptions;
- its documented public ports, parameters, widths, reset behavior, and compile-time modes;
- every clock the VIP currently drives or observes;
- any build, plugin, pin-registration, or testcase-facing instructions affected by the new wrapper;
- its table of contents and the section where the wrapper contract should be integrated.

This file is a staged handoff artifact, not authority over the synthesizable RTL. Resolve wrapper facts from the completed RTL implementation and authoritative RTL documentation, then patch this guide to describe the final consumer-visible contract. Preserve unrelated protocol usage, testcase guidance, examples, and document structure. Do not rewrite the guide from scratch.

---

## 5. Build the clock-ownership inventory

Before writing `dut_wrapper.sv`, classify every clock-like top-level signal:

| Clock | Direction at real DUT | Source in hardware | Simulation owner | Independently controllable | Wrapper action |
|---|---|---|---|---|---|
| project-specific | input/output | external/DUT/protocol | wrapper/DUT/protocol | yes/no | native generator / pass-through / none |

### 5.1 External/VIP-owned input clock

A clock belongs to the wrapper only when all of these are true:

- it is an input to the real synthesizable top;
- hardware receives it from outside the DUT;
- verification must be able to start, stop, rephase, or change its period;
- the paired VIP will control it through `vip_common::Clock`.

Required wrapper result for real DUT clock input `<clock>`:

```text
generated waveform net:
  <clock>

control/status:
  sim_<clock>_enable
  sim_<clock>_period_ticks
  sim_<clock>_stopped
```

The original clock input is removed from the public port list of `dut_wrapper`; it becomes an internal generated net connected to `u_dut.<clock>`.

### 5.2 DUT-generated output clock

If the real DUT generates a clock output:

- preserve it as a normal `dut_wrapper` output;
- connect it directly from `u_dut`;
- do not create `sim_*` controls;
- do not instantiate a generator for it;
- document it as observation-only.

Direction, not the presence of `clk` in the name, decides ownership.

### 5.3 Protocol-generated clock

Some transaction protocols intentionally generate a clock as part of a transfer. If that clock is driven by a protocol initiator rather than being a free-running wrapper-owned input, do not automatically migrate it to `sim_native_clock_gen`. Preserve the established protocol ownership and document why.

If ownership is ambiguous, stop. Do not allow two drivers.

### 5.4 Conditional clocks and modes

When a compile-time mode selects mutually exclusive interfaces/clocks:

- use the same `ifdef`/`ifndef`/`elsif` condition in wrapper ports, control groups, generated nets, generator instances, and DUT connections;
- ensure the same mode define is active in Questa and Verilator;
- include only the active wrapper signals in the paired VIP registration;
- never expose both modes merely to simplify the wrapper.

### 5.5 Nominal period conversion

The wrapper control unit is always 1 ps. Convert documented nominal full periods mechanically:

| Nominal period | Initial value |
|---:|---:|
| `10 ns` | `64'd10000` |
| `8 ns` | `64'd8000` |
| `6.4 ns` | `64'd6400` |
| `4 ns` | `64'd4000` |

Derive values from the current project documentation and VIP startup configuration. Do not copy `rtl_mac_1g` defaults. If documentation, old testcase configuration, and current RTL disagree, resolve the conflict before editing. Although clocks start disabled and VIP normally programs the period before enabling, the checked-in defaults must still be sane and documented.

---

## 6. Create the standard `dut_wrapper.sv`

Create exactly one repository-root file:

```text
dut_wrapper.sv
```

It is simulation-only. It contains exactly two top-level modules in this order:

1. `sim_native_clock_gen`;
2. `dut_wrapper`.

Begin the file with:

```systemverilog
`default_nettype none
```

Do not put this file under `src/`. Do not add synthesis guards around it; exclude it through source manifests.

Inside `dut_wrapper`, retain this exact section order so every project is easy to compare:

1. `timeunit 1ns; timeprecision 1ps;`;
2. `USER CONTROLLABLE CLOCKS` control/status declarations;
3. `Generated DUT clock nets`;
4. `Native simulation clocks`;
5. `Simulation timing-queue keepalive for Verilator`;
6. `Synthesizable DUT instance`.

Use those section labels in comments. Conditional interface blocks may appear within the relevant section but shall not change the overall order.

### 6.1 Mandatory top-of-file documentation

Use the following structure and replace only project-real names/list entries:

```systemverilog
`default_nettype none

// Simulation-only wrapper for <REAL_DUT_TOP>.
//
// Purpose:
//   - Keep <REAL_DUT_TOP>.sv synthesizable and unchanged.
//   - Generate DUT clocks natively in SystemVerilog instead of toggling every
//     clock edge through RapidVPI.
//   - Expose a small set of internal VPI-visible control/status signals so the
//     existing vip_common::Clock API remains transparent to all tc_* code.
//   - Keep one far-future native HDL timed event scheduled so Verilator's
//     stock --binary --vpi timing loop remains alive even when every DUT clock
//     is intentionally stopped by a testcase.
//
// -----------------------------------------------------------------------------
// USER CONTROLLABLE CLOCKS
// -----------------------------------------------------------------------------
//   <purpose> -> <clock_1>
//     sim_<clock_1>_enable
//     sim_<clock_1>_period_ticks
//     sim_<clock_1>_stopped
//
//   <purpose> -> <clock_2>
//     sim_<clock_2>_enable
//     sim_<clock_2>_period_ticks
//     sim_<clock_2>_stopped
//
// Control semantics:
//   *_enable        : 1 = run clock, 0 = stop and park clock low
//   *_period_ticks  : full clock period; one tick = 1ps in this wrapper
//   *_stopped       : status; 1 = clock is stopped and parked low
//
// The native clock generator clamps periods below 2 ticks to 2 ticks and uses:
//   high_ticks = period_ticks / 2
//   low_ticks  = period_ticks - high_ticks
//
// For deterministic start_at() semantics, vip_common programs the period
// first, then schedules the corresponding *_enable 0->1 write at the requested
// first-rise tick. The native generator produces the first rising edge in that
// same simulation time slot.
// -----------------------------------------------------------------------------
```

List every and only wrapper-owned clock, including mode conditions. Also list DUT-generated clocks separately as observation-only when confusion is possible.

### 6.2 Copy this native generator literally

The following module is canonical infrastructure. Copy it without semantic edits, renaming, abstraction, or per-project variants:

```systemverilog
module sim_native_clock_gen (
    input  wire         enable_i,         // 1: run clock, 0: stop and park low
    input  wire  [63:0] period_ticks_i,   // Full clock period in simulation ticks
    output logic        clk_o = 1'b0,     // Generated clock output
    output logic        stopped_o = 1'b1  // 1 when clock is stopped and parked low
);

  timeunit 1ns; timeprecision 1ps;

  longint unsigned period_ticks_now;
  longint unsigned high_ticks;
  longint unsigned low_ticks;

  // Simulation-only clock process.
  //
  // Stop behavior intentionally takes effect on the next half-cycle boundary,
  // matching the existing RapidVPI clock agent's apply_requests_() behavior.
  // The clock is parked low before stopped_o is asserted.
  always begin : p_native_clock
    wait (enable_i === 1'b1);

    stopped_o = 1'b0;
    clk_o = 1'b1;

    while (enable_i === 1'b1) begin
      period_ticks_now = (period_ticks_i < 64'd2) ? 64'd2 : period_ticks_i;
      high_ticks = period_ticks_now / 2;
      low_ticks = period_ticks_now - high_ticks;

      #(high_ticks * 1ps);
      if (enable_i !== 1'b1) begin
        break;
      end

      clk_o = 1'b0;

      #(low_ticks * 1ps);
      if (enable_i !== 1'b1) begin
        break;
      end

      clk_o = 1'b1;
    end

    clk_o = 1'b0;
    stopped_o = 1'b1;
  end

endmodule
```

These plain `always`, `wait`, initialized signals, blocking assignments, `break`, and `#` delays are deliberate simulation-only exceptions. Never copy them into synthesizable RTL.

### 6.3 Mirror the real top parameters

Declare `module dut_wrapper #(...)` using the real top's parameter names, types, widths, signedness, defaults, order, and compile-time conditions.

Rules:

- copy the parameter declaration mechanically from the real top;
- do not substitute untyped parameters;
- do not change defaults to match a testcase;
- do not add wrapper-only functional parameters;
- explicitly map every exposed parameter by name into `u_dut`;
- retain `RTL_DBG` or equivalent only when it already belongs to the real top;
- if the real top has no parameters, omit `#(...)` entirely.

Canonical form:

```systemverilog
module dut_wrapper #(
    parameter int unsigned DATA_W = 32,
    parameter bit          RTL_DBG = 1'b1
) (
    // project-real non-wrapper-owned ports
);
```

The example names are illustrative. Never add `DATA_W` or `RTL_DBG` unless the actual top declares them.

### 6.4 Mirror normal product I/O exactly

Copy every real top port except wrapper-owned external clock inputs into the wrapper interface.

Preserve:

- port name;
- input/output/inout direction;
- scalar/vector width and expression;
- signedness;
- packed/unpacked dimensions;
- interface/modport type;
- compile-time conditional grouping;
- reset ports as ordinary external ports;
- DUT-generated clock outputs as ordinary outputs.

Use the project's normal SystemVerilog style, including `input wire` for scalar/vector inputs and `output logic` where the project uses variables for outputs. Do not introduce adapters, renaming wires, protocol behavior, reset generation, tie-offs, or width conversions.

If a real top port is intentionally tied to a constant in the old simulation flow, preserve the established elaboration contract and document it. Do not silently drop the port.

### 6.5 Declare one standard control group per wrapper-owned clock

Inside `dut_wrapper`, after:

```systemverilog
  timeunit 1ns; timeprecision 1ps;
```

declare each group mechanically:

```systemverilog
  logic        sim_<clock>_enable /* verilator public_flat_rw */ = 1'b0;
  logic [63:0] sim_<clock>_period_ticks /* verilator public_flat_rw */ = 64'd<nominal_ps>;
  logic        sim_<clock>_stopped /* verilator public_flat_rd */;
```

The exact annotations are mandatory:

| Signal | Width | Initial value | Annotation | Role |
|---|---:|---:|---|---|
| `sim_<clock>_enable` | 1 | `0` | `public_flat_rw` | VIP start/stop request |
| `sim_<clock>_period_ticks` | 64 | nominal full period | `public_flat_rw` | Full period in 1 ps ticks |
| `sim_<clock>_stopped` | 1 | generator initializes output | `public_flat_rd` | Fully stopped/low acknowledgement |

Do not initialize enable to `1`. Do not make these public wrapper ports. They remain internal but VPI-visible.

### 6.6 Preserve original clock-net names

For each wrapper-owned real-DUT clock input, create:

```systemverilog
  logic <clock> /* verilator public_flat_rd */;
```

The name must equal the real DUT port and the paired VIP pin definition. Do not use `generated_<clock>`, `<clock>_sim`, or another alias.

### 6.7 Instantiate each native generator mechanically

For every independent wrapper-owned clock:

```systemverilog
  sim_native_clock_gen u_sim_<clock> (
      .enable_i      (sim_<clock>_enable),
      .period_ticks_i(sim_<clock>_period_ticks),
      .clk_o         (<clock>),
      .stopped_o     (sim_<clock>_stopped)
  );
```

Do not share one generator instance between unrelated clocks. Do not create phase-coupled derived clocks in the wrapper unless that relationship is already the real DUT's responsibility.

### 6.8 Add the standard Verilator keepalive literally

Inside `dut_wrapper`, before `u_dut`, include:

```systemverilog
  // Far-future native timed event so Verilator does not terminate merely
  // because all controllable DUT clocks are intentionally stopped.
  logic sim_keepalive /* verilator public_flat_rd */ = 1'b0;

  always begin : p_sim_keepalive
    #1s;
    sim_keepalive = ~sim_keepalive;
  end
```

Rules:

- do not connect it to `u_dut`;
- do not expose it as a port;
- do not register it in VIP;
- do not shorten the interval;
- do not use it for reset, timeout, heartbeat, or functional behavior.

### 6.9 Instantiate the real product top as `u_dut`

Use an explicit parameter and port map:

```systemverilog
  <REAL_DUT_TOP> #(
      .<PARAM_1>(<PARAM_1>),
      .<PARAM_2>(<PARAM_2>)
  ) u_dut (
      .<clock_1>(<clock_1>),
      .<reset>  (<reset>),
      .<port_1> (<port_1>),
      .<port_2> (<port_2>)
  );
```

Do not use `.*`. An explicit map is required so review can prove the wrapper preserves the product contract and correctly replaces only wrapper-owned clocks.

For a parameterless top:

```systemverilog
  <REAL_DUT_TOP> u_dut (
      // explicit project-real port map
  );
```

### 6.10 Wrapper structural review checklist

Before touching CMake, verify:

- exactly one `sim_native_clock_gen` definition;
- exactly one `dut_wrapper` definition;
- exactly one `u_dut` instance;
- control-group count equals independently controlled external-clock count;
- generator-instance count equals control-group count;
- every control signal follows the exact naming/width/annotation rule;
- no wrapper-owned clock remains a public wrapper input;
- no DUT-generated output clock has a control group;
- normal ports and parameters match the real top;
- conditional groups match the real top's preprocessor conditions;
- every default period is an integer full period in 1 ps ticks;
- `sim_keepalive` is isolated;
- the real RTL was not changed to accommodate the wrapper.

---

## 7. Standard root CMake architecture

Normalize the root CMake around the following fixed architecture. Preserve unrelated project targets such as ctags, documentation, synthesis, or packaging.

### 7.1 Canonical identity and source variables

At the top of root `CMakeLists.txt`, use:

```cmake
cmake_minimum_required(VERSION 3.10)

project(<RTL_PROJECT_NAME> VERSION <EXISTING_VERSION> LANGUAGES NONE)

set(RTL_TOP_MODULE "dut_wrapper")
set(VIP_DESIGN_NAME "<VIP_PROJECT_NAME>")

set(VPI_MODULE_DIR "${CMAKE_SOURCE_DIR}/../${VIP_DESIGN_NAME}/cmake-build-release/")
set(VPI_MODULE_NAME "${VPI_MODULE_DIR}/lib${VIP_DESIGN_NAME}.so")

set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(EXT_DIR "${CMAKE_SOURCE_DIR}/ext")
set(SIM_WRAPPER_SOURCE "${CMAKE_SOURCE_DIR}/dut_wrapper.sv")

set(RTL_SOURCES
    # Explicit, dependency-ordered, synthesizable project sources only.
    "${SRC_DIR}/<first_project_source>.sv"
    "${SRC_DIR}/<next_project_source>.sv"
    "${SRC_DIR}/<real_top_source>.sv"
)
```

Project substitutions are limited to project/version, VIP name, and actual explicit source entries. These names are fixed:

- `RTL_TOP_MODULE`;
- `VIP_DESIGN_NAME`;
- `VPI_MODULE_DIR`;
- `VPI_MODULE_NAME`;
- `SRC_DIR`;
- `EXT_DIR`;
- `SIM_WRAPPER_SOURCE`;
- `RTL_SOURCES`.

Never put `${SIM_WRAPPER_SOURCE}` inside `RTL_SOURCES`.

### 7.2 Source-list rules

`RTL_SOURCES` contains only local synthesizable product sources. List files explicitly and in dependency order:

1. packages/type definitions;
2. interfaces;
3. leaf modules;
4. intermediate modules;
5. real product top last.

Do not add dependency sources from `ext/` to `RTL_SOURCES`; they have their own Questa libraries and the separate Verilator flat dependency manifest.

Do not use globs. A deterministic manifest is part of the contract.

### 7.3 Standard simulator file variables

Use:

```cmake
set(VLOG_OPTIONS "${CMAKE_SOURCE_DIR}/scripts/sim/questa/vlog_options")
set(VSIM_OPTIONS "${CMAKE_SOURCE_DIR}/scripts/sim/questa/vsim_options")
set(QUESTA_RUN_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/sim/questa/questa_run.tcl")
set(QUESTA_RUN_GUI_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/sim/questa/questa_run_gui.tcl")

set(QUESTA_SIM_DIR "${CMAKE_BINARY_DIR}/questa")
file(MAKE_DIRECTORY ${QUESTA_SIM_DIR})

include("${CMAKE_SOURCE_DIR}/scripts/cmake/rtl_dependency_helpers.cmake")
```

Preserve project-specific run script content. The paths and variable names above remain standard.

---

## 8. Preserve the canonical Questa flow

The new wrapper/Verilator work must not replace the existing compiled-library Questa architecture.

### 8.1 Logical library declarations

Declare `work`, then one CMake variable and directory for each real dependency library:

```cmake
set(WORK_LIB "work")
set(DEP_A_LIB "project_dep_a")
set(DEP_B_LIB "project_dep_b")

set(WORK_LIB_DIR "${QUESTA_SIM_DIR}/${WORK_LIB}")
set(DEP_A_LIB_DIR "${QUESTA_SIM_DIR}/${DEP_A_LIB}")
set(DEP_B_LIB_DIR "${QUESTA_SIM_DIR}/${DEP_B_LIB}")

set(QUESTA_INIT_COMMANDS)

rtl_import_modules(
    project_dep_a
    project_dep_b
)
```

`DEP_A_LIB`/`DEP_B_LIB` and `project_dep_a`/`project_dep_b` are illustrative slots. Replace them consistently with the project's established CMake variables, logical library values, and exact `ext/` directory names. Do not retain the illustrative names and do not change the surrounding structure. For example, a real variable named `FOO_LIB` has directory variable `FOO_LIB_DIR`.

Dependency import order must be topological. Each `ext/<dependency>/scripts/cmake/questa_modules.cmake` appends its ordered compile commands to `QUESTA_INIT_COMMANDS`.

For a project with no `ext/` RTL dependencies, retain `WORK_LIB`, `WORK_LIB_DIR`, and an empty `QUESTA_INIT_COMMANDS`; omit only the nonexistent dependency variables and import entries.

### 8.2 Compile project RTL, then the wrapper

Use one command per local product source and one separate final wrapper command:

```cmake
set(QUESTA_RTL_VLOG_COMMANDS)

foreach(RTL_SOURCE IN LISTS RTL_SOURCES)
    list(APPEND QUESTA_RTL_VLOG_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR}
        vlog -f ${VLOG_OPTIONS}
        -L ${DEP_A_LIB}
        -L ${DEP_B_LIB}
        -work ${WORK_LIB}
        ${RTL_SOURCE}
    )
endforeach()

list(APPEND QUESTA_RTL_VLOG_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR}
    vlog -f ${VLOG_OPTIONS}
    -L ${DEP_A_LIB}
    -L ${DEP_B_LIB}
    -work ${WORK_LIB}
    ${SIM_WRAPPER_SOURCE}
)
```

Repeat the actual `-L` entries mechanically for all project libraries. Do not invent a new helper abstraction. The separate final wrapper command is mandatory and must not be folded into `RTL_SOURCES`.

### 8.3 Standard `sim_questa_init`

Preserve the canonical target shape:

```cmake
add_custom_target(
    sim_questa_init
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${QUESTA_SIM_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${QUESTA_SIM_DIR}
    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} vlib ${WORK_LIB}
    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} vlib ${DEP_A_LIB}
    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} vlib ${DEP_B_LIB}
    COMMAND vmap ${WORK_LIB} ${WORK_LIB_DIR}
    COMMAND vmap ${DEP_A_LIB} ${DEP_A_LIB_DIR}
    COMMAND vmap ${DEP_B_LIB} ${DEP_B_LIB_DIR}
    ${QUESTA_INIT_COMMANDS}
    ${QUESTA_RTL_VLOG_COMMANDS}
    COMMAND bash -c "cd ${QUESTA_SIM_DIR} && vmake ${WORK_LIB} > ${WORK_LIB}.mak"
    COMMAND bash -c "cd ${QUESTA_SIM_DIR} && vmake ${DEP_A_LIB} > ${DEP_A_LIB}.mak"
    COMMAND bash -c "cd ${QUESTA_SIM_DIR} && vmake ${DEP_B_LIB} > ${DEP_B_LIB}.mak"
    COMMENT "Initial QuestaSim compilation and vmake makefile generation"
    VERBATIM
)
```

Repeat `vlib`, `vmap`, and `vmake` once per actual dependency. Do not leave placeholder tokens in committed CMake.

This target intentionally recreates only `${QUESTA_SIM_DIR}`, a build artifact directory. It must never remove source or repository directories.

### 8.4 Standard `sim_questa_compile`

```cmake
add_custom_target(
    sim_questa_compile
    COMMAND ${CMAKE_COMMAND} -E remove -f ${QUESTA_SIM_DIR}/.stamp
    COMMAND ${CMAKE_COMMAND} -E echo "Rebuilding QuestaSim libraries via make ..."
    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} make -f ${DEP_A_LIB}.mak
    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} make -f ${DEP_B_LIB}.mak
    COMMAND ${CMAKE_COMMAND} -E chdir ${QUESTA_SIM_DIR} make -f ${WORK_LIB}.mak
    COMMAND ${CMAKE_COMMAND} -E echo "QuestaSim make finished"
    COMMAND ${CMAKE_COMMAND} -E touch ${QUESTA_SIM_DIR}/.stamp
    COMMENT "Compiling all units into libraries (incremental via vmake)"
)
```

Compile dependencies before `work`. On a new build tree or after source/library topology changes, run `sim_questa_init` before `sim_questa_compile`; do not add a second competing initialization scheme.

### 8.5 Standard Questa run targets

```cmake
add_custom_target(
    sim_questa_run
    COMMAND env LD_PRELOAD=/usr/lib64/libstdc++.so.6
    vsim -c
    -f ${VSIM_OPTIONS}
    -L ${DEP_A_LIB}
    -L ${DEP_B_LIB}
    ${RTL_TOP_MODULE}
    -pli ${VPI_MODULE_NAME}
    -do ${QUESTA_RUN_SCRIPT}
    WORKING_DIRECTORY ${QUESTA_SIM_DIR}
    DEPENDS sim_questa_compile
    COMMENT "Running QuestaSim simulation with modern libstdc++ (batch mode)"
)

add_custom_target(
    sim_questa_run_gui
    COMMAND env LD_PRELOAD=/usr/lib64/libstdc++.so.6
    vsim -gui
    -f ${VSIM_OPTIONS}
    -L ${DEP_A_LIB}
    -L ${DEP_B_LIB}
    ${RTL_TOP_MODULE}
    -pli ${VPI_MODULE_NAME}
    -do ${QUESTA_RUN_GUI_SCRIPT}
    WORKING_DIRECTORY ${QUESTA_SIM_DIR}
    DEPENDS sim_questa_compile
    COMMENT "Running QuestaSim simulation with modern libstdc++ (GUI mode)"
)
```

Repeat the real `-L` list. `${RTL_TOP_MODULE}` must resolve to `dut_wrapper`.

The `LD_PRELOAD` line is part of the proven environment used by the reference flow. If the target environment does not have `/usr/lib64/libstdc++.so.6`, stop and report the platform incompatibility rather than silently substituting a different library path across only one project.

### 8.6 Standard Questa option and run files

The minimum reference intent is:

`scripts/sim/questa/vlog_options`:

```text
-64
-sv
<project-real +define+ entries>
```

`scripts/sim/questa/vsim_options`:

```text
-64
-t 1ps
-onfinish stop
```

Keep existing required project options, but `-onfinish stop` must be the final active, non-comment entry in `vsim_options`. This ensures `run -all` stops the simulator cleanly when the VIP/test flow calls simulation finish. Do not leave `-onfinish exit`, `-onfinish ask`, or another later option that overrides it.

`scripts/sim/questa/questa_run.tcl` must contain exactly:

```tcl
log -r /*
run -all
```

Do not use a fixed duration such as `run 1 us`, `run 100 ms`, or `run 1000000`. Do not append any active command after `run -all`.

`scripts/sim/questa/questa_run_gui.tcl` may retain project-specific waveform/watch setup, but its final active simulation command must be:

```tcl
run -all
```

Canonical GUI structure:

```tcl
log -r /*
# Preserve the project's existing wave/watch setup commands here.
run -all
```

Replace every finite-duration active `run <time>` command in the GUI script with the single final `run -all`. Commented examples do not affect execution, but remove or update stale finite-run comments when they could mislead future maintenance. Do not remove existing waveform setup merely to normalize the final run command.

If the GUI flow sources `scripts/sim/questa/wave.tcl`, another wave Tcl file, or contains inline `add wave`, `log`, `examine`, `force`, watch, or hierarchy-navigation commands, update that setup for the new top-level hierarchy in the same task.

Apply these hierarchy rules mechanically:

- former top-level DUT ports/signals now exposed by the wrapper use `/dut_wrapper/<signal>`;
- real-DUT internal signals and subinstances use `/dut_wrapper/u_dut/<internal-path>`;
- native-clock controls, generated clock nets, stopped acknowledgements, and `sim_keepalive` use `/dut_wrapper/<wrapper-signal>` when intentionally displayed;
- conditional-mode signals appear only under the same active compile-time conditions used by the wrapper and DUT;
- no active wave/watch command may retain `/<REAL_DUT_TOP>/...`, `sim:/<REAL_DUT_TOP>/...`, or another path rooted at the former simulation top.

Preserve useful waveform groups, dividers, radix settings, colors, formatting, and project-specific signal selection. Change only hierarchy paths and directly stale labels/comments. Do not replace the project's waveform layout with a generic full-hierarchy dump. If no waveform setup file or inline waveform configuration exists, do not create one merely for this migration; report that the waveform update was not applicable.

The mandatory cross-flow facts are SystemVerilog compilation, the correct project-mode definitions, 1 ps effective simulation precision, unbounded `run -all` execution in both Questa scripts, and `-onfinish stop` as the final active simulator option.

Do not copy `+define+MAC_1G_RGMII_SEL` into another project. Translate only its own real configuration.

---

## 9. Standard RTL dependency helper

If `scripts/cmake/rtl_dependency_helpers.cmake` already exists and matches the standard import contract, preserve it. If it is absent while the project uses `ext/` RTL dependencies, add the canonical helper below without project-specific forks:

```cmake
# scripts/cmake/rtl_dependency_helpers.cmake
#
# Helper macros for importing already-checked-out RTL dependency submodules.
# Expected layout:
#   <repo>/ext/<dependency>/scripts/cmake/questa_modules.cmake
#
# Macros are intentional so imported files can update caller-scope variables
# such as QUESTA_INIT_COMMANDS.

if(DEFINED RTL_DEPENDENCY_HELPERS_INCLUDED)
  return()
endif()
set(RTL_DEPENDENCY_HELPERS_INCLUDED TRUE)

macro(rtl_import_module module_name)
  if("${module_name}" STREQUAL "")
    message(FATAL_ERROR
      "rtl_import_module called with an empty module name"
    )
  endif()

  if(NOT DEFINED EXT_DIR)
    message(FATAL_ERROR
      "EXT_DIR is not defined before rtl_import_module(${module_name}). "
      "Define EXT_DIR in the top-level CMakeLists.txt, normally as "
      "\${CMAKE_CURRENT_LIST_DIR}/ext."
    )
  endif()

  if("${EXT_DIR}" STREQUAL "")
    message(FATAL_ERROR
      "EXT_DIR is empty before rtl_import_module(${module_name})"
    )
  endif()

  set(_rtl_dep_name "${module_name}")
  set(_rtl_dep_dir "${EXT_DIR}/${_rtl_dep_name}")
  set(_rtl_dep_import_file "${_rtl_dep_dir}/scripts/cmake/questa_modules.cmake")

  get_property(_rtl_dep_imported_modules GLOBAL PROPERTY RTL_IMPORTED_MODULES)
  if(NOT _rtl_dep_imported_modules)
    set(_rtl_dep_imported_modules "")
  endif()

  list(FIND _rtl_dep_imported_modules "${_rtl_dep_name}" _rtl_dep_index)

  if(_rtl_dep_index EQUAL -1)
    if(NOT EXISTS "${_rtl_dep_dir}")
      message(FATAL_ERROR
        "Missing RTL dependency submodule: ${_rtl_dep_dir}\n"
        "Expected imported submodule under EXT_DIR: ${EXT_DIR}"
      )
    endif()

    if(NOT IS_DIRECTORY "${_rtl_dep_dir}")
      message(FATAL_ERROR
        "RTL dependency path exists but is not a directory: ${_rtl_dep_dir}"
      )
    endif()

    if(NOT EXISTS "${_rtl_dep_import_file}")
      message(FATAL_ERROR
        "Missing Questa module import file for RTL dependency ${_rtl_dep_name}:\n"
        "  ${_rtl_dep_import_file}\n"
        "Expected standard filename:\n"
        "  scripts/cmake/questa_modules.cmake"
      )
    endif()

    message(STATUS "Importing RTL dependency: ${_rtl_dep_name}")

    set_property(GLOBAL APPEND PROPERTY RTL_IMPORTED_MODULES "${_rtl_dep_name}")

    include("${_rtl_dep_import_file}")
  else()
    message(STATUS "RTL dependency already imported: ${_rtl_dep_name}")
  endif()

  unset(_rtl_dep_name)
  unset(_rtl_dep_dir)
  unset(_rtl_dep_import_file)
  unset(_rtl_dep_imported_modules)
  unset(_rtl_dep_index)
endmacro()

macro(rtl_import_modules)
  foreach(_rtl_dep_batch_name ${ARGN})
    rtl_import_module(${_rtl_dep_batch_name})
  endforeach()

  unset(_rtl_dep_batch_name)
endmacro()
```

Do not make this helper fetch dependencies. Missing dependencies are explicit errors. Do not use `git submodule update --recursive` as a fallback.

---

## 10. Add the canonical Verilator flow

Append the following architecture after the Questa targets. Replace only project-real names, source entries, definitions, and required include options.

### 10.1 Fixed Verilator paths and plugin pairing

```cmake
# =============================================================================
# Verilator simulation flow
# =============================================================================

set(VERILATOR_TOP_MODULE "dut_wrapper")

set(VERILATOR_SIM_DIR "${CMAKE_BINARY_DIR}/verilator")
set(VERILATOR_OBJ_DIR "${VERILATOR_SIM_DIR}/obj_dir")
set(VERILATOR_EXE_NAME "sim_verilator")
set(VERILATOR_EXE "${VERILATOR_OBJ_DIR}/${VERILATOR_EXE_NAME}")

set(VERILATOR_VPI_MODULE_DIR
    "${CMAKE_SOURCE_DIR}/../${VIP_DESIGN_NAME}/cmake-build-verilator"
)
set(VERILATOR_VPI_MODULE_NAME
    "${VERILATOR_VPI_MODULE_DIR}/lib${VIP_DESIGN_NAME}.so"
)
```

Do not reuse the existing/Questa plugin for Verilator. The sibling VIP guide produces a distinct plugin against Verilator's VPI header in `cmake-build-verilator`.

### 10.2 Build a flat dependency-ordered manifest

Questa logical libraries do not exist in the `--binary` Verilator compile. Expand all required RTL dependencies into one explicit flat list:

```cmake
# Verilator consumes a flat, dependency-ordered source manifest rather than
# the compiled logical libraries used by Questa.
set(VERILATOR_DEP_SOURCES
    "${EXT_DIR}/<dependency_1>/src/<package_or_interface_first>.sv"
    "${EXT_DIR}/<dependency_1>/src/<consumer_next>.sv"

    "${EXT_DIR}/<dependency_2>/src/<package_first>.sv"
    "${EXT_DIR}/<dependency_2>/src/<modules_in_order>.sv"
)

set(VERILATOR_SIM_SOURCES
    ${VERILATOR_DEP_SOURCES}
    ${RTL_SOURCES}
    ${SIM_WRAPPER_SOURCE}
)
```

Derive `VERILATOR_DEP_SOURCES` from the real transitive dependency manifests. Do not copy filenames from another project.

Manifest rules:

- include every source required to elaborate the real top;
- exclude unused dependency sources where the known product does not need them;
- packages and interfaces precede consumers;
- lower-level modules precede aggregators/tops;
- do not duplicate a file across dependency and project lists;
- do not include testbench sources, `dut_wrapper.sv` twice, vendor synthesis-only files, constraints, or the VIP C++ source;
- do not use logical-library flags as a substitute for the flat list;
- do not use globs.

### 10.3 Canonical Verilator options

Use this exact fixed core and append only proven project-real `-D`, `-U`, `-I`, or parameter options at the marked location:

```cmake
set(VERILATOR_OPTIONS
    --binary
    --vpi
    --top-module ${VERILATOR_TOP_MODULE}
    --threads 1
    --public-flat-rw
    -Wno-fatal
    --timescale 1ns/1ps
    --default-language 1800-2012
    +1800-2012ext+sv

    # Project-real configuration only. Examples of syntax, not values to copy:
    # -D<ACTIVE_DEFINE>
    # -U<INACTIVE_DEFINE>
    # -I${SRC_DIR}/<include_directory>
    # -G<TOP_PARAMETER>=<value>

    --Mdir ${VERILATOR_OBJ_DIR}
    -o ${VERILATOR_EXE_NAME}
)
```

Do not add `MAC_1G_RGMII_SEL` unless it is genuinely this project and the selected configuration requires it.

The fixed options mean:

| Option | Required purpose |
|---|---|
| `--binary` | Use Verilator's standard timed executable generation; no custom harness |
| `--vpi` | Enable VPI and plugin loading |
| `--top-module dut_wrapper` | Match Questa/VIP hierarchy |
| `--threads 1` | Preserve deterministic single-thread reference behavior |
| `--public-flat-rw` | Retain public flat access needed by the VPI-visible wrapper contract |
| `-Wno-fatal` | Do not make unrelated warnings fatal |
| `--timescale 1ns/1ps` | Establish the common time unit/precision fallback |
| `--default-language 1800-2012` | SystemVerilog 2012 default |
| `+1800-2012ext+sv` | Treat `.sv` as SystemVerilog 2012 |
| `--Mdir .../obj_dir` | Fixed isolated output tree |
| `-o sim_verilator` | Fixed executable name |

Do not add broad warning suppressions to hide errors. Do not add `--no-timing`. Do not add a custom `main()`.

The proven reference option block intentionally does not add a separate `--timing` argument; reproduce the block as shown. If the installed Verilator version rejects timed constructs unless `--timing` is explicit, stop and report that version/tool incompatibility instead of creating a per-project option variant. A common tool-version decision can then be made once and propagated to this specification and all projects.

### 10.4 Definition/include/parameter parity gate

Before building, create and verify this table:

| Elaboration fact | Questa source | Verilator equivalent | VIP expectation | Match? |
|---|---|---|---|---|
| Active mode define | `+define+NAME` in `vlog_options` | `-DNAME` | typed VIP mode/config | yes/no |
| Explicit undefine | absence or project option | `-UNAME` only if required | active interface choice | yes/no |
| Include path | Questa `+incdir+...`/option | `-I...` | N/A | yes/no |
| Top parameter override | `vsim -g...` or default | `-GNAME=value` or same default | pin widths/config | yes/no |
| Time precision | `vsim -t 1ps` | `--timescale 1ns/1ps` plus wrapper precision | VPI precision `-12` | yes/no |
| Top module | `dut_wrapper` | `dut_wrapper` | `setDutName("dut_wrapper")` | yes/no |

Do not proceed when any row is unknown or mismatched. A successful compile in two different modes is not a valid dual-simulator result.

### 10.5 Canonical Verilator compile target

```cmake
add_custom_target(
    sim_verilator_compile
    COMMAND ${CMAKE_COMMAND} -E make_directory ${VERILATOR_SIM_DIR}
    COMMAND verilator --version
    COMMAND verilator ${VERILATOR_OPTIONS} ${VERILATOR_SIM_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building dut_wrapper with Verilator VPI support (<PROJECT_MODE>)"
    USES_TERMINAL
    VERBATIM
)
```

Replace only the human-readable `<PROJECT_MODE>` comment. Do not rename the target.

### 10.6 Canonical Verilator run target

```cmake
add_custom_target(
    sim_verilator_run
    COMMAND ${CMAKE_COMMAND} -E echo
            "Using Verilator VPI plugin: ${VERILATOR_VPI_MODULE_NAME}"
    COMMAND bash -c
            "test -f '${VERILATOR_VPI_MODULE_NAME}' || { echo 'ERROR: Verilator VIP plugin missing: ${VERILATOR_VPI_MODULE_NAME}. Build ${VIP_DESIGN_NAME} target verilator_<VIP_LOGICAL_NAME> first.'; exit 1; }"
    COMMAND bash -c
            "test -x '${VERILATOR_EXE}' || { echo 'ERROR: Verilator executable missing: ${VERILATOR_EXE}'; exit 1; }"
    COMMAND ${VERILATOR_EXE}
            "+verilator+vpi+${VERILATOR_VPI_MODULE_NAME}"
    WORKING_DIRECTORY ${VERILATOR_SIM_DIR}
    DEPENDS sim_verilator_compile
    COMMENT "Running dut_wrapper with Verilator + RapidVPI (<PROJECT_MODE>)"
    USES_TERMINAL
    VERBATIM
)
```

Replace `<VIP_LOGICAL_NAME>` with the exact suffix of the paired VIP target and `<PROJECT_MODE>` in the comment. The command, directory, dependency, and load syntax remain fixed.

Do not make the RTL CMake configure/build the sibling VIP implicitly. The explicit file check gives a clear prerequisite and keeps the two repositories/build caches separate.

---

## 11. Keep synthesis and reusable RTL manifests clean

The wrapper must appear in exactly these root simulation locations:

- `${SIM_WRAPPER_SOURCE}`;
- final separate Questa `vlog` command;
- final element of `VERILATOR_SIM_SOURCES`.

It must not appear in:

- `RTL_SOURCES`;
- project `scripts/cmake/questa_modules.cmake` when exporting the RTL project as a reusable dependency;
- dependency `questa_modules.cmake` files;
- Vivado `read_sources.tcl`;
- ISE `.xst`/`.prj` source lists;
- Quartus assignments;
- synthesis file lists;
- package manifests for downstream RTL consumers;
- formal product source lists unless a separate simulation-only harness explicitly imports it;
- lint source lists intended to enforce synthesizable design rules.

The project source list itself should remain synchronized across its product manifests. If an older project has stale manifests, update only the explicit synthesizable file membership/order necessary to make them describe the same real product. Do not use this migration as permission to reorganize `src/`.

---

## 12. Paired RTL/VIP contract

The RTL refactor is only half of the native-clock architecture. Define the paired VIP contract from the completed wrapper and capture it in the user-supplied root `user_guide.md` without entering or editing sibling VIP source.

For each wrapper-owned clock `<clock>`, the paired VIP must eventually have:

```text
actual waveform:             <clock>                 width 1
enable control:              sim_<clock>_enable      width 1
full-period control:         sim_<clock>_period_ticks width 64
stopped acknowledgement:     sim_<clock>_stopped     width 1
```

It must:

- set the DUT name to `dut_wrapper`;
- register all four names;
- construct one `vip::common::Clock` with `NativeClockCfg` per independent wrapper-owned clock;
- never directly drive `<clock>` edge-by-edge;
- never control DUT-generated output clocks;
- never register or use `sim_keepalive`;
- use a typed configuration matching the RTL compile-time mode;
- build its Verilator plugin at `cmake-build-verilator/lib${VIP_DESIGN_NAME}.so` through target `verilator_<logical_name>`.

If the sibling VIP still uses the old bare DUT top or legacy per-edge VPI clock generation, complete the separate `vip_refactor.md` task after this RTL task. Do not alter the RTL contract to preserve an obsolete VIP implementation.

---

## 13. Minimal Verilator portability repairs

First add the wrapper and build integration without changing synthesizable RTL. Compile. Only then consider source repairs.

A synthesizable source edit is allowed only when all are true:

1. Verilator emits a concrete error, not merely a warning;
2. the construct is illegal, ambiguous, or unsupported for the stated SystemVerilog 2012 flow;
3. the smallest replacement preserves hardware semantics, reset behavior, widths, signedness, and synthesis intent;
4. Questa still compiles the same source after the repair;
5. vendor synthesis manifests remain valid;
6. the change is directly documented in the handoff.

Examples of acceptable repair categories when proven by diagnostics:

- making an implicit width/sign conversion explicit;
- correcting a package/interface compile order;
- adding a genuinely missing include path or definition to both flows;
- replacing a nonportable but equivalent declaration syntax with legal SystemVerilog 2012;
- removing an accidental multiple driver that was already invalid, without redesigning behavior.

Not acceptable:

- changing protocol timing to pass a testcase;
- adding simulator-name `ifdef` branches to functional behavior;
- weakening reset or initialization behavior;
- changing an FSM architecture;
- changing public parameters or ports;
- broad warning suppression;
- deleting modules or sources to reduce compile scope;
- converting unknown/X behavior into a convenient constant;
- introducing `#` delays into synthesizable RTL;
- changing assertions/test expectations merely because Verilator reports a difference.

Warnings remain visible and non-fatal under the standard option. Report relevant warnings; do not launch a cleanup campaign.

---

## 14. Documentation and VIP user-guide handoff

### 14.1 Update RTL-project documentation

Update the project-specific design/user guide with a concise integration section. It must state:

```text
wrapper file:      dut_wrapper.sv
simulation top:    dut_wrapper
real RTL top:      <REAL_DUT_TOP>
DUT instance:      u_dut
wrapper precision: 1 ps
```

Add a clock table:

| Clock | Real DUT direction | Owner | Build-mode presence | Wrapper controls/default | Notes |
|---|---|---|---|---|---|
| project-specific | input/output | wrapper/DUT/protocol | exact condition | exact triplet or N/A | nominal period/observe-only |

Document:

- which real clock inputs disappear from the public wrapper port list because they are generated internally;
- which output clocks remain observation-only;
- the exact active compile-time mode and how Questa/Verilator select it;
- that all clock enables initialize to zero;
- that one period tick is 1 ps;
- that VIP programs the period before enable;
- that the first rising edge occurs in the enable-write time slot;
- that stop completes at a half-cycle boundary and parks low;
- that `sim_keepalive` is infrastructure only;
- that `dut_wrapper.sv` is excluded from synthesis;
- the exact build commands for both flows and the sibling VIP prerequisite.

Do not duplicate project-independent generator semantics differently. Refer to `docs/rtl_design_guide.md` as the universal authority and keep project documentation focused on actual project facts.

### 14.2 Patch the staged VIP-facing `user_guide.md`

The user places the current paired VIP guide at repository root as:

```text
user_guide.md
```

Patch this file in place after `dut_wrapper.sv`, CMake integration, clock ownership, compile-time mode, and wrapper-visible names are final. Leave the patched file at repository root for the user to copy back into `vip_<project>/docs/user_guide.md`. Do not move it into the RTL `docs/` directory, delete it, or write directly into the sibling VIP repository.

Preserve all still-valid content, organization, examples, testcase descriptions, and usage guidance. Add or update the smallest coherent section needed to make the guide a complete input to the later VIP refactor. Update its clickable table of contents when a new heading is introduced.

The patched VIP guide must state, using exact project-real names:

- simulation top `dut_wrapper`, not the former bare RTL top;
- real synthesizable top `<REAL_DUT_TOP>` instantiated exactly once as `dut_wrapper.u_dut`;
- `dut_wrapper.sv` is simulation-only and excluded from synthesis;
- wrapper parameter pass-through and the exact wrapper-visible functional port contract, including widths, unpacked dimensions, signedness, and compile-time conditional groups relevant to VIP pin registration;
- every reset's wrapper-visible name, polarity, ownership, and whether reset remains VIP-driven;
- every external/VIP-owned real-DUT input clock that is now generated internally by the wrapper;
- the VPI-visible generated waveform name `<clock>` for each such clock even though it is no longer a public wrapper input port;
- the exact `sim_<clock>_enable`, `sim_<clock>_period_ticks`, and `sim_<clock>_stopped` names and 1/64/1 widths;
- nominal full-period defaults in 1 ps ticks, initially-disabled behavior, low parked state, period-before-enable ordering, deterministic first edge, and stopped acknowledgement behavior;
- every DUT-generated or protocol-generated clock that remains observation-only or protocol-owned and therefore has no native-clock control triplet;
- the exact compile-time mode selected by both Questa and Verilator and which wrapper ports/clocks exist in that mode;
- that the VIP shall use `vip::common::Clock` with `NativeClockCfg` for each wrapper-owned clock and shall not drive those clock nets edge-by-edge;
- that `sim_keepalive` is wrapper infrastructure only and must never be registered, driven, awaited, or treated as testcase state by the VIP;
- Questa and Verilator plugin/build prerequisites and the expected sibling plugin flavors/paths relevant to the later VIP refactor;
- any hierarchy-sensitive diagnostic or waveform paths that now descend through `dut_wrapper.u_dut`.

Do not expose internal RTL architecture merely because `u_dut` now exists. The VIP guide describes the simulation boundary and how verification must bind to it, not how the product is implemented internally. Do not copy large universal clock-generator internals into the VIP guide; provide the exact behavioral contract the VIP needs.

Before handoff, compare the patched guide directly against `dut_wrapper.sv` and CMake. There must be no stale instruction to elaborate the old bare top, no instruction to drive a wrapper-owned clock directly, no missing native control/status net, no wrong width, and no mode-inactive port presented as universally available.

---

## 15. Static conformance audit

Run these checks after editing and interpret the results; do not merely paste output:

```bash
git status --short

rg -n 'set\((RTL_TOP_MODULE|VERILATOR_TOP_MODULE) "dut_wrapper"\)' CMakeLists.txt
rg -n 'set\(SIM_WRAPPER_SOURCE .*dut_wrapper\.sv' CMakeLists.txt
rg -n 'sim_questa_(init|compile|run|run_gui)|sim_verilator_(compile|run)' CMakeLists.txt
rg -n 'VERILATOR_(SIM_DIR|OBJ_DIR|EXE_NAME|VPI_MODULE_DIR|VPI_MODULE_NAME)' CMakeLists.txt
rg -n -- '--binary|--vpi|--public-flat-rw|--timescale|--default-language|1800-2012|\+verilator\+vpi\+' CMakeLists.txt

awk 'NF && $1 !~ /^#/ {print}' scripts/sim/questa/questa_run.tcl
awk 'NF && $1 !~ /^#/ {last=$0} END {print last}' scripts/sim/questa/questa_run_gui.tcl
awk 'NF && $1 !~ /^#/ {last=$0} END {print last}' scripts/sim/questa/vsim_options
rg -n -P '^\s*run\s+(?!-all(?:\s|$))' scripts/sim/questa/questa_run.tcl scripts/sim/questa/questa_run_gui.tcl

rg -n '^module sim_native_clock_gen|^module dut_wrapper|\) u_dut \(' dut_wrapper.sv
rg -n 'timeunit 1ns; timeprecision 1ps;' dut_wrapper.sv
rg -n 'sim_[A-Za-z0-9_]+_(enable|period_ticks|stopped)|public_flat_(rw|rd)' dut_wrapper.sv
rg -n 'sim_keepalive|#1s' dut_wrapper.sv

rg -n 'dut_wrapper\.sv|SIM_WRAPPER_SOURCE' . \
  -g '!cmake-build*/**' -g '!build/**' -g '!docs/**'

test -f user_guide.md
rg -n 'dut_wrapper|u_dut|sim_[A-Za-z0-9_]+_(enable|period_ticks|stopped)|sim_keepalive' user_guide.md

rg -n 'add wave|log |examine|force|view wave|do .*wave.*\.tcl|source .*wave.*\.tcl' \
  scripts/sim/questa -g '*.tcl'
rg -n '(/|sim:/)<REAL_DUT_TOP>(/|$)' scripts/sim/questa -g '*.tcl'
```

Manually prove:

- `dut_wrapper.sv` is absent from `RTL_SOURCES`;
- wrapper occurrence in CMake is limited to the separate simulation variable/commands;
- wrapper does not occur in synthesis manifests;
- the batch Tcl active lines are exactly `log -r /*` and `run -all`;
- the GUI Tcl final active simulation command is `run -all` and no active finite-duration `run` remains;
- the final active `vsim_options` entry is `-onfinish stop`;
- control groups exactly match the clock inventory;
- Verilator flat sources equal the transitive compile closure;
- definitions and include paths match Questa;
- all CMake placeholders and example tokens are removed;
- root `user_guide.md` matches the final wrapper ports, parameters, clock controls, ownership, widths, conditional mode, and plugin prerequisites;
- root `user_guide.md` no longer instructs the VIP to elaborate the former bare top or directly drive a wrapper-owned clock;
- existing waveform files resolve wrapper signals below `/dut_wrapper` and real-DUT internals below `/dut_wrapper/u_dut`;
- no active waveform/watch command remains rooted at the former simulation top.

Useful placeholder search before handoff:

```bash
rg -n '<(RTL_PROJECT_NAME|REAL_DUT_TOP|VIP_PROJECT_NAME|VIP_LOGICAL_NAME|PROJECT_MODE|clock|dependency|PARAM|port)' . \
  -g 'CMakeLists.txt' -g 'dut_wrapper.sv' -g 'user_guide.md' -g '*.cmake' -g '*.tcl'
```

No placeholder may remain in committed implementation files.

---

## 16. Configure and build qualification

Obey repository instructions about simulation execution. Compilation is required for this refactor; full simulation runs are required only when the user's task and applicable repository instructions authorize them.

Use the project's normal RTL build directory, represented below as `<RTL_BUILD_DIR>`.

### 16.1 Configure

```bash
cmake -S . -B <RTL_BUILD_DIR> -DCMAKE_BUILD_TYPE=Release
```

Do not reuse a sibling VIP build directory for RTL.

### 16.2 Questa initialization and compile

Because source/library topology changed, regenerate the Questa `vmake` files:

```bash
cmake --build <RTL_BUILD_DIR> --target sim_questa_init
cmake --build <RTL_BUILD_DIR> --target sim_questa_compile
```

Confirm the compile log shows:

1. dependency libraries in order;
2. project synthesizable sources in order;
3. `dut_wrapper.sv` last in `work`;
4. no attempt to compile the wrapper for synthesis;
5. no missing or multiply defined module/package.

### 16.3 Build the paired Verilator VIP plugin

From the sibling VIP repository, use its canonical convenience target:

```bash
cmake --build cmake-build-release --target verilator_<VIP_LOGICAL_NAME> --parallel
```

Expected byproduct:

```text
../<VIP_PROJECT_NAME>/cmake-build-verilator/lib<VIP_PROJECT_NAME>.so
```

If the paired VIP refactor has not yet been completed, record that as a run prerequisite. The RTL executable can still be compiled independently.

### 16.4 Verilator compile

```bash
cmake --build <RTL_BUILD_DIR> --target sim_verilator_compile
```

Confirm:

- reported Verilator version is recorded;
- top is `dut_wrapper`;
- product mode/defines match Questa;
- the flat source closure is complete and nonduplicated;
- executable exists at `<RTL_BUILD_DIR>/verilator/obj_dir/sim_verilator`;
- wrapper control/status nets remain VPI-visible;
- no functional source was removed to make compilation succeed.

### 16.5 Run qualification when authorized

Questa batch:

```bash
cmake --build <RTL_BUILD_DIR> --target sim_questa_run
```

Verilator:

```bash
cmake --build <RTL_BUILD_DIR> --target sim_verilator_run
```

For both flows verify:

- plugin path is the intended flavor;
- VPI top is `dut_wrapper`;
- required nets resolve below `dut_wrapper`;
- clocks start disabled, VIP programs periods, and starts them through `sim_*_enable`;
- no protocol agent or legacy task also drives a wrapper-owned clock;
- reset remains controlled through the established test API;
- mode-specific ports match the active compile configuration;
- when the GUI flow is exercised, its waveform script loads without stale-path errors and displays wrapper signals below `dut_wrapper` and intended DUT internals below `dut_wrapper/u_dut`;
- stopping all clocks does not cause premature Verilator termination;
- simulation completion occurs through the existing project/VIP completion path.

Do not change test intent to force cross-simulator agreement. Diagnose configuration, scheduling, ownership, or genuine RTL portability issues.

### 16.6 Re-run incremental Questa compile

After successful initialization, prove the preserved incremental path:

```bash
cmake --build <RTL_BUILD_DIR> --target sim_questa_compile
```

This confirms generated `.mak` files include `dut_wrapper.sv` and remain usable.

---

## 17. Failure diagnosis order

When a target fails, diagnose in this order:

1. **Missing dependency/file** — compare explicit manifests; do not fetch recursively.
2. **Package/interface not found** — fix source or dependency order.
3. **Mode-specific port mismatch** — compare Questa define, Verilator `-D/-U`, wrapper condition, real top condition, and VIP typed mode.
4. **Width/parameter mismatch** — compare wrapper pass-through, simulator overrides, and VIP pin widths.
5. **VPI net not found** — confirm top `dut_wrapper`, exact original clock name, `sim_*` spelling, visibility annotation, and active conditional group.
6. **Plugin missing** — build the sibling VIP `verilator_<logical_name>` target; do not point Verilator at the existing-flow plugin.
7. **VPI symbol/link failure** — confirm the VIP plugin was built with `VIP_VPI_FLAVOR=VERILATOR` and is loaded by the Verilator executable.
8. **Clock does not start** — verify period registration/write precedes enable and the control tick is 1 ps.
9. **Clock does not stop** — wait for `sim_<clock>_stopped`; stop occurs at a half-cycle boundary and parks low.
10. **Verilator exits when clocks stop** — verify the exact keepalive block and timed compile behavior.
11. **One simulator passes, one fails** — prove configuration parity before changing RTL or tests.
12. **Compiler rejects synthesizable source** — apply only the minimal repair policy in Section 13.

Never start by changing testcase expectations or adding arbitrary delays.

---

## 18. Anti-drift review for repeated Codex use

Before declaring success, compare the implementation against this matrix:

| Mechanism | Must be identical across projects | May be project-specific |
|---|---|---|
| Wrapper file/module | `dut_wrapper.sv` / `dut_wrapper` | no |
| DUT instance | `u_dut` | instantiated module, params, ports |
| Native generator | exact module/behavior | number of instances only |
| Clock controls | `sim_<clock>_{enable,period_ticks,stopped}` and 1/64/1 | `<clock>` names/default periods |
| Visibility | exact `public_flat_rw/rd` placement | number/conditional presence |
| Time unit | 1 ns / 1 ps | no |
| Keepalive | exact `sim_keepalive` block | no |
| Questa top/targets | fixed names/structure | sources, libraries, mode options |
| Verilator top/targets | fixed names/structure | sources, definitions, includes, comments |
| Verilator executable | `verilator/obj_dir/sim_verilator` | no |
| VIP plugin convention | sibling `cmake-build-verilator/libvip_*.so` | project name/target suffix |
| VIP guide handoff | supplied root `user_guide.md` patched in place; top `dut_wrapper`; instance `u_dut`; exact native-clock contract | actual ports, parameters, clocks, modes, defaults, and plugin names |
| Waveform hierarchy | wrapper paths below `/dut_wrapper`; DUT internals below `/dut_wrapper/u_dut`; no stale former-top paths | selected signals, groups, radix, formatting, and conditional-mode content |
| Synthesis exclusion | wrapper always excluded | product source membership |
| Functional RTL | preserved | minimal proven compile repair only |

Reject an implementation that adds a new abstraction or naming variant even if its tests pass. Passing behavior is necessary but does not satisfy cross-project standardization by itself.

---

## 19. Required handoff report

The implementing Codex shall finish with a concise evidence-based report containing:

### 19.1 Files changed

List every changed file and one-line purpose. Explicitly distinguish:

- new/canonical simulation infrastructure;
- CMake/build integration;
- RTL-project documentation;
- the staged root `user_guide.md` patched for later VIP handoff;
- waveform hierarchy updates, or an explicit statement that no applicable waveform setup existed;
- any synthesizable portability repair.

### 19.2 Final project mapping

Report:

- RTL project name;
- real synthesizable top;
- simulation top;
- sibling VIP project and Verilator target;
- wrapper parameters/port contract source;
- wrapper-owned clocks and default periods;
- DUT-owned/observe-only clocks;
- active compile-time mode;
- staged VIP guide source/location and the wrapper-contract section added or updated;
- waveform script path and its final hierarchy roots, when applicable;
- dependency libraries and flat Verilator dependency count.

### 19.3 Source-separation proof

State that:

- `RTL_SOURCES` is synthesizable only;
- wrapper is compiled separately for Questa;
- wrapper is last in `VERILATOR_SIM_SOURCES`;
- wrapper is absent from synthesis/export manifests.

### 19.4 Commands and exact outcomes

Report configure/build/run commands actually executed and whether each passed, failed, or was not authorized:

- CMake configure;
- `sim_questa_init`;
- `sim_questa_compile`;
- sibling `verilator_<logical_name>` plugin build;
- `sim_verilator_compile`;
- `sim_questa_run`, if authorized;
- `sim_verilator_run`, if authorized;
- incremental `sim_questa_compile`.

Include the first relevant diagnostic for any failure, not only a generic status.

### 19.5 Preserved behavior statement

Confirm explicitly:

- real product ports/parameters and synthesis top were not changed;
- no testcase intent was changed;
- no unrelated refactor was performed;
- no submodule topology was changed;
- sibling VIP source was not inspected or modified and only the user-supplied root `user_guide.md` copy was patched;
- existing waveform selection/formatting was preserved apart from required hierarchy repairs;
- any pre-existing user edits were preserved.

### 19.6 Remaining blockers

Name exact blockers such as unavailable Questa license/tool, missing sibling VIP plugin, missing checked-out dependency, ambiguous project documentation, or an approved exception still required. Do not describe a blocked task as complete.

---

## 20. Final mandatory checklist

### Wrapper

- [ ] Root `dut_wrapper.sv` exists.
- [ ] Modules are `sim_native_clock_gen` then `dut_wrapper`.
- [ ] Real top is instantiated once as `u_dut`.
- [ ] Real parameters are mirrored and explicitly passed.
- [ ] Normal real ports are preserved.
- [ ] Wrapper-owned clock inputs are internal generated nets.
- [ ] DUT-generated output clocks remain observe-only outputs.
- [ ] One exact control triplet and generator instance exists per independent external clock.
- [ ] Clock control widths are 1/64/1.
- [ ] Enable initial values are zero.
- [ ] Periods are full-period 1 ps ticks.
- [ ] Correct Verilator visibility annotations are present.
- [ ] Conditional clock groups match the real top.
- [ ] Exact keepalive exists and is isolated.

### CMake and manifests

- [ ] `RTL_TOP_MODULE` is `dut_wrapper`.
- [ ] `VERILATOR_TOP_MODULE` is `dut_wrapper`.
- [ ] `SIM_WRAPPER_SOURCE` is separate from `RTL_SOURCES`.
- [ ] Local synthesizable sources are explicit and ordered.
- [ ] Questa dependency libraries/imports are complete and ordered.
- [ ] Wrapper compiles after project RTL in Questa.
- [ ] Questa elaborates `dut_wrapper` and loads the existing plugin.
- [ ] Batch `questa_run.tcl` contains exactly `log -r /*` and `run -all`.
- [ ] GUI `questa_run_gui.tcl` ends operationally with `run -all` and has no active finite-duration `run`.
- [ ] The final active `vsim_options` entry is `-onfinish stop`.
- [ ] Verilator flat dependency manifest is complete and ordered.
- [ ] `VERILATOR_SIM_SOURCES` ends with the wrapper.
- [ ] Canonical Verilator options are present.
- [ ] Project defines/includes/parameters match Questa.
- [ ] Targets are exactly `sim_verilator_compile` and `sim_verilator_run`.
- [ ] Executable path/name is canonical.
- [ ] Verilator run target uses the separate sibling plugin path and load syntax.
- [ ] Wrapper is absent from all synthesis/export manifests.

### Documentation and waveforms

- [ ] User-supplied root `user_guide.md` exists and was read completely before patching.
- [ ] Existing VIP usage/testcase content in `user_guide.md` was preserved.
- [ ] Its table of contents was updated if a wrapper-contract heading was added.
- [ ] It names simulation top `dut_wrapper` and real DUT instance `dut_wrapper.u_dut`.
- [ ] It accurately lists wrapper-visible functional ports, parameters, resets, widths, dimensions, compile-time conditions, and active mode needed by the VIP.
- [ ] Every wrapper-owned clock has the exact waveform name and 1/64/1 `sim_*` control/status triplet documented.
- [ ] DUT-generated and protocol-generated clocks are correctly documented as observation-only or protocol-owned.
- [ ] It requires `vip::common::Clock`/`NativeClockCfg` and forbids direct edge-by-edge driving of wrapper-owned clocks.
- [ ] It identifies `sim_keepalive` as non-VIP infrastructure.
- [ ] It contains no stale instruction to elaborate the former bare top or use obsolete clock ownership.
- [ ] The patched guide remains at RTL repository root for user handoff; sibling VIP source was not modified.
- [ ] Existing `wave.tcl` or equivalent files use `/dut_wrapper` for wrapper signals and `/dut_wrapper/u_dut` for DUT internals.
- [ ] No active waveform/watch path remains rooted at the former simulation top.
- [ ] Waveform groups, radix, formatting, and useful signal selection were preserved; if no waveform setup existed, non-applicability was reported instead of inventing one.

### Qualification

- [ ] Static anti-placeholder and source-separation audit passed.
- [ ] CMake configured successfully.
- [ ] `sim_questa_init` passed.
- [ ] `sim_questa_compile` passed.
- [ ] `sim_verilator_compile` passed.
- [ ] Paired plugin exists or is reported as an exact prerequisite.
- [ ] Authorized Questa run passed or exact blocker is recorded.
- [ ] Authorized Verilator run passed or exact blocker is recorded.
- [ ] Incremental Questa compile passed.
- [ ] No tests/sources/checks were deleted or weakened.
- [ ] No unrelated RTL change was made.
- [ ] Dirty-worktree state was preserved and reported.

---

## Final instruction to the implementing Codex

Use this document as a conformance specification. Reproduce the fixed architecture literally and substitute only facts proven from the current project. Do not optimize the common snippets and do not develop another equivalent mechanism.

The expected result is deliberately repetitive across repositories:

```text
same wrapper name
same native generator
same control naming and semantics
same VPI visibility
same keepalive
same source separation
same Questa target architecture
same Verilator target architecture
same plugin path convention
same staged VIP user-guide handoff discipline
same waveform hierarchy rebasing rule
same validation sequence
```

Only the following should tell one RTL project from another:

```text
real project name and top
real source files and dependencies
real parameters and I/O
real compile-time product mode
real external-clock inventory and nominal periods
real VIP-facing wrapper port/reset contract
real waveform groups and project-specific signal selection
```

When those facts cannot be proved, stop and report the missing contract. Do not guess, improvise, or silently diverge.
