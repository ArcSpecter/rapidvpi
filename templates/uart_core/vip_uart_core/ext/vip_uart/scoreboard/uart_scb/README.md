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

# vip_uart Scoreboards

## Table of contents

- [1. Stream scoreboard](#1-stream-scoreboard)
- [2. Rules scoreboard](#2-rules-scoreboard)
- [3. Lifecycle](#3-lifecycle)

## 1. Stream scoreboard

`vip::uart::ScbUartStream` compares expected UART frames with observed frames
per logical port.

Observed frame event timestamps use raw simulator ticks from `UartFrame::end_tick`.

Useful APIs:

- `expect_byte(port, data)`
- `expect_bytes(port, data)`
- `expect_frame(port, frame)`
- `observe_frame(port, frame)`
- `set_unexpected_policy(policy)`
- `unexpected_policy()`
- `set_fail_on_unexpected(enable)`
- `set_strict_status_compare(enable)`

### Unexpected-byte ownership

`UartUnexpectedPolicy` has three policies:

- `Fail` is the default and reports an observation with an empty expected queue
  as a scoreboard failure.
- `Warn` reports that observation as a warning. The compatibility calls
  `set_fail_on_unexpected(true)` and `set_fail_on_unexpected(false)` map to
  `Fail` and `Warn`, respectively.
- `ExternalOwner` creates no scoreboard event for an observation with an empty
  expected queue because a named higher-level checker owns a bounded interval.

`ExternalOwner` is not a generic ignore or warning-suppression mode. Observed
counters still increment, and a nonempty expected queue is still popped and
compared exactly, including UART status. The external owner must compare the
complete physical UART history slice, so bytes skipped by a frame collector's
SOF search cannot escape checking. Restore the exact prior policy when the
bounded interval ends.

For example, a testcase can use a non-copyable RAII guard that saves
`stream.unexpected_policy()`, selects `ExternalOwner`, collects the bounded
frames, and compares the raw history slice against `optional_stale + ping`.
The guard destructor restores the saved policy before strict traffic resumes:

```cpp
{
    UnexpectedPolicyGuard guard(stream, UartUnexpectedPolicy::ExternalOwner);
    // Collect the bounded higher-level response interval.
    tc_expect_bytes(test, observed_raw_slice, expected_stale_plus_ping, label);
}
stream.expect_bytes(port, exact_next_response);
```

`reset_case()` clears expected queues and observed counters only. It does not
change the unexpected-byte policy, so runners should establish their per-case
default explicitly and scoped owners must restore their saved policy.

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
