# vip_uart TX Agent

## Table of contents

- [1. Purpose](#1-purpose)
- [2. Port map](#2-port-map)
- [3. Flow control](#3-flow-control)
- [4. Phase-offset launch](#4-phase-offset-launch)
- [5. Public API](#5-public-api)

## 1. Purpose

`vip::uart::UartTx` drives UART serial frames onto a configured net. In a
`uart_core` RX-path test, this net is usually the DUT `uart_rx_i` pin.

The agent supports 5 to 8 data bits, one or two stop bits, none/even/odd
parity, configurable bit timing in testbench clock edges, optional
phase-offset frame launch with baud-derived time delays, inter-frame gaps, bad
parity injection, and bad stop-bit framing injection.

## 2. Port map

Use `UartTxPortConfig` for explicit net names:

```cpp
vip::uart::UartTxPortConfig p;
p.name = "uart0";
p.tx_net = "uart_rx_i";
p.rts_net = "uart_rts_o";
p.use_rts = true;
```

## 3. Flow control

When `use_rts` or `set_respect_rts(port, true)` is enabled, the agent samples
the configured RTS net before launching each new frame. It does not interrupt a
frame already in progress, matching normal UART RTS behavior.

`set_rts_wait_timeout_clks()` can turn a stuck inactive RTS into a scoreboard
rule event.

## 4. Phase-offset launch

Use `enqueue_byte_with_phase()` when a testcase needs the first UART start edge
at a deterministic offset from a known testbench clock edge:

```cpp
const unsigned ticket =
    tx.enqueue_byte_with_phase("uart0", 0xa5, 921600, 2500);
co_await tx.wait_done(ticket);
```

The phase argument is in picoseconds. The agent waits for a known clock edge,
waits the requested phase offset, drives the start bit, and then uses
baud-derived time delays for the rest of the frame. Use
`enqueue_bytes_with_phase()` for a back-to-back sequence where only the first
frame start needs an explicit phase offset.

## 5. Public API

- `enqueue_byte(port, data)`
- `enqueue_bytes(port, data)`
- `enqueue_byte_with_phase(port, data, baud_rate, phase_offset_ps)`
- `enqueue_bytes_with_phase(port, data, baud_rate, initial_phase_offset_ps)`
- `wait_done(ticket)`
- `set_inter_frame_gap_clks(port, clks)`
- `set_respect_rts(port, enable)`
- `set_rts_active_low(port, active_low)`
- `set_rts_wait_timeout_clks(port, clks)`
- `set_auto_expect(port, enable)`
- `arm_next_framing_error(port)`
- `arm_next_parity_error(port)`
- `pending_count(port)`
- `get_history(port)`
