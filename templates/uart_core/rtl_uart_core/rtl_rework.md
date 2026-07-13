# RTL debug-time rework guide for `rtl_xxx` projects

Refactor this RTL project generically for any `rtl_xxx` repo debug-print policy.

Goal: keep RTL debug simple and consistent:

- one debug enable parameter: `RTL_DBG`
- no RTL-side ns/us/ms debug formatting
- `[RTL]` prints use raw SystemVerilog simulator time as a bare bracket field

---

## Rules

1. In every synthesizable RTL module that has debug parameters, keep only:

```systemverilog
parameter bit RTL_DBG = 1'b1
```

2. Remove RTL debug time-unit selector parameters from module parameter lists, wrappers, examples, documentation snippets, and parameter override lists.

3. Remove RTL-local debug time-format helper logic, including any helper functions, strings, localparams, or conversion math used only to convert debug print time into ns/us/ms.

4. Update all `[RTL]` debug prints to use this timestamp style:

```systemverilog
`ifndef SYNTHESIS
  if (RTL_DBG) begin
    `DPRINT($display(
      "[RTL][WARN][%0t] module_name: message ...",
      $time
    ));
  end
`endif
```

5. The timestamp field must be just the simulator time value inside its own bracket:

```text
[RTL][INFO][22000000] module_name: message ...
[RTL][WARN][22000000] module_name: message ...
[RTL][ERR ][22000000] module_name: message ...
```

6. Do not print time-unit strings from RTL debug code. No ns/us/ms suffixes and no converted debug-time values.

7. Keep the existing `DPRINT` macro style. Do not rename the macro.

8. Keep the existing debug-print guard style:

```systemverilog
`ifndef SYNTHESIS
  if (RTL_DBG) begin
    `DPRINT($display(...));
  end
`endif
```

9. Do not change functional RTL behavior, ports, FSM structure, reset behavior, datapath logic, or synthesis-visible behavior. This is only a debug-print cleanup.

10. After edits, grep should find no remaining RTL debug time-format helpers or debug time-unit selector parameter overrides. Existing documentation may mention removed legacy names only when explicitly describing migration from old code.

---

## Preferred output style

Use this:

```text
# [RTL][INFO][22000000] uart_core.u_tx_fifo: FIFO push, level_before=0 data=0x3c
# [RTL][INFO][22020000] uart_core.u_tx_fifo: FIFO push, level_before=1 data=0xc5
```

Not converted-unit RTL debug output.

---

## Codex task summary

Apply the debug-print cleanup across the whole `rtl_xxx` repo:

1. keep only `parameter bit RTL_DBG = 1'b1` for RTL debug control
2. remove RTL-side debug time-unit formatting knobs and helpers
3. make every `[RTL]` debug print use `[%0t]` with `$time`
4. keep `DPRINT`, `ifndef SYNTHESIS`, and `if (RTL_DBG)` style unchanged
5. do not touch functional RTL behavior
