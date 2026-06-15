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
`ScbUartStream` and `ScbUartRules`.

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
- `wait_for_frames(port, count)`
- `clear_history(port)`
- `set_cts_drive_enable(port, enable)`
- `set_cts_active_low(port, active_low)`
- `set_cts_active(port, active)`
- `drive_cts_now(port, active)`
