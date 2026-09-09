<!--
MIT License

Copyright (c) 2024 Rovshan Rustamov

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
-->

# vip_uart RX Agent

## Table of contents

- [1. Purpose](#1-purpose)
- [2. Port map](#2-port-map)
- [3. CTS control](#3-cts-control)
- [4. Public API](#4-public-api)

## 1. Purpose

`vip::uart::UartRx` samples UART serial frames from a configured net. In a
`uart_core` TX-path test, this net is usually the DUT `uart_tx_o` pin.

The agent decodes data bits, optional parity, stop bits, framing errors,
parity errors, and simple break conditions. Captured frames can be sent to
`ScbUartStream` and `ScbUartRules`. Captured frame history stores raw
simulator timestamps in `start_tick` and `end_tick`.

## 2. Port map

Use `UartRxPortConfig` for explicit net names:

```cpp
vip::uart::UartRxPortConfig p;
p.name = "uart0";
p.rx_net = "uart_tx_o";
p.cts_net = "uart_cts_i";
p.drive_cts = true;
```

## 3. CTS control

When CTS driving is enabled, the agent owns the configured CTS net. Use
`set_cts_active()` for state updates or `drive_cts_now()` when the testcase
needs an awaited immediate write. The physical active level comes from the port
configuration.

## 4. Public API

- `set_capture_enable(port, enable)`
- `get_history(port)`
- `history_size(port)`
- `observed_count(port)`
- `started_count(port)` and `last_start_tick(port)` expose character-launch
  evidence before the active character completes.
- `wait_for_frames(port, count)`
- `wait_for_started_count(port, count, timeout_cycles, reached)` provides a
  bounded character-launch wait.
- `wait_for_observed_count(port, count, timeout_cycles, reached)` provides a
  bounded character-count wait for timestamped history milestones.
- `clear_history(port)`
- `set_cts_drive_enable(port, enable)`
- `set_cts_active_low(port, active_low)`
- `set_cts_active(port, active)`
- `drive_cts_now(port, active)`
- `arm_cts_inactive_after_observed_count(port, count)` schedules a one-shot
  logical CTS deassertion after the exact future captured-character count;
  `scheduled_cts_pending()`, `scheduled_cts_fired()`, and
  `last_cts_transition_tick()` expose bounded cut-point evidence.
