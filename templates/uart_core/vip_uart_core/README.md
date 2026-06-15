# vip_uart_core

RapidVPI/C++ coroutine verification project for the `uart_core` RTL top.

`uart_core` is a bare UART PHY core. The project verifies the raw UART pins
with reusable `vip_uart`, and verifies the DUT-specific byte/config/status
interface with local agents and a local scoreboard.

## Table of contents

- [1. Project structure](#1-project-structure)
- [2. VIP usage](#2-vip-usage)
- [3. Local DUT infrastructure](#3-local-dut-infrastructure)
- [4. Testcases](#4-testcases)
- [5. Build](#5-build)
- [6. Simulation policy](#6-simulation-policy)

## 1. Project structure

```text
src/
  init.cpp
  pindefs.hpp
  test.hpp
  test.cpp

  agents/
    uart_core_intf/
      intf.hpp/.cpp
      intf_types.hpp

  scoreboard/
    scb_uart_core.hpp/.cpp

  cases/
    tc_basic.hpp/.cpp
    tc_cfg.hpp/.cpp
    tc_fifo.hpp/.cpp
    tc_error.hpp/.cpp
    tc_flow_ctrl.hpp/.cpp
    tc_reset.hpp/.cpp
    tc_phase.hpp/.cpp
    tc_stress_no_cts.hpp/.cpp
    tc_utils.hpp/.cpp

ext/
  vip_common/
  vip_uart/
```

`src/test.cpp` wires the shared scoreboard, clock/reset helpers, UART serial
agents, local byte-interface agent, local scoreboard, runner hooks, and
testcase registration.

## 2. VIP usage

This project uses:

- `vip_common`
  - runner
  - common scoreboard
  - clock
  - POR/reset
  - coroutine utilities
- `vip_uart`
  - `vip::uart::UartTx` drives the external serial peer into `uart_rx_i`
  - `vip::uart::UartRx` samples `uart_tx_o`
  - UART stream and rules scoreboards for protocol-level serial checks

## 3. Local DUT infrastructure

The right side of `uart_core` is a DUT-specific byte/config/status interface,
not a reusable bus protocol. It stays local:

- `UartCoreIntf` drives `cfg_*`, FIFO clear pulses, `tx_byte_*`, and
  `rx_byte_ready`.
- `UartCoreIntf` samples `tx_byte_ready`, `rx_byte_*`, FIFO status, activity
  status, flow-control status, and event pulses.
- `UartCoreIntf` counts one-cycle `event_*` pulses for configuration/error
  checks.
- `ScbUartCore` compares serial RX stimulus against observed `rx_byte_*`
  records and compares byte-side TX stimulus against observed UART TX frames.

## 4. Testcases

`tc_basic` is the starter smoke test:

- reset and configure the core for 8N1 at 921600 baud using `cfg_baud_inc`
- send bytes into `uart_rx_i` through `vip_uart::UartTx`
- pop the resulting `rx_byte_*` records through `UartCoreIntf`
- push bytes through `tx_byte_*`
- observe the resulting `uart_tx_o` frames through `vip_uart::UartRx`
- check both directions through the local UART-core scoreboard
- sample idle FIFO/activity status after reset and after TX drain

`tc_cfg` verifies the live configuration inputs of `uart_core`:

- global `cfg_enable` gating
- independent `cfg_rx_enable` and `cfg_tx_enable` behavior
- fractional/NCO `cfg_baud_inc` timing changes
- 5/6/7/8 data-bit modes
- parity none/even/odd behavior, including parity-error reporting
- 1-stop and 2-stop behavior, including frame-error reporting
- benign `cfg_hw_flow_enable` behavior when RTS/CTS is not synthesized

Detailed RTS/CTS threshold and CTS-blocking behavior is intentionally left for
a future `tc_flow_ctrl`.

`tc_fifo` verifies RX/TX FIFO-visible behavior:

- reset FIFO status
- RX FIFO level, empty/full, valid, ordering, and clear behavior
- TX FIFO level, empty/full, ready, ordering, idle clear, and active-character clear behavior
- independent RX/TX FIFO operation during parallel RX fill and TX drain
- cross-clear independence between RX and TX FIFOs

It intentionally avoids parity/frame/break error stress and RTS/CTS
flow-control checks.

`tc_error` verifies UART RX error reporting. It checks parity-error metadata
and events, frame-error metadata and events, break-detect metadata and events,
and RX overrun event behavior. It intentionally does not test RTS/CTS flow
control or normal TX completion behavior.

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

`tc_reset` verifies reset behavior of `uart_core`.

It checks reset while idle, with non-empty RX/TX FIFOs, while TX serialization
is active, while RX reception is active, and during RX low/start-like activity.
The case verifies that reset clears FIFO and busy state, returns `uart_tx_o` to
idle high, prevents stale RX/TX records after reset, keeps event outputs quiet
after reset, and confirms normal RX/TX operation after reconfiguration.

`tc_phase` verifies RX start-bit detection and sampling when `uart_rx_i` frames
begin at different offsets relative to `clk`.

It checks representative start-edge phase offsets across one 100 MHz clock
period, multiple byte patterns, back-to-back frames, changing phase offsets,
and recovery after near-edge offsets. The case is RX-focused and intentionally
does not test malformed frames, FIFO full behavior, RTS/CTS, or reset behavior.

`tc_stress_no_cts` is a compact deterministic mixed-stress testcase for
`uart_core` with RTS/CTS disabled.

It checks simultaneous RX/TX activity, RX ready backpressure, TX valid gaps, TX
enable pause/resume, a few legal UART format changes while idle, and moderate
RX/TX FIFO pressure. It intentionally avoids long random runs, malformed
frames, reset stress, and RTS/CTS behavior so UART simulation runtime stays
reasonable.

## 5. Build

When a configured release build directory exists, build the RapidVPI shared
library target:

```text
cmake --build cmake-build-release --target vip_uart_core -j
```

## 6. Simulation policy

RTL simulations are intentionally not run by this automation unless explicitly
requested with a specific simulation command.
