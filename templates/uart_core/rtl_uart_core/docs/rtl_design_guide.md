# General RTL Design Guide

This guide defines the required coding style for synthesizable RTL in this project. It is intended to be strict enough that a smaller AI agent can generate code that matches the expected style without guessing.

---

## Table of contents

* [1. Language and coding style](#1-language-and-coding-style)
  * [1.1 Language](#11-language)
  * [1.2 Modern RTL only](#12-modern-rtl-only)
* [2. Nettype rule](#2-nettype-rule)
  * [2.1 Top of file](#21-top-of-file)
  * [2.2 End of file](#22-end-of-file)
* [3. Reset policy](#3-reset-policy)
  * [3.1 General rule](#31-general-rule)
  * [3.2 Asynchronous reset](#32-asynchronous-reset)
  * [3.3 No extra reset inventions](#33-no-extra-reset-inventions)
* [4. FSM policy](#4-fsm-policy)
  * [4.1 Required FSM style](#41-required-fsm-style)
  * [4.2 Forbidden FSM style](#42-forbidden-fsm-style)
  * [4.3 Default state handling](#43-default-state-handling)
  * [4.4 Output behavior](#44-output-behavior)
* [5. Combinational logic policy](#5-combinational-logic-policy)
  * [5.1 Allowed forms](#51-allowed-forms)
  * [5.2 Forbidden form](#52-forbidden-form)
  * [5.3 `always_comb` style](#53-always_comb-style)
* [6. Synthesizable-only RTL policy](#6-synthesizable-only-rtl-policy)
  * [6.1 General rule](#61-general-rule)
  * [6.2 Top-of-file simulation-related items](#62-top-of-file-simulation-related-items)
* [7. Standard RTL debug capability](#7-standard-rtl-debug-capability)
* [7.1 Debug macro header snippet](#71-debug-macro-header-snippet)
* [7.2 Standard debug parameters](#72-standard-debug-parameters)
* [7.3 Debug time policy](#73-debug-time-policy)
* [7.4 Standard debug print insertion style](#74-standard-debug-print-insertion-style)
  * [7.5 Message style](#75-message-style)
  * [7.6 Where to add debug prints](#76-where-to-add-debug-prints)
* [8. Parameters and widths](#8-parameters-and-widths)
  * [8.1 Typed parameters](#81-typed-parameters)
  * [8.2 Width cleanliness](#82-width-cleanliness)
  * [8.3 Derived width style](#83-derived-width-style)
  * [8.4 Counters and indices](#84-counters-and-indices)
* [9. Signal and module organization](#9-signal-and-module-organization)
  * [9.1 Use `logic`](#91-use-logic)
  * [9.2 File organization](#92-file-organization)
  * [9.3 Readability](#93-readability)
* [10. Preferred module template](#10-preferred-module-template)
* [11. Rules for AI/code-generation agents](#11-rules-for-aicode-generation-agents)
* [12. Input signals](#12-input-signals)
* [13. Do not try to run sims until I specifically tell you](#13-do-not-try-to-run-sims-until-i-specifically-tell-you)
* [14. FSM Encoding style](#14-fsm-encoding-style)
* [15. Standard simulation DUT wrapper architecture](#15-standard-simulation-dut-wrapper-architecture)
  * [15.1 Purpose and scope](#151-purpose-and-scope)
  * [15.2 Simulation-only exception to the normal RTL rules](#152-simulation-only-exception-to-the-normal-rtl-rules)
  * [15.3 Required file structure and style](#153-required-file-structure-and-style)
  * [15.4 Decide which clocks the wrapper owns](#154-decide-which-clocks-the-wrapper-owns)
  * [15.5 Mandatory clock-control naming contract](#155-mandatory-clock-control-naming-contract)
  * [15.6 Standard `sim_native_clock_gen` behavior](#156-standard-sim_native_clock_gen-behavior)
  * [15.7 Wrapper ports, parameters, and DUT instance](#157-wrapper-ports-parameters-and-dut-instance)
  * [15.8 Mandatory top-of-file clock documentation](#158-mandatory-top-of-file-clock-documentation)
  * [15.9 Standard Verilator keepalive](#159-standard-verilator-keepalive)
  * [15.10 Build and hierarchy rules](#1510-build-and-hierarchy-rules)
  * [15.11 Retrofitting an older project](#1511-retrofitting-an-older-project)
  * [15.12 Mandatory agent rule](#1512-mandatory-agent-rule)
* [Short mandatory summary](#short-mandatory-summary)

---
## 1. Language and coding style

### 1.1 Language

* Use **SystemVerilog 2012**.

### 1.2 Modern RTL only

Use modern synthesizable constructs only.

Allowed procedural styles:

* `always_ff`
* `always_comb`

Also allowed:

* continuous `assign`

Not allowed:

* plain `always`
* `always @(*)`
* old mixed legacy procedural style unless explicitly requested

So the rule is **not** “everything must be in `always_comb`”.
The real rule is:

* use `always_ff` for sequential logic
* use `always_comb` for combinational procedural logic
* use `assign` freely where it is cleaner for simple combinational connections

---

## 2. Nettype rule

### 2.1 Top of file

At the top of RTL files, place:

```systemverilog
`default_nettype none
```

### 2.2 End of file

Do **not** put this at the end:

```systemverilog
`default_nettype wire
```

This project style intentionally does **not** restore `default_nettype wire` at the end.

---

## 3. Reset policy

### 3.1 General rule

When reset is needed inside `always_ff`, use **synchronous reset**.

Preferred style:

```systemverilog
always_ff @(posedge clk) begin
  if (!rst_n) begin
    ...
  end else begin
    ...
  end
end
```

### 3.2 Asynchronous reset

Do **not** use asynchronous reset unless:

* the user explicitly asks for it, or
* a specific design discussion concluded that async reset is required

### 3.3 No extra reset inventions

Do not add extra reset mechanisms, reset synchronizers, or reset helper logic unless explicitly required by the design.

---

## 4. FSM policy

### 4.1 Required FSM style

All FSMs must be coded as **single-process synchronous FSMs**.

That means:

* one `always_ff` process
* state update happens there
* state-driven outputs/registers also update there as needed

### 4.2 Forbidden FSM style

Do **not** use the classic two-process FSM style:

* one `always_comb` for next-state logic
* one `always_ff` for state register

That style is not wanted here.

### 4.3 Default state handling

FSMs must include a safe default case.

Preferred pattern:

```systemverilog
default: begin
  s_current <= S_IDLE;
end
```

### 4.4 Output behavior

Prefer FSM-controlled registered outputs and registered control where natural to the design.

---

## 5. Combinational logic policy

### 5.1 Allowed forms

Combinational logic may be written using either:

* `always_comb`
* `assign`

Both are valid.

Use `assign` when the logic is simple and direct.
Use `always_comb` when there are multiple branches, defaults, muxing, or more structured combinational behavior.

### 5.2 Forbidden form

Do **not** use:

```systemverilog
always @(*)
```

### 5.3 `always_comb` style

When using `always_comb`:

* assign defaults first
* make all paths explicit
* avoid latch inference

Example:

```systemverilog
always_comb begin
  out_valid = 1'b0;
  out_data  = '0;

  if (sel_a) begin
    out_valid = 1'b1;
    out_data  = data_a;
  end else if (sel_b) begin
    out_valid = 1'b1;
    out_data  = data_b;
  end
end
```

---

## 6. Synthesizable-only RTL policy

### 6.1 General rule

RTL files should stay synthesizable and clean.

Do not add:

* testbench constructs
* delays
* `initial` blocks
* simulation helpers unrelated to requested debug support
* assertions unless explicitly requested

### 6.2 Top-of-file simulation-related items

Do not place sim-only primitives or wrappers at the top of the file other than:

* `` `default_nettype none ``
* the standardized debug macro block, when debug support is requested

---

## 7. Standard RTL debug capability

When asked to add RTL debug capabilities, use the following exact style.

## 7.1 Debug macro header snippet

Place this near the top of the file before the module declaration:

```systemverilog
// ----------------------------------------------------------------------------
// Debug print macro:
// - In simulation: expands to `DPRINT(...)
// - In synthesis: expands to nothing (arguments removed by preprocessor)
// ----------------------------------------------------------------------------
`ifndef DPRINT
`ifndef SYNTHESIS
`define DPRINT(stmt) stmt
`else
`define DPRINT(stmt)
`endif
`endif
```

Use this exact macro name:

* `DPRINT`

Do not rename it unless explicitly told to.

---

## 7.2 Standard debug parameters

When debug support is requested, add this parameter to the module parameter list:

```systemverilog
    // ================================================================
    // DEBUG CONTROL
    // ================================================================
    parameter bit RTL_DBG = 1'b1
```

This parameter must appear clearly in the module parameter list.
It is the standardized RTL debug control parameter and gates simulation-only debug prints.

---

## 7.3 Debug time policy

RTL debug prints should use raw SystemVerilog simulator time through `$time` and print it as a bare numeric timestamp inside the third bracket.

Intent:

* RTL debug is only local simulation breadcrumbs.
* RapidVPI/VIP/runner/scoreboard code owns precise event timing, testcase timing, and formatted runtime summaries.
* RTL prints should stay simple and should not duplicate the RapidVPI time-formatting policy.

---

## 7.4 Standard debug print insertion style

When inserting debug prints, use this exact general pattern:

```systemverilog
`ifndef SYNTHESIS
                  if (RTL_DBG) begin
                    `DPRINT($display(
                      "[RTL][WARN][%0t] gmii_rx_rs: ASSUME_STRIP saw non-preamble first byte 0x%02x, treating as DA",
                      $time,
                      gmii_rxd
                    ));
                  end
`endif
```

Rules:

* wrap debug prints with `` `ifndef SYNTHESIS ``
* gate them with `if (RTL_DBG)`
* invoke them using `` `DPRINT(...) ``
* print simulator time as `[%0t]` using `$time`

### 7.5 Message style

Debug messages should be:

* short
* specific
* contextual
* tagged with severity when useful

Preferred general format:

```text
[RTL][INFO][%0t] ...
[RTL][WARN][%0t] ...
[RTL][ERR ][%0t] ...
```

### 7.6 Where to add debug prints

Place debug prints only at meaningful events, such as:

* unusual protocol conditions
* state transitions
* packet/frame boundaries
* accepted transfers
* overflow/underflow/drop conditions
* assumption violations
* mode/config announcements

Do not spam every cycle unless explicitly requested.

---

## 8. Parameters and widths

### 8.1 Typed parameters

Use typed parameters.

Preferred style:

```systemverilog
parameter int unsigned DATA_W = 32;
parameter bit          EN_SOMETHING = 1'b1;
```

### 8.2 Width cleanliness

Be careful with widths.

* keep widths explicit
* use localparams for derived widths
* avoid ugly casts and slice hacks unless truly necessary

### 8.3 Derived width style

Prefer simple localparams:

```systemverilog
localparam int unsigned KEEP_W = DATA_W / 8;
```

### 8.4 Counters and indices

Use clean sizing for counters and pointers.
Do not use ugly width hacks if a clean localparam or typed local signal solves it.

---

## 9. Signal and module organization

### 9.1 Use `logic`

Prefer `logic` for RTL signals unless a specific net type is genuinely needed.

### 9.2 File organization

Keep the file organized into clear sections, for example:

* localparams / typedefs
* internal signals
* optional debug prints
* combinational logic
* sequential logic

### 9.3 Readability

Generated RTL should be readable to an engineer without needing to reverse-engineer intent.

---

## 10. Preferred module template

A good general template is:

```systemverilog
`default_nettype none

// ----------------------------------------------------------------------------
// Debug print macro:
// - In simulation: expands to `DPRINT(...)
// - In synthesis: expands to nothing (arguments removed by preprocessor)
// ----------------------------------------------------------------------------
`ifndef DPRINT
`ifndef SYNTHESIS
`define DPRINT(stmt) stmt
`else
`define DPRINT(stmt)
`endif
`endif

module some_module #(
  parameter int unsigned SOME_W = 8,
    // ================================================================
    // DEBUG CONTROL
    // ================================================================
    parameter bit RTL_DBG = 1'b1
) (
  input  logic clk,
  input  logic rst_n,
  ...
);

  // ================================================================
  // LOCALPARAMS / TYPEDEFS
  // ================================================================

  // ================================================================
  // INTERNAL SIGNALS
  // ================================================================

  // ================================================================
  // OPTIONAL DEBUG PRINTS
  // ================================================================

  // ================================================================
  // COMBINATIONAL LOGIC
  // ================================================================

  assign some_wire = some_expr;

  always_comb begin
    ...
  end

  // ================================================================
  // SEQUENTIAL LOGIC
  // ================================================================
  always_ff @(posedge clk) begin
    if (!rst_n) begin
      ...
    end else begin
      ...
    end
  end

endmodule
```

---

## 11. Rules for AI/code-generation agents

Any AI agent generating RTL in this style must follow these rules:

1. Use **SystemVerilog 2012**.
2. Use only:

   * `always_ff`
   * `always_comb`
   * `assign`
3. Do **not** use plain `always`.
4. Do **not** use `always @(*)`.
5. Code FSMs as **single-process synchronous FSMs** only.
6. Use **synchronous resets** unless explicitly told otherwise.
7. Put `` `default_nettype none `` at the top of the file.
8. Do **not** put `` `default_nettype wire `` at the end of the file.
9. Keep RTL synthesizable unless explicitly asked for non-synthesizable additions.
10. When debug capability is requested, use the exact standardized `DPRINT` macro header, the single `RTL_DBG` debug parameter, and the standard guarded tick-only debug print style shown above.

---

## 12. Input signals
Do not use `input logic xxx`, use `input wire xxx` for IO inputs.

## 13. Do not try to run sims until I specifically tell you
Do not try to run iverilog or any other sim or construct manually testbench to run any sims until I specifically give you target cmake command to run within repo. When you edit files you just edit them and I run and verify myself.

## 14. FSM Encoding style
For FSMs, do **not** scatter raw encodings like `2'b10` or `3'd4` directly inside the case logic. Declare the state type up top in the **LOCALPARAMS / TYPEDEFS** section using `typedef enum logic [...]`, then use symbolic names like `S_IDLE`, `S_WAIT`, `S_DONE` in the one-piece `always_ff` FSM; that makes the code much easier to read, safer to modify, and avoids “what was 2'b10 again?” reverse-engineering later. This fits the guide’s emphasis on typed constructs, local typedef organization, readability, and single-process FSM style. 

Typical style:

```systemverilog
typedef enum logic [1:0] {
  S_IDLE,
  S_WAIT,
  S_RUN,
  S_DONE
} state_t;

state_t s_current;
```

Then the FSM should use only `S_IDLE`, `S_WAIT`, etc., never raw numeric literals for state comparisons or assignments.



## 15. Standard simulation DUT wrapper architecture

### 15.1 Purpose and scope

Every RTL project that uses simulator-native controllable clocks for VIP/RapidVPI verification must use the same simulation-top architecture.

The required file name is always:

```text
dut_wrapper.sv
```

The required simulation-top module name is always:

```systemverilog
dut_wrapper
```

Do not invent project-specific wrapper names.

`dut_wrapper.sv` is a **simulation-only integration file**. Its purpose is to:

* keep the real RTL top synthesizable and unchanged
* instantiate the real synthesizable RTL top as `u_dut`
* generate VIP-owned/external DUT clocks natively inside the HDL simulator
* expose a small, predictable set of VPI-visible clock control/status signals
* preserve the normal DUT signal names under the `dut_wrapper` hierarchy
* provide the standard Verilator timing-queue keepalive

This architecture applies equally to a new IP and to an older IP being retrofitted.

The wrapper must remain thin. Do not move functional RTL, protocol behavior, scoreboarding, stimulus generation, or design policy into it.

### 15.2 Simulation-only exception to the normal RTL rules

Sections above require synthesizable RTL and prohibit plain `always` blocks and `#` delays. Those rules remain mandatory for the real RTL design.

`dut_wrapper.sv` is the deliberate exception because it is simulation-only infrastructure.

The standard native clock generator and Verilator keepalive in `dut_wrapper.sv` may therefore use:

* plain `always`
* `wait`
* `#` timing delays
* initialized simulation variables/signals

Do not copy these simulation-only constructs into synthesizable RTL files.

### 15.3 Required file structure and style

Construct `dut_wrapper.sv` in the same style for every project:

```text
`default_nettype none

large top comment:
  purpose
  USER CONTROLLABLE CLOCKS
  exact clock/control mappings
  control semantics
  timeprecision/tick meaning
  start_at() first-rise behavior

sim_native_clock_gen
  generic simulation-only clock generator

dut_wrapper
  parameters mirroring the real RTL top as required
  normal external DUT ports
  internal native-clock control/status groups
  internal generated DUT clock nets
  one sim_native_clock_gen instance per controllable clock
  Verilator keepalive
  synthesizable DUT instance: u_dut
```

Use clear section comments in the wrapper, matching the established style.

Both `sim_native_clock_gen` and `dut_wrapper` use:

```systemverilog
timeunit 1ns; timeprecision 1ps;
```

Therefore one `*_period_ticks` tick is 1 ps.

### 15.4 Decide which clocks the wrapper owns

Before writing the wrapper, inventory every clock associated with the real RTL top and classify it.

A clock is **wrapper/VIP owned** when it is an external clock input to the DUT whose timing must be controllable by verification, including normal start/stop, period changes, or phase/`start_at()` stress.

Examples include:

* system/interface input clocks
* externally supplied PHY receive clocks
* independent source/sink clocks used for CDC testing
* any other external DUT clock that a testcase may intentionally manipulate

For every such clock, the wrapper generates the clock natively.

A clock is **DUT owned** when the DUT itself generates it as an output. DUT-owned clocks remain ordinary DUT outputs and are observed only. Do not create native control signals for them and do not take ownership of them in the wrapper.

If a project has `N` independently controllable external clocks, create `N` independent native-clock control groups and `N` `sim_native_clock_gen` instances.

Do not combine unrelated clocks into one control group.

### 15.5 Mandatory clock-control naming contract

For an externally controlled DUT clock named:

```text
<clock>
```

create these internal signals inside `dut_wrapper`:

```systemverilog
logic        sim_<clock>_enable       /* verilator public_flat_rw */ = 1'b0;
logic [63:0] sim_<clock>_period_ticks /* verilator public_flat_rw */ = <sane_default_period>;
logic        sim_<clock>_stopped      /* verilator public_flat_rd */;
```

and create the generated clock net itself using the original DUT clock-port name:

```systemverilog
logic <clock> /* verilator public_flat_rd */;
```

Example for a clock named `rx_clk`:

```systemverilog
logic        sim_rx_clk_enable       /* verilator public_flat_rw */ = 1'b0;
logic [63:0] sim_rx_clk_period_ticks /* verilator public_flat_rw */ = 64'd8000;
logic        sim_rx_clk_stopped      /* verilator public_flat_rd */;

logic rx_clk /* verilator public_flat_rd */;
```

The control semantics are always:

```text
sim_<clock>_enable
  1 = run
  0 = stop and park low

sim_<clock>_period_ticks
  full clock period in 1 ps ticks

sim_<clock>_stopped
  1 only when the clock is fully stopped and parked low
```

Initial `*_enable` must be `0`. A testcase/VIP explicitly starts clocks; the wrapper must not silently start them on its own.

Choose a sane project-specific default `*_period_ticks` matching the nominal clock period, but verification must still be able to overwrite it.

### 15.6 Standard `sim_native_clock_gen` behavior

Keep one generic `sim_native_clock_gen` definition in `dut_wrapper.sv` and instantiate it once per controllable clock.

Its externally visible behavior must match the established implementation:

* clock starts parked low
* stopped status starts asserted
* periods below 2 ticks are clamped to 2 ticks
* full period is split as:

```text
high_ticks = period_ticks / 2
low_ticks  = period_ticks - high_ticks
```

* odd periods therefore remain deterministic
* stopping takes effect at a half-cycle boundary
* the clock is parked low before `stopped_o` is asserted
* period changes are honored without replacing the clock generator
* when `enable_i` changes from 0 to 1, the first rising edge is produced in that same simulation time slot

That final rule is required for deterministic VIP `start_at()` behavior. The VIP side programs the period first and schedules the `*_enable` 0->1 write at the requested first-rise tick.

Do not redesign these semantics independently for each IP.

Instantiate clocks in the standard form:

```systemverilog
sim_native_clock_gen u_sim_<clock> (
    .enable_i      (sim_<clock>_enable),
    .period_ticks_i(sim_<clock>_period_ticks),
    .clk_o         (<clock>),
    .stopped_o     (sim_<clock>_stopped)
);
```

### 15.7 Wrapper ports, parameters, and DUT instance

The wrapper is the simulation shell around the normal synthesizable top.

Rules:

* instantiate the real RTL top as `u_dut`
* preserve the real top's functional parameters and pass them through explicitly
* expose normal non-clock DUT interfaces through `dut_wrapper` with the same names, widths, and directions wherever practical
* use this guide's normal I/O style: `input wire` for input ports
* connect normal wrapper ports directly to `u_dut`
* do not insert protocol behavior between wrapper ports and the DUT
* externally controlled DUT clock inputs become internal generated nets and therefore are not ordinary wrapper input ports
* DUT-generated clock outputs remain ordinary wrapper outputs
* preserve compile-time interface-selection macros/conditions when the real top has mutually exclusive interfaces

The resulting hierarchy should conceptually remain:

```text
dut_wrapper
├── native simulation clock generators
├── Verilator keepalive
└── <real synthesizable RTL top> u_dut
```

### 15.8 Mandatory top-of-file clock documentation

At the top of every `dut_wrapper.sv`, include a concise `USER CONTROLLABLE CLOCKS` comment block.

It must list every wrapper-owned clock and its exact three control/status signals.

For example:

```text
// -----------------------------------------------------------------------------
// USER CONTROLLABLE CLOCKS
// -----------------------------------------------------------------------------
//   System clock -> clk
//     sim_clk_enable
//     sim_clk_period_ticks
//     sim_clk_stopped
//
//   Receive clock -> rx_clk
//     sim_rx_clk_enable
//     sim_rx_clk_period_ticks
//     sim_rx_clk_stopped
//
// Control semantics:
//   *_enable        : 1 = run clock, 0 = stop and park clock low
//   *_period_ticks  : full clock period; one tick = 1ps in this wrapper
//   *_stopped       : 1 = clock is stopped and parked low
```

For an IP with more clocks, extend this list mechanically. Do not omit clocks from the documentation and do not document control groups that do not exist.

Also state the deterministic `start_at()` rule: period is programmed first and the enable write at the requested first-rise tick produces the first rising edge in that same simulation time slot.

### 15.9 Standard Verilator keepalive

Include the standard simulation-only keepalive inside `dut_wrapper`:

```systemverilog
logic sim_keepalive /* verilator public_flat_rd */ = 1'b0;

always begin : p_sim_keepalive
  #1s;
  sim_keepalive = ~sim_keepalive;
end
```

This signal is unrelated to the DUT and is not a testcase control.

Its purpose is only to keep a far-future native HDL timed event in the queue so a stock Verilator `--binary --vpi --timing` simulation does not terminate merely because all controllable DUT clocks are intentionally stopped.

Do not connect `sim_keepalive` to `u_dut`.

### 15.10 Build and hierarchy rules

`dut_wrapper.sv` is the simulation top.

For a project using this architecture:

* compile `dut_wrapper.sv` as a simulation source
* elaborate/run `dut_wrapper`, not the bare synthesizable RTL top
* do not place `dut_wrapper.sv` in synthesis `RTL_SOURCES`
* do not synthesize `sim_native_clock_gen`
* keep the real RTL top usable independently for synthesis
* keep the generated clock names equal to the real DUT clock-port names so top-level VPI paths remain simple and predictable under `dut_wrapper`

The wrapper may contain simulator-specific visibility annotations such as:

```systemverilog
/* verilator public_flat_rw */
/* verilator public_flat_rd */
```

where required for the native-clock/VPI contract.

### 15.11 Retrofitting an older project

When this guide is introduced into an older project that does not yet use the standard wrapper, retrofit it systematically.

1. Identify the real synthesizable RTL top.
2. Inventory every clock and classify it as:
   * external/VIP owned, or
   * DUT generated/observe-only.
3. Create exactly:
   * `dut_wrapper.sv`
   * `module dut_wrapper`
4. Copy/pass through the real top's required parameters.
5. Mirror the normal external DUT I/O on the wrapper.
6. Replace every external/VIP-owned DUT clock input with:
   * the internal generated clock net using the original clock name
   * `sim_<clock>_enable`
   * `sim_<clock>_period_ticks`
   * `sim_<clock>_stopped`
   * one `sim_native_clock_gen` instance.
7. Leave DUT-generated clocks as normal outputs.
8. Add the standard top-of-file clock mapping/semantics comments.
9. Add the standard Verilator keepalive.
10. Instantiate the real synthesizable top as `u_dut`.
11. Add `dut_wrapper.sv` to simulation sources only.
12. Change the simulator top from the bare RTL top to `dut_wrapper`.
13. Do not otherwise redesign, refactor, rename, optimize, or alter the RTL top while performing this wrapper retrofit.

If conditional build modes expose different external clocks, make the wrapper use the same compile-time selection and provide the correct native-clock group for the active mode.

### 15.12 Mandatory agent rule

When an agent creates a new RTL project or encounters an existing project that uses this native-clock verification architecture, it must treat `dut_wrapper.sv` as standard infrastructure, not as project-specific experimentation.

The IP-specific `design_guide.md` should tell the agent **which clocks exist, which are external/VIP-owned, which are DUT-owned, and any interface-selection details**.

This universal `rtl_design_guide.md` tells the agent **how to construct the wrapper**.

Do not invent a different clock-control mechanism merely because the next IP has a different number or type of clocks.

---

## Short mandatory summary

This project RTL style is:

* SystemVerilog 2012
* synthesizable RTL
* modern style only
* `always_ff` for sequential logic
* `always_comb` or `assign` for combinational logic
* never plain `always`
* never `always @(*)`
* synchronous one-process FSMs only
* synchronous resets unless explicitly told otherwise
* `` `default_nettype none `` at file top
* no `` `default_nettype wire `` at file end
* standardized optional debug infrastructure using `DPRINT` with bare `$time` `[RTL]` prints

