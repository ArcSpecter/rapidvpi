# vip_uart Scoreboards

## Table of contents

- [1. Stream scoreboard](#1-stream-scoreboard)
- [2. Rules scoreboard](#2-rules-scoreboard)
- [3. Lifecycle](#3-lifecycle)

## 1. Stream scoreboard

`vip::uart::ScbUartStream` compares expected UART frames with observed frames
per logical port.

Useful APIs:

- `expect_byte(port, data)`
- `expect_bytes(port, data)`
- `expect_frame(port, frame)`
- `observe_frame(port, frame)`
- `set_fail_on_unexpected(enable)`
- `set_strict_status_compare(enable)`

## 2. Rules scoreboard

`vip::uart::ScbUartRules` reports protocol-level events that are not tied to a
specific expected byte queue:

- framing errors
- parity errors
- break detection
- RTS wait timeouts
- optional verbose CTS/RTS flow-control observations

It supports `set_warn_only()` for bring-up and `set_enable()` for disabling
rule checks temporarily.

## 3. Lifecycle

Call these from the parent project runner hooks:

```cpp
scb_uart.reset_case();
scb_uart_rules.reset_case();

scb_uart.end_case_check(true);
scb_uart_rules.end_case_check(false);
```
