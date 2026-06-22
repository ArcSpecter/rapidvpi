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

# vip_uart

Reusable RapidVPI/C++ coroutine VIP for UART serial links.

## Table of contents

- [1. Scope](#1-scope)
- [2. Provided components](#2-provided-components)
- [3. Parameters](#3-parameters)
- [4. Port mapping](#4-port-mapping)
- [5. Flow control](#5-flow-control)
- [6. Example wiring](#6-example-wiring)
- [7. Limitations](#7-limitations)

## 1. Scope

`vip_uart` provides protocol-generic UART agents and scoreboards. It does not
contain DUT register maps, wrapper command formats, or project-specific byte
interfaces.

Version one supports:

- 5, 6, 7, or 8 data bits
- one or two stop bits
- none, even, or odd parity
- configurable bit timing in parent testbench clock edges
- optional TX start-edge phase offsets with time-based bit timing
- TX serial driving
- RX serial sampling and decode
- bad parity and bad stop-bit injection
- RTS observation before TX frame launch
- CTS driving from the RX side
- stream and rules scoreboards

## 2. Provided components

```text
common/
  uart_params.hpp
  uart_types.hpp
agents/
  uart_tx/tx.hpp
  uart_rx/rx.hpp
scoreboard/
  uart_scb/scb_uart_stream.hpp
  uart_scb/scb_uart_rules.hpp
```

## 3. Parameters

`vip::uart::UartParams` centralizes UART protocol shape:

```cpp
vip::uart::UartParams p;
p.data_bits = 8;
p.stop_bits = 1;
p.parity = vip::uart::UartParity::NONE;
p.bit_clks = 16;
p.sample_clk_index = 8;
p.idle_high = true;
p.cts_active_low = true;
p.rts_active_low = true;
```

`bit_clks` is measured in parent testbench clock rising edges.

For deterministic asynchronous-start testing, `UartTx` also provides
`enqueue_byte_with_phase()` and `enqueue_bytes_with_phase()`. These APIs align
the first start bit to a known clock edge plus a picosecond phase offset and
then drive the frame using baud-derived time delays instead of clock-edge
counts.

`UartTx`, `UartRx`, and `ScbUartStream` also provide `set_params(...)` for
testcases that need to change UART framing or timing while the line is idle.

## 4. Port mapping

Use explicit port configs so the reusable VIP is not tied to one DUT's signal
names.

For a DUT RX-path test:

```cpp
vip::uart::UartTxPortConfig tx;
tx.name = "uart0";
tx.tx_net = "uart_rx_i";
tx.rts_net = "uart_rts_o";
tx.use_rts = true;
```

For a DUT TX-path test:

```cpp
vip::uart::UartRxPortConfig rx;
rx.name = "uart0";
rx.rx_net = "uart_tx_o";
rx.cts_net = "uart_cts_i";
rx.drive_cts = true;
```

## 5. Flow control

RTS is handled by `UartTx`. When enabled, TX samples RTS before each new UART
frame and waits while RTS is inactive. It does not interrupt a frame already in
progress.

CTS is handled by `UartRx`. When enabled, RX owns the CTS net and can drive it
active or inactive through:

```cpp
rx.set_cts_active("uart0", true);
co_await rx.drive_cts_now("uart0", false);
```

## 6. Example wiring

```cpp
vip::common::Scoreboard scb(*this);
vip::uart::UartParams uart_params;
vip::uart::ScbUartStream scb_uart(scb, uart_params);
vip::uart::ScbUartRules scb_uart_rules(scb);

vip::uart::UartTx uart_peer_tx(*this, clk, rst_n, tx_cfg, uart_params);
vip::uart::UartRx uart_peer_rx(*this, clk, rst_n, rx_cfg, uart_params);

uart_peer_tx.attach_scoreboards(&scb_uart, &scb_uart_rules);
uart_peer_rx.attach_scoreboards(&scb_uart, &scb_uart_rules);

registerTest("uart_peer_tx", [this]() { return uart_peer_tx.agent(0).handle; });
registerTest("uart_peer_rx", [this]() { return uart_peer_rx.agent(0).handle; });
```

## 7. Limitations

The default TX/RX path uses testbench clock edges for UART timing. The
phase-offset TX API uses ideal baud-derived time delays for deterministic
sub-clock start-edge placement. The VIP does not model baud-rate drift, jitter,
analog line effects, nine-bit UART modes, or automatic RTS threshold behavior.
Those are intended as additive hooks if later testcases require them.
