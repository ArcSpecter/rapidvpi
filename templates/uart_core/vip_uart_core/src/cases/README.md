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

# vip_uart_core testcases

## Table of contents

- [1. Purpose](#1-purpose)
- [2. Files](#2-files)
- [3. tc_basic](#3-tc_basic)
- [4. tc_cfg](#4-tc_cfg)
- [5. tc_fifo](#5-tc_fifo)
- [6. tc_error](#6-tc_error)
- [7. tc_flow_ctrl](#7-tc_flow_ctrl)
- [8. tc_reset](#8-tc_reset)
- [9. tc_phase](#9-tc_phase)
- [10. tc_stress_no_cts](#10-tc_stress_no_cts)
- [11. Helper split](#11-helper-split)
- [12. Adding more cases](#12-adding-more-cases)

## 1. Purpose

This folder contains project-owned testcase code for `vip_uart_core`.

Reusable UART serial mechanics stay in `ext/vip_uart`. DUT-specific
configuration, byte-stream handshakes, status sampling, and UART-core semantic
checks stay under `src/agents` and `src/scoreboard`.

## 2. Files

```text
tc_basic.hpp  inline testcase registration
tc_basic.cpp  basic UART RX and TX byte-path smoke sequence
tc_cfg.hpp    inline testcase registration
tc_cfg.cpp    live UART-core configuration behavior sequence
tc_fifo.hpp   inline testcase registration
tc_fifo.cpp   RX/TX FIFO status, clear, ordering, and independence sequence
tc_error.hpp  inline testcase registration
tc_error.cpp  RX error metadata, event pulse, break, and overrun sequence
tc_flow_ctrl.hpp  inline testcase registration
tc_flow_ctrl.cpp  RTS/CTS flow-control status, threshold, and launch-gating sequence
tc_reset.hpp  inline testcase registration
tc_reset.cpp  reset behavior across idle, FIFO-filled, and active RX/TX states
tc_phase.hpp  inline testcase registration
tc_phase.cpp  RX start-edge phase-offset sweep and recovery sequence
tc_stress_no_cts.hpp  inline testcase registration
tc_stress_no_cts.cpp  compact deterministic no-CTS mixed-stress sequence
tc_utils.hpp  shared testcase helper declarations
tc_utils.cpp  reset and basic configuration helpers
```

## 3. tc_basic

`tc_basic` configures the core for 8N1 at 921600 baud using `cfg_baud_inc` and
checks both directions:

- serial peer sends `0x55` and `0xa3` into `uart_rx_i`
- testcase pops matching `rx_byte_*` records with no per-byte errors
- testcase pushes `0x3c` and `0xc5` through `tx_byte_*`
- serial peer observes matching UART frames on `uart_tx_o`
- idle FIFO/activity status is sampled after reset and after TX drain

The case uses `ScbUartCore` for DUT-specific byte/serial comparisons and
`ScbUartStream`/`ScbUartRules` for reusable UART protocol checks.

## 4. tc_cfg

`tc_cfg` verifies the live configuration inputs of `uart_core`.

It checks:

- global `cfg_enable` gating
- independent `cfg_rx_enable` and `cfg_tx_enable` behavior
- fractional/NCO `cfg_baud_inc` timing changes
- 5/6/7/8 data-bit modes
- parity none/even/odd behavior, including parity-error reporting
- 1-stop and 2-stop behavior, including frame-error reporting
- benign `cfg_hw_flow_enable` behavior when RTS/CTS is not synthesized

Detailed RTS/CTS threshold and CTS-blocking behavior is intentionally left for
a future `tc_flow_ctrl`.

## 5. tc_fifo

`tc_fifo` verifies RX/TX FIFO level, empty/full, ready/valid, FIFO clear
behavior, ordering, and parallel RX/TX FIFO operation.

It covers reset FIFO status, RX fill/pop/clear, TX fill/drain/clear, clearing
TX while a character is active, parallel RX fill plus TX drain, and cross-clear
independence. It intentionally avoids parity/frame/break error stress and
RTS/CTS flow-control checks.

## 6. tc_error

`tc_error` verifies RX error reporting in `uart_core`.

It checks:

- parity-error metadata and `event_rx_parity_error`
- frame-error metadata and `event_rx_frame_error`
- break-detect metadata and `event_rx_break_detect`
- RX overrun behavior through `event_rx_overrun`
- preservation of RX error metadata ordering through the RX FIFO

It intentionally does not test normal TX completion, FIFO clear behavior, or
RTS/CTS flow control.

## 7. tc_flow_ctrl

`tc_flow_ctrl` verifies RTS/CTS hardware flow-control behavior using a DUT
build with `HAS_RTS_CTS=1`.

It checks:

- CTS polarity normalization through `cts_active`
- CTS blocking of new TX character launch
- CTS not interrupting an active TX character
- `cts_blocked` asserting only when a queued TX byte is blocked by CTS
- `cfg_hw_flow_enable=0` bypassing CTS behavior
- RTS generation from RX FIFO level using deassert/assert thresholds
- RTS physical polarity through `uart_rts_o`
- RX/TX independence while flow control is active

Detailed parity/frame/break error behavior is covered by `tc_error`; FIFO
clear/full behavior is covered by `tc_fifo`.

## 8. tc_reset

`tc_reset` verifies reset behavior of `uart_core`.

It checks reset while idle, with non-empty RX/TX FIFOs, while TX serialization
is active, while RX reception is active, and during RX low/start-like activity.
The case verifies that reset clears FIFO and busy state, returns `uart_tx_o` to
idle high, prevents stale RX/TX records after reset, keeps event outputs quiet
after reset, and confirms normal RX/TX operation after reconfiguration.

## 9. tc_phase

`tc_phase` verifies RX start-bit detection and sampling when `uart_rx_i` frames
begin at different offsets relative to `clk`.

It checks representative start-edge phase offsets across one 100 MHz clock
period, multiple byte patterns, back-to-back frames, changing phase offsets,
and recovery after near-edge offsets. The case is RX-focused and intentionally
does not test malformed frames, FIFO full behavior, RTS/CTS, or reset behavior.

## 10. tc_stress_no_cts

`tc_stress_no_cts` is a compact deterministic mixed-stress testcase for
`uart_core` with RTS/CTS disabled.

It checks simultaneous RX/TX activity, RX ready backpressure, TX valid gaps, TX
enable pause/resume, a few legal UART format changes while idle, and moderate
RX/TX FIFO pressure. It intentionally avoids long random runs, malformed
frames, reset stress, and RTS/CTS behavior so UART simulation runtime stays
reasonable.

## 11. Helper split

`tc_utils` owns only project-local testcase helpers:

- `tc_local_reset`
- `tc_apply_basic_config`
- `tc_apply_uart_config`
- UART baud/config parameter builders for testcase use

Protocol mechanics stay in `ext/vip_uart`; DUT byte/config/status mechanics
stay in `src/agents/uart_core_intf`.

## 12. Adding more cases

Add each new case as `tc_<feature>.hpp/.cpp`, register it from `src/test.cpp`,
add the source to `src/cases/CMakeLists.txt`, and describe the intent here.
