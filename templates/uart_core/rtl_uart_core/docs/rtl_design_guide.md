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

# General RTL Design Guide

This guide defines the required coding style for synthesizable RTL in this project. It is intended to be strict enough that a smaller AI agent can generate code that matches the expected style without guessing.

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

