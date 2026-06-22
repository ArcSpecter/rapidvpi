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

# RapidVPI reusable VIP design guide

This document defines the default architecture for creating any reusable `vip_xxx` package in the RapidVPI/C++ coroutine ecosystem.

The goal is that a new VIP can be created from this guide without copying another `vip_xxx` repository as a reference and without forcing an AI agent to inspect `/usr/local/include/rapidvpi` or RapidVPI source files. Existing VIPs such as `vip_axi`, `vip_axil`, `vip_axis`, `vip_mdio`, `vip_eth`, or `vip_uart` may still be inspected when available, but they are examples only. This guide is the policy source.

This version reflects the current RapidVPI time API after the explicit unit refactor:

```cpp
rd.getTime<ticks>();
rd.getTime<ns>();
test.getCoWrite();
test.getCoWrite<ns>(10.0);
test.getCoWrite<ticks>(10000);
```

Do not use old implicit-nanosecond forms such as `getCoWrite(0)`, `getCoRead(0)`, `getCoWrite(10.0)`, or `rd.getTime()`.

---

## Table of contents

- [1. Scope and intent](#1-scope-and-intent)
- [2. Core design principles](#2-core-design-principles)
- [3. RapidVPI public API reference](#3-rapidvpi-public-api-reference)
  - [3.1 Include model and namespace](#31-include-model-and-namespace)
  - [3.2 Time units and raw simulator ticks](#32-time-units-and-raw-simulator-ticks)
  - [3.3 `TestBase` lifecycle and net registration](#33-testbase-lifecycle-and-net-registration)
  - [3.4 `RunTask` and `RunUserTask`](#34-runtask-and-runusertask)
  - [3.5 Write awaitable: `getCoWrite`](#35-write-awaitable-getcowrite)
  - [3.6 Read awaitable: `getCoRead`](#36-read-awaitable-getcoread)
  - [3.7 Change awaitable: `getCoChange`](#37-change-awaitable-getcochange)
  - [3.8 Value conversion helpers](#38-value-conversion-helpers)
  - [3.9 Test registration and factory registration](#39-test-registration-and-factory-registration)
  - [3.10 RapidVPI API cheat sheet](#310-rapidvpi-api-cheat-sheet)
- [4. Time, timestamps, logging, and scoreboard policy](#4-time-timestamps-logging-and-scoreboard-policy)
  - [4.1 Timestamp rule: ticks are the default truth](#41-timestamp-rule-ticks-are-the-default-truth)
  - [4.2 Delay rule: explicit human units for intended time delays](#42-delay-rule-explicit-human-units-for-intended-time-delays)
  - [4.3 When to use `getTime<ticks>()`](#43-when-to-use-gettimeticks)
  - [4.4 When to use `getTime<ns>()`, `getTime<us>()`, or `getTime<ms>()`](#44-when-to-use-gettimens-gettimeus-or-gettimems)
  - [4.5 Runner-level and generic print timestamps](#45-runner-level-and-generic-print-timestamps)
  - [4.6 Scoreboard timestamp policy](#46-scoreboard-timestamp-policy)
  - [4.7 Simulator time precision metadata](#47-simulator-time-precision-metadata)
  - [4.8 Do not recreate RapidVPI time conversion in VIPs](#48-do-not-recreate-rapidvpi-time-conversion-in-vips)
- [5. Standard repository layout](#5-standard-repository-layout)
- [6. CMake integration model](#6-cmake-integration-model)
- [7. Namespaces, file naming, and public headers](#7-namespaces-file-naming-and-public-headers)
- [8. Protocol parameter objects](#8-protocol-parameter-objects)
- [9. Agent architecture](#9-agent-architecture)
  - [9.1 Master or initiator agents](#91-master-or-initiator-agents)
  - [9.2 Slave or responder agents](#92-slave-or-responder-agents)
  - [9.3 Monitor-only agents](#93-monitor-only-agents)
  - [9.4 Serial or pin-level protocol agents](#94-serial-or-pin-level-protocol-agents)
  - [9.5 Combined convenience agents](#95-combined-convenience-agents)
- [10. Scoreboard architecture](#10-scoreboard-architecture)
  - [10.1 Functional scoreboards](#101-functional-scoreboards)
  - [10.2 Rules scoreboards](#102-rules-scoreboards)
  - [10.3 Local DUT-specific scoreboards](#103-local-dut-specific-scoreboards)
- [11. Testcase architecture](#11-testcase-architecture)
- [12. RapidVPI phase discipline](#12-rapidvpi-phase-discipline)
- [13. Valid-ready and sampled-handshake discipline](#13-valid-ready-and-sampled-handshake-discipline)
- [14. Wide-vector and packed-field policy](#14-wide-vector-and-packed-field-policy)
- [15. Configuration knobs and additive evolution](#15-configuration-knobs-and-additive-evolution)
- [16. Error injection, backpressure, timing, and stress hooks](#16-error-injection-backpressure-timing-and-stress-hooks)
- [17. Memory models and protocol data models](#17-memory-models-and-protocol-data-models)
- [18. Logging, tracing, and debug controls](#18-logging-tracing-and-debug-controls)
- [19. Reset, lifecycle, and runner hooks](#19-reset-lifecycle-and-runner-hooks)
- [20. Documentation requirements](#20-documentation-requirements)
- [21. Creation checklist for a new `vip_xxx`](#21-creation-checklist-for-a-new-vip_xxx)
- [22. Example target shape for `vip_uart`](#22-example-target-shape-for-vip_uart)
- [23. Anti-patterns](#23-anti-patterns)

---

## 1. Scope and intent

A reusable `vip_xxx` package is a protocol-level verification package for RapidVPI-based cosimulation. It is not a DUT-specific testcase collection.

A reusable VIP should provide:

- protocol agents that hide pin-level mechanics from testcases
- optional protocol scoreboards and checkers
- protocol parameters and common transaction types
- optional trace/debug helpers
- documentation that explains how a project uses the package
- backward-compatible extension points for future features

A reusable VIP should not contain:

- assumptions about one DUT's address map, register map, command format, or wrapper-specific behavior unless that behavior is explicitly made configurable
- project-local testcase intent that belongs in `src/cases`
- hardcoded top-level net names beyond documented default prefix conventions
- a second copy of `vip_common`
- simulation launch policy; the parent project owns simulator execution
- raw VPI callback registration logic unless the VIP is intentionally adding a new low-level RapidVPI primitive

The reusable VIP should be usable as:

```text
<project>/ext/vip_xxx
```

and should normally depend on:

```text
<project>/ext/vip_common
```

---

## 2. Core design principles

### 2.1 Agents own protocol mechanics

Testcases should express intent. Agents should perform the pin-level or signal-level protocol work.

Good testcase style:

```cpp
const auto ticket = test.uart_tx.enqueue_byte("uart0", 0x55);
co_await test.uart_tx.wait_done(ticket);
```

Bad testcase style:

```cpp
// Do not manually toggle every protocol pin in every testcase
// when a reusable agent can own this behavior.
```

### 2.2 Scoreboards own checking policy

Agents may observe and forward events, but reusable comparison policy belongs in scoreboards.

Agents should call methods such as:

```cpp
scb_protocol->expect_txn(...);
scb_protocol->observe_txn(...);
scb_rules->observe_signal_event(...);
```

Do not bury major pass/fail behavior in random testcase code when it is protocol-generic.

### 2.3 Tests own intent, not mechanics

Testcases should describe scenarios:

- simple transfer
- backpressure
- timeout
- bad response
- random burst
- unaligned access
- framing error
- collision
- split transaction
- reset during transfer

The mechanics of driving, waiting, observing, and checking should be delegated to reusable helpers or agents.

### 2.4 Additive extension only after API stabilization

New features should be added without breaking existing users when the library is already considered stable.

Default policy for mature VIPs:

- keep old constructors working
- keep old method names working
- add new configuration structs or setter methods rather than changing existing signatures when the change is not trivial
- keep stricter scoreboard behavior opt-in unless every existing user requires it
- add new enum values rather than replacing old boolean behavior when the feature has more than two stable modes
- reset new per-case state in `reset_case()`
- document any changed default clearly in README

During early template or library cleanup, it is acceptable to make deliberate breaking changes if the result is a cleaner policy source. Do not preserve misleading names such as `time_ns` when the value is actually raw simulator ticks.

### 2.5 Protocol-generic goes into `vip_xxx`; DUT-specific stays local

Put behavior into reusable VIP only if it is true for the protocol or useful across multiple projects.

Examples of reusable behavior:

- AXI valid-ready transfer mechanics
- AXI burst beat counting
- UART 8N1 bit framing
- MDIO Clause 22 frame serialization
- AXIS packet monitor and TLAST handling

Examples of project-local behavior:

- one DUT's command word layout
- one bridge's reserved field rules
- one interconnect's address map
- one DMA's status folding policy
- one wrapper's local debug register meanings

---

## 3. RapidVPI public API reference

This section is intentionally detailed. Agents should not need to inspect RapidVPI installed headers to discover basic awaiter operations.

### 3.1 Include model and namespace

Typical VIP source files include RapidVPI through installed include paths configured by CMake:

```cpp
#include "testbase.hpp"
```

All current public RapidVPI testbase symbols are in namespace:

```cpp
namespace test {
}
```

Most project test classes derive from:

```cpp
class Test : public test::TestBase {
public:
    using RunTask = test::TestBase::RunTask;
    using RunUserTask = test::TestBase::RunUserTask;

    void initNets() override;
};
```

Common convenience in `.cpp` files:

```cpp
using test::ticks;
using test::ps;
using test::ns;
using test::us;
using test::ms;
```

or, for local functions only:

```cpp
using namespace test;
```

Do not put `using namespace test;` in public headers of reusable VIP packages unless that header is intentionally tiny and local. Prefer explicit `test::` in public headers.

### 3.2 Time units and raw simulator ticks

RapidVPI exposes these time-related public types and constants:

```cpp
namespace test {
    using sim_tick_t = std::uint64_t;

    enum class TimeUnit {
        ticks,
        ps,
        ns,
        us,
        ms
    };

    inline constexpr auto ticks = TimeUnit::ticks;
    inline constexpr auto ps    = TimeUnit::ps;
    inline constexpr auto ns    = TimeUnit::ns;
    inline constexpr auto us    = TimeUnit::us;
    inline constexpr auto ms    = TimeUnit::ms;
}
```

Meaning:

```text
ticks  raw simulator ticks from VPI time
ps     picoseconds
ns     nanoseconds
us     microseconds
ms     milliseconds
```

A raw tick is whatever the simulator effective precision says it is. If Questa is launched with `-t 1ps`, one raw VPI tick is one picosecond. If the effective VPI precision is `1ns`, one raw VPI tick is one nanosecond.

Do not assume ticks are nanoseconds.

Observed time should normally be stored as:

```cpp
test::sim_tick_t time_tick = rd.getTime<test::ticks>();
```

Converted human time is available when needed:

```cpp
double t_ns = rd.getTime<test::ns>();
double t_us = rd.getTime<test::us>();
double t_ms = rd.getTime<test::ms>();
```

### 3.3 `TestBase` lifecycle and net registration

A project test object derives from `test::TestBase` and implements:

```cpp
virtual void initNets() = 0;
```

`initNets()` registers all DUT nets that will be used by coroutines:

```cpp
void Test::initNets() {
    addNet("clk", 1);
    addNet("rst_n", 1);
    addNet("s_axis_tvalid", 1);
    addNet("s_axis_tready", 1);
    addNet("s_axis_tdata", 32);
}
```

Public net and metadata functions:

```cpp
void setDutName(const std::string& name);
void addNet(const std::string& key, unsigned int length);
vpiHandle getNetHandle(const std::string& key);
unsigned int getNetLength(const std::string& key);
```

Rules:

- `setDutName()` is normally called by project setup before `initNets()` runs.
- `addNet(key, length)` resolves the full VPI path as `dutName + "." + key`.
- Use stable short keys such as `clk`, `rst_n`, `m0_awvalid` in VIP/test code.
- `length` is the RTL net width in bits and is used for vector packing/unpacking.
- Missing or misspelled nets produce error prints and usually lead to failed callbacks or reads.

Public simulator precision metadata:

```cpp
int vpiTimePrecisionExp10() const noexcept;
long double vpiTickPeriodSeconds() const noexcept;
```

Use these for startup/debug metadata only. Do not implement your own repeated unit conversion in VIP code.

Example startup print:

```cpp
std::printf("[INFO] vpi_precision_exp10=%d tick_period_s=%.21Lg\n",
            test.vpiTimePrecisionExp10(),
            test.vpiTickPeriodSeconds());
```

### 3.4 `RunTask` and `RunUserTask`

RapidVPI uses two coroutine return types:

```cpp
test::TestBase::RunTask
test::TestBase::RunUserTask
```

Use `RunTask` for top-level testcase coroutines or long-lived agent coroutines registered with the test runner:

```cpp
Test::RunTask tc_smoke(Test& test) {
    co_await reset_case(test);
    co_return;
}
```

Use `RunUserTask` for nested helper coroutines that are themselves awaited by another coroutine:

```cpp
Test::RunUserTask wait_cycles(Test& test, unsigned cycles) {
    for (unsigned i = 0; i < cycles; ++i) {
        co_await clock_once(test);
    }
    co_return;
}
```

Rules:

- `RunTask` starts immediately and is used for top-level tests/agents.
- `RunUserTask` starts suspended and is resumed by `co_await`.
- Helpers such as reset, wait, polling loops, and protocol sub-steps should return `RunUserTask`.
- Do not manually destroy coroutine handles from user code.

### 3.5 Write awaitable: `getCoWrite`

Write awaiters schedule writes into the VPI write/update side. They are used to drive DUT inputs.

Factory APIs:

```cpp
TestBase::AwaitWrite getCoWrite();

template <test::TimeUnit U>
TestBase::AwaitWrite getCoWrite(test::delay_arg_t<U> delay);
```

Use no-argument `getCoWrite()` for the common zero-delay write phase:

```cpp
auto wr = test.getCoWrite();
wr.write("valid", 1);
wr.write("data", 0x55);
co_await wr;
```

Use explicit units for delayed writes:

```cpp
auto wr = test.getCoWrite<test::ns>(10.0);
wr.write("valid", 0);
co_await wr;
```

Use raw simulator ticks only when the delay is intentionally simulator-native:

```cpp
auto wr = test.getCoWrite<test::ticks>(10000);
wr.write("valid", 1);
co_await wr;
```

Do not use old forms:

```cpp
// Wrong with current RapidVPI API
// auto wr = test.getCoWrite(0);
// auto wr = test.getCoWrite(10.0);
```

Public `AwaitWrite` methods:

```cpp
void write(const std::string& net, unsigned long long value);
void write(const std::string& net, const std::string& value, unsigned int base = 2);

void force(const std::string& net, unsigned long long value);
void force(const std::string& net, const std::string& value, unsigned int base = 2);

void release(const std::string& net);

void setDelay();

template <test::TimeUnit U>
void setDelay(test::delay_arg_t<U> delay);
```

Numeric write examples:

```cpp
auto wr = test.getCoWrite();
wr.write("rst_n", 0);
wr.write("s_axis_tdata", 0x12345678ULL);
co_await wr;
```

Binary string write example:

```cpp
auto wr = test.getCoWrite();
wr.write("wide_bus", "10100011");
co_await wr;
```

Hex string write example:

```cpp
auto wr = test.getCoWrite();
wr.write("wide_bus", "DEADBEEF01234567", 16);
co_await wr;
```

Force/release examples:

```cpp
auto wr = test.getCoWrite();
wr.force("rx_i", 0);
co_await wr;

wr = test.getCoWrite();
wr.release("rx_i");
co_await wr;
```

`setDelay()` is available but should be uncommon. Prefer selecting the delay at construction:

```cpp
auto wr = test.getCoWrite();
wr.setDelay<test::ns>(10.0);
wr.write("sig", 1);
co_await wr;
```

Prefer this instead:

```cpp
auto wr = test.getCoWrite<test::ns>(10.0);
wr.write("sig", 1);
co_await wr;
```

Important write rules:

- Queue all writes before `co_await wr`.
- The actual `vpi_put_value()` calls happen when the awaiter resumes.
- Do not queue two writes to the same net in the same `AwaitWrite`; use one final value or separate write phases.
- Numeric writes are for signals up to 64 bits. For wider buses, use string writes.
- String writes preserve wide values and can represent `x` and `z` states.
- Use base `16` for hex input. Default base is binary.

### 3.6 Read awaitable: `getCoRead`

Read awaiters schedule a read-only synchronization and then sample requested nets. They are used to observe DUT outputs and internal registered state.

Factory APIs:

```cpp
TestBase::AwaitRead getCoRead();

template <test::TimeUnit U>
TestBase::AwaitRead getCoRead(test::delay_arg_t<U> delay);
```

Use no-argument `getCoRead()` for the common immediate read-only sample:

```cpp
auto rd = test.getCoRead();
rd.read("ready");
rd.read("data");
co_await rd;

const bool ready = (rd.getNum("ready") & 1ULL) != 0ULL;
const auto data = rd.getNum("data");
```

Use explicit units for delayed reads:

```cpp
auto rd = test.getCoRead<test::ns>(10.0);
rd.read("status");
co_await rd;
```

Use raw ticks only when the delay is intentionally simulator-native:

```cpp
auto rd = test.getCoRead<test::ticks>(10000);
rd.read("status");
co_await rd;
```

Public `AwaitRead` methods:

```cpp
void read(const std::string& net);

unsigned long long getNum(const std::string& net);
std::string getBinStr(const std::string& net);
std::string getHexStr(const std::string& net);

void setDelay();

template <test::TimeUnit U>
void setDelay(test::delay_arg_t<U> delay);

template <test::TimeUnit U>
test::time_value_t<U> getTime() const;
```

Timestamp examples:

```cpp
const test::sim_tick_t t_tick = rd.getTime<test::ticks>();
const double t_ns = rd.getTime<test::ns>();
```

Return type policy:

```text
rd.getTime<ticks>() returns test::sim_tick_t
rd.getTime<ps>()    returns double
rd.getTime<ns>()    returns double
rd.getTime<us>()    returns double
rd.getTime<ms>()    returns double
```

Important read rules:

- Call `rd.read(net)` before `co_await rd`.
- Call value getters after `co_await rd`.
- `getNum(net)` is intended only for signals that fit in 64 bits.
- `getNum(net)` consumes the stored numeric chunks for that read result. Do not call `getNum(net)` repeatedly for the same sampled value unless you intentionally know how the stored chunks behave.
- Use `getBinStr(net)` or `getHexStr(net)` for wide buses.
- `getBinStr(net)` returns a vector-width-oriented binary string that may be padded to a 32-bit chunk boundary.
- `getHexStr(net)` converts the binary string to hex and trims leading zeros.
- For protocol packets, agents should convert raw sampled strings into protocol-specific byte/field types instead of making testcases decode everything manually.

### 3.7 Change awaitable: `getCoChange`

Change awaiters wait for a VPI value-change callback on one net. They are useful for edge/change-driven monitors and simple status waits.

Factory APIs:

```cpp
TestBase::AwaitChange getCoChange(const std::string& net);
TestBase::AwaitChange getCoChange(const std::string& net,
                                  unsigned long long target_value);
```

Non-targeted change wait:

```cpp
auto chg = test.getCoChange("irq");
co_await chg;

const bool irq = (chg.getNum() & 1ULL) != 0ULL;
const test::sim_tick_t t_tick = chg.getTime<test::ticks>();
```

Targeted change wait:

```cpp
auto chg = test.getCoChange("done", 1);
co_await chg;
```

Public `AwaitChange` methods:

```cpp
unsigned long long getNum();
std::string getBinStr();
std::string getHexStr();

template <test::TimeUnit U>
test::time_value_t<U> getTime() const;
```

Return type policy:

```text
chg.getTime<ticks>() returns test::sim_tick_t
chg.getTime<ps>()    returns double
chg.getTime<ns>()    returns double
chg.getTime<us>()    returns double
chg.getTime<ms>()    returns double
```

Important change-wait rules:

- Use targeted `getCoChange(net, value)` mostly for scalar or small status signals.
- Avoid bus-equality waits on wide buses or multi-field protocol state.
- For bus conditions, use an edge-sampled read loop and bit masks.
- After any change/read observation, do not immediately schedule writes that are intended to affect the same sampled time slot. Move to a safe future write phase.

Preferred bit-mask polling pattern:

```cpp
while (true) {
    auto rd = test.getCoRead();
    rd.read("status");
    co_await rd;

    const auto status = rd.getNum("status");
    const bool done = (status & DONE_MASK) != 0ULL;

    if (done) {
        break;
    }

    co_await clock_once(test);
}
```

### 3.8 Value conversion helpers

`TestBase` exposes static conversion helpers:

```cpp
static char bin_to_hex_char(const std::string& bin);
static std::string bin_to_hex(const std::string& bin);
static std::string hex_to_bin(const std::string& hex);
```

Use cases:

```cpp
const std::string bin = test::TestBase::hex_to_bin("DEAD");
const std::string hex = test::TestBase::bin_to_hex("1101111010101101");
```

Rules:

- `hex_to_bin()` accepts `0-9`, `a-f`, `A-F`, `x`, `X`, `z`, and `Z`.
- Hex `X` maps to binary `xxxx`; hex `Z` maps to binary `zzzz`.
- `bin_to_hex()` supports `x` and `z`. Mixed-state nibbles that cannot be represented by one exact hex digit collapse to `X`.
- For exact mixed 4-state patterns, keep binary strings or protocol-specific vector helpers.

### 3.9 Test registration and factory registration

A project normally registers a concrete `Test` factory with RapidVPI through:

```cpp
namespace core {
    void registerTestFactory(std::function<std::unique_ptr<test::TestBase>()> factory);
}
```

User project must provide:

```cpp
extern "C" void userRegisterFactory() {
    core::registerTestFactory([]() -> std::unique_ptr<test::TestBase> {
        auto t = std::make_unique<Test>();
        t->setDutName("tb.dut");
        return t;
    });
}
```

Tests are registered through the `TestBase` API:

```cpp
void registerTest(const std::string& name,
                  std::function<std::coroutine_handle<>()> func);
```

Typical pattern inside `Test` construction/setup:

```cpp
registerTest("tc_smoke", [this]() {
    return tc_smoke().handle;
});
```

Project wrappers may hide this behind a local runner/helper. The important rule is that the registered function returns a coroutine handle for a `RunTask` coroutine.

### 3.10 RapidVPI API cheat sheet

Use this as the quick reference for generated VIP code.

| Intent | API | Notes |
|---|---|---|
| Zero-delay write phase | `auto wr = test.getCoWrite();` | Preferred replacement for old `getCoWrite(0)` |
| Delayed write in ns | `auto wr = test.getCoWrite<ns>(10.0);` | Explicit human unit |
| Delayed write in raw ticks | `auto wr = test.getCoWrite<ticks>(10000);` | Simulator-native delay |
| Queue numeric write | `wr.write("sig", 1);` | Use for width <= 64 |
| Queue binary string write | `wr.write("bus", "1010");` | Default base is 2 |
| Queue hex string write | `wr.write("bus", "DEAD", 16);` | Good for wide vectors |
| Force net | `wr.force("sig", 0);` | Uses VPI force |
| Release force | `wr.release("sig");` | Uses VPI release |
| Zero-delay read phase | `auto rd = test.getCoRead();` | Preferred replacement for old `getCoRead(0)` |
| Delayed read in ns | `auto rd = test.getCoRead<ns>(10.0);` | Explicit human unit |
| Request read | `rd.read("sig");` | Call before `co_await rd` |
| Get numeric read | `rd.getNum("sig")` | Use width <= 64 only |
| Get binary read | `rd.getBinStr("bus")` | Use for wide/4-state vectors |
| Get hex read | `rd.getHexStr("bus")` | Use for wide display/compare |
| Read timestamp raw | `rd.getTime<ticks>()` | Best for scoreboards/logging/CSV |
| Read timestamp ns | `rd.getTime<ns>()` | Use for human display only |
| Wait any change | `auto chg = test.getCoChange("sig");` | Change-driven monitor |
| Wait target value | `auto chg = test.getCoChange("done", 1);` | Prefer small/scalar signals |
| Change value numeric | `chg.getNum()` | After `co_await chg` |
| Change value hex/bin | `chg.getHexStr()`, `chg.getBinStr()` | After `co_await chg` |
| Change timestamp raw | `chg.getTime<ticks>()` | Best for scoreboards/logging/CSV |
| Sim precision exp10 | `test.vpiTimePrecisionExp10()` | Metadata, not repeated conversion |
| Tick period seconds | `test.vpiTickPeriodSeconds()` | Metadata/debug only |
| Register net | `addNet("sig", width)` | Usually in `initNets()` |
| Get net width | `getNetLength("sig")` | Rarely needed by agents |
| Hex to bin helper | `TestBase::hex_to_bin("DEAD")` | Public static helper |
| Bin to hex helper | `TestBase::bin_to_hex("1010")` | Public static helper |

---

## 4. Time, timestamps, logging, and scoreboard policy

### 4.1 Timestamp rule: ticks are the default truth

Observed timestamps should be stored and passed around as raw simulator ticks:

```cpp
test::sim_tick_t time_tick = rd.getTime<test::ticks>();
```

Why:

- raw ticks are exact
- no precision is lost through `double`
- no unit assumption leaks into generic VIP code
- logs and CSV can be post-processed using the simulator precision
- generic `vip_common` does not need to guess whether the user wants ps, ns, us, or ms

Do not name a raw tick field `time_ns`.

Good names:

```cpp
test::sim_tick_t time_tick = 0;
test::sim_tick_t start_tick = 0;
test::sim_tick_t end_tick = 0;
```

Bad names for raw ticks:

```cpp
double time_ns;
std::uint64_t sim_time_ns;
```

### 4.2 Delay rule: explicit human units for intended time delays

When code intentionally waits for real simulated time, use explicit units:

```cpp
co_await test.getCoWrite<test::ns>(10.0);
co_await test.getCoRead<test::us>(1.0);
```

When code intentionally waits raw simulator ticks, use `<ticks>`:

```cpp
co_await test.getCoWrite<test::ticks>(10000);
```

The no-delay phase APIs are unitless:

```cpp
co_await test.getCoWrite();
co_await test.getCoRead();
```

Do not write:

```cpp
co_await test.getCoWrite<test::ns>(0.0);
co_await test.getCoRead<test::ps>(0.0);
```

when the intent is simply “next write/read phase.” Use no-argument APIs.

### 4.3 When to use `getTime<ticks>()`

Use `getTime<ticks>()` when you already have a read/change awaiter and need to record the event time:

```cpp
auto rd = test.getCoRead();
rd.read("rx_valid");
rd.read("rx_data");
co_await rd;

const test::sim_tick_t tick = rd.getTime<test::ticks>();
scb.observe_rx_byte("uart0", byte, tick);
```

Use it for:

- scoreboard observed events
- expected/observed transaction timestamps
- trace records
- CSV event rows
- debug history buffers
- rules checker event ordering
- latency calculations that will remain in simulator ticks

`getTime<ticks>()` is the least ambiguous and cheapest timestamp path because it returns the stored raw tick count.

### 4.4 When to use `getTime<ns>()`, `getTime<us>()`, or `getTime<ms>()`

Use converted time only for human-facing display or when a protocol/user API explicitly wants physical units:

```cpp
const double t_ns = rd.getTime<test::ns>();
std::printf("[DBG] rx byte at %.3f ns\n", t_ns);
```

Good uses:

- one-off debug prints
- final human-readable report lines
- measured latency displayed to user
- diagnostics where the unit is printed next to the value

Avoid converted time for internal scoreboard storage:

```cpp
// Prefer this
scb.observe_frame(port, frame, rd.getTime<test::ticks>());

// Avoid this for generic storage
scb.observe_frame(port, frame, rd.getTime<test::ns>());
```

Converted time returns `double`, so it is a display/convenience view, not the canonical timestamp.

### 4.5 Runner-level and generic print timestamps

Sometimes a log line is not tied to a specific `AwaitRead` or `AwaitChange` object, for example:

```text
runner starting case
runner done
scoreboard total summary
agent queue depth report
```

Preferred policy:

- let `vip_common` logger/runner stamp lines with raw ticks automatically when available
- keep the log label explicit, for example `[tick=123456]`
- print simulator precision once near the beginning if useful

Example style:

```text
# [runner][INFO] vpi_precision_exp10=-12 tick_unit=1ps
# [runner][INFO][tick=0] starting tc_smoke
# [SCB][PASS][tick=25000] rx byte matched
# [runner][INFO][tick=90000] runner done
```

If a VIP helper needs a generic current timestamp and no awaiter is available, do not scatter raw VPI calls everywhere. Add or use one centralized `vip_common` helper such as:

```cpp
test::sim_tick_t sim_time_ticks();
```

That helper should stamp raw ticks. It should not be named `sim_time_ns()` unless it actually converts to ns.

Do not call `test::detail::current_vpi_time_ticks()` from reusable VIP code. It is an internal RapidVPI detail namespace.

### 4.6 Scoreboard timestamp policy

Reusable scoreboards should accept raw tick timestamps:

```cpp
void observe_frame(const std::string& port,
                   const UartFrame& frame,
                   test::sim_tick_t time_tick);
```

or optional tick timestamps:

```cpp
static constexpr test::sim_tick_t INVALID_TICK =
    std::numeric_limits<test::sim_tick_t>::max();

void note_fail(const std::string& msg,
               test::sim_tick_t time_tick = INVALID_TICK);
```

Do not design new reusable scoreboards around:

```cpp
double time_ns = -1.0;
```

That older style mixes display units with exact simulation event ordering.

If a scoreboard prints a human-readable timestamp, it may convert only at print time and must show the unit:

```text
[SCB][FAIL][tick=25000] rx mismatch
```

or:

```text
[SCB][FAIL][tick=25000][time_ns=25.000] rx mismatch
```

For normal logs, `[tick=...]` alone is preferred.

### 4.7 Simulator time precision metadata

RapidVPI reads the effective VPI time precision at simulation start:

```cpp
const int precision_exp10 = vpi_get(vpiTimePrecision, nullptr);
```

and stores it inside `TestBase`. User code can inspect:

```cpp
int precision = test.vpiTimePrecisionExp10();
long double tick_s = test.vpiTickPeriodSeconds();
```

Examples:

```text
precision=-12 -> one tick is 1ps  -> tick_period_s = 1e-12
precision=-9  -> one tick is 1ns  -> tick_period_s = 1e-9
precision=-6  -> one tick is 1us  -> tick_period_s = 1e-6
```

The simulator/project owns the actual resolution, for example through Questa `-t 1ps` and HDL `timeunit/timeprecision`. RapidVPI does not override the simulator resolution. It only reads the effective VPI precision and uses it for explicit unit conversion.

### 4.8 Do not recreate RapidVPI time conversion in VIPs

Do not write VIP-local conversion code such as:

```cpp
// Do not do this in reusable VIP code
const double t_ns = raw_ticks / 1000.0;
```

That assumes a specific precision. Use RapidVPI conversion:

```cpp
const double t_ns = rd.getTime<test::ns>();
```

or store raw ticks:

```cpp
const test::sim_tick_t t = rd.getTime<test::ticks>();
```

For current-time stamping outside an awaiter, centralize the helper in `vip_common` and keep it raw tick based.

---

## 5. Standard repository layout

Use this layout as the default for a new `vip_xxx` package:

```text
vip_xxx/
  CMakeLists.txt
  README.md
  AGENTS.md                    optional, but recommended for repo-local rules
  docs/
    user_guide.md              optional if README becomes too large
    protocol_notes.md          optional protocol notes, limits, and future hooks
  common/
    CMakeLists.txt
    xxx_params.hpp
    xxx_types.hpp
    xxx_bitvec.hpp             only if protocol needs custom wide-vector helpers
    xxx_vpi_utils.hpp          optional VPI read/write helper wrappers
    xxx_trace.hpp              optional trace declarations
    xxx_trace.cpp              optional trace implementation
  agents/
    xxx_master/
      CMakeLists.txt
      README.md
      master.hpp
      master_agent.cpp
      master_wr.cpp            when protocol has write/tx side
      master_rd.cpp            when protocol has read/rx side
      mst_coroutines_wr.cpp    low-level coroutine mechanics
      mst_coroutines_rd.cpp    low-level coroutine mechanics
    xxx_slave/
      CMakeLists.txt
      README.md
      slave.hpp
      slave_agent.cpp
      slave_wr.cpp
      slave_rd.cpp
      slv_coroutines_wr.cpp
      slv_coroutines_rd.cpp
      slv_checks_wr.cpp        optional deep/stability checks
      slv_checks_rd.cpp        optional deep/stability checks
    xxx_monitor/
      CMakeLists.txt           optional for passive-only monitor APIs
      README.md
      monitor.hpp
      monitor_agent.cpp
  scoreboard/
    xxx_scb/
      CMakeLists.txt
      README.md
      scb_xxx_tx.cpp
      scb_xxx_tx.hpp
      scb_xxx_rx.cpp
      scb_xxx_rx.hpp
      scb_xxx_rules.cpp
      scb_xxx_rules.hpp
```

This is a default, not a prison. Match the protocol shape.

For a simple one-direction serial protocol, this may be enough:

```text
vip_uart/
  CMakeLists.txt
  README.md
  common/
    CMakeLists.txt
    uart_params.hpp
    uart_types.hpp
  agents/
    uart_tx/
      CMakeLists.txt
      tx.hpp
      tx_agent.cpp
      tx_coroutines.cpp
    uart_rx/
      CMakeLists.txt
      rx.hpp
      rx_agent.cpp
      rx_coroutines.cpp
  scoreboard/
    uart_scb/
      CMakeLists.txt
      scb_uart_stream.hpp
      scb_uart_stream.cpp
      scb_uart_rules.hpp
      scb_uart_rules.cpp
```

For a packet protocol with no master/slave meaning, use semantic names:

```text
agents/packet_source
agents/packet_sink
agents/packet_monitor
```

For a bus protocol where the common industry terms are master/slave, use master/slave. Keep that notation consistent.

---

## 6. CMake integration model

A reusable VIP normally contributes object libraries to the parent project's shared-library target.

Root `CMakeLists.txt` should be small:

```cmake
# vip_xxx — protocol VIP for RapidVPI/C++ coroutines
# Included under <project>/ext/vip_xxx.
# Builds object libraries and contributes them to parent ${PROJECT_NAME}.

add_subdirectory(common)
add_subdirectory(agents/xxx_master)
add_subdirectory(agents/xxx_slave)
add_subdirectory(scoreboard/xxx_scb)
```

Each subfolder should build a globally unique object target:

```cmake
set(TEST_SUBFOLDER vip_xxx_master)

add_library(${TEST_SUBFOLDER} OBJECT
        master_agent.cpp
        master_wr.cpp
        master_rd.cpp
        mst_coroutines_wr.cpp
        mst_coroutines_rd.cpp
)

target_include_directories(${TEST_SUBFOLDER} PRIVATE
        ${CMAKE_SOURCE_DIR}/ext
        /usr/local/include/rapidvpi/core
        /usr/local/include/rapidvpi/scheduler
        /usr/local/include/rapidvpi/testbase
        /usr/local/include/rapidvpi/testmanager
        ${vpi_include_dir}
)

set_property(TARGET ${TEST_SUBFOLDER} PROPERTY POSITION_INDEPENDENT_CODE ON)

target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:${TEST_SUBFOLDER}>)
```

Rules:

- Do not build `vip_common` again inside a protocol VIP.
- Keep target names globally unique, for example `vip_uart_tx`, not just `tx`.
- Do not create unrelated executables for agents or scoreboards.
- Let the parent project build the final RapidVPI `.so`.
- Use `${CMAKE_SOURCE_DIR}/ext` so includes work as `"vip_xxx/..."` and `"vip_common/..."`.
- Keep CMake boring and predictable.
- Keep simulator launch targets in the RTL/project repo, not inside reusable VIP packages.

---

## 7. Namespaces, file naming, and public headers

Use one namespace per protocol VIP:

```cpp
namespace vip::uart {
}
```

or:

```cpp
namespace vip::axi {
}
```

Class names should be explicit:

```text
UartTx
UartRx
UartMonitor
ScbUartStream
ScbUartRules
AxiMaster
AxiSlave
ScbAxiWrite
ScbAxiRead
```

Headers should be included through the `ext` root path:

```cpp
#include "vip_uart/common/uart_params.hpp"
#include "vip_uart/agents/uart_tx/tx.hpp"
#include "vip_uart/scoreboard/uart_scb/scb_uart_stream.hpp"
```

Default policy:

- Put public agent APIs in the agent header, for example `master.hpp`, `slave.hpp`, `tx.hpp`, `rx.hpp`.
- Put transaction structs and parameters under `common/`.
- Keep private helper structs inside `.cpp` files unless testcases or other agents need them.
- Avoid dumping every helper into one giant public header.
- Keep header dependencies as small as practical, but do not over-engineer.
- In public headers, prefer explicit `test::TestBase`, `test::sim_tick_t`, and `test::ns` over broad namespace imports.

---

## 8. Protocol parameter objects

Every reusable VIP should have a central parameter/config object under `common/`.

Example:

```cpp
struct UartParams {
    unsigned data_bits = 8;
    unsigned stop_bits = 1;
    bool parity_enable = false;
    bool parity_odd = false;

    unsigned bit_clks = 16;
    unsigned sample_clk_index = 8;

    bool lsb_first = true;
};
```

Avoid using `ticks` in protocol-local names when the value means clock cycles or oversampling ticks. In RapidVPI, `ticks` means raw simulator ticks. For protocol-local counts, prefer names such as:

```text
bit_clks
sample_clk_index
idle_poll_clks
gap_cycles
response_cycles
```

For buses, parameter objects should centralize widths and protocol encoding helpers:

```cpp
struct XxxParams {
    unsigned data_bits = 32;
    unsigned addr_bits = 32;
    unsigned id_bits = 4;

    unsigned data_bytes() const {
        return (data_bits + 7u) / 8u;
    }
};
```

Rules:

- Keep protocol constants in `common/xxx_params.hpp` or `common/xxx_types.hpp`.
- Do not scatter width math across agents and scoreboards.
- Make derived helpers available through methods.
- Keep parameter objects cheap to copy unless they intentionally own shared state.
- Avoid compile-time templates unless they clearly reduce code and do not harm runtime configurability.
- Runtime-configurable VIP is preferred because the same compiled VIP often verifies multiple DUT configurations.
- If a parameter is a RapidVPI raw simulator tick, name it `*_tick` or `*_ticks` and document that it is `test::sim_tick_t`-compatible.
- If a parameter is a protocol clock count, name it `*_clk`, `*_clks`, or `*_cycles`.

---

## 9. Agent architecture

An agent is a long-lived object owned by the project `Test` class. It uses RapidVPI coroutines to drive or monitor a protocol interface.

Typical construction:

```cpp
class Test : public test::TestBase {
public:
    Test()
        : scb()
        , uart_params()
        , scb_uart(scb, uart_params)
        , uart_tx(*this, {"uart0"}, uart_params)
        , uart_rx(*this, {"uart0"}, uart_params)
    {
        uart_tx.attach_scoreboards(&scb_uart);
        uart_rx.attach_scoreboards(&scb_uart);
    }

    vip::common::Scoreboard scb;
    vip::uart::UartParams uart_params;
    vip::uart::ScbUartStream scb_uart;
    vip::uart::UartTx uart_tx;
    vip::uart::UartRx uart_rx;
};
```

Agents should usually provide:

```cpp
using RunTask = test::TestBase::RunTask;

RunTask agent(unsigned idx);
void attach_scoreboards(...);
```

For multi-port agents, constructors should accept either a count or explicit base names:

```cpp
XxxMaster(test::TestBase& tb, unsigned count, XxxParams params = XxxParams{});
XxxMaster(test::TestBase& tb, std::vector<std::string> names, XxxParams params = XxxParams{});
```

Base names should derive signal names from suffixes. Example:

```text
base = "uart0"
uart0_rx_i
uart0_tx_o
uart0_cts_i
uart0_rts_o
```

For bus protocols:

```text
base = "m0"
m0_awvalid
m0_awready
m0_awaddr
```

Never require testcases to hardcode every pin name if a stable prefix convention exists. If the protocol or DUT wrapper does not match the default convention, add a port-map/config object rather than hacking the testcase.

### 9.1 Master or initiator agents

Use a master/initiator agent when the VIP starts transactions into the DUT.

It should own:

- command queues
- ticket allocation
- write-side and read-side worker coroutines if applicable
- completion tracking
- optional response capture queues
- optional trace emission
- expected-side scoreboard calls when enqueueing stimulus
- master-side observed completion calls when accepting responses

Public API style:

```cpp
unsigned enqueue_write(...);
unsigned enqueue_read(...);

test::TestBase::RunUserTask wait_done(unsigned ticket);
bool is_done(unsigned ticket) const;

void set_trace_enable(const std::string& port, bool enable);
void set_backpressure_mode(...);
```

For protocols with split directions, keep internal split files:

```text
master_wr.cpp
master_rd.cpp
mst_coroutines_wr.cpp
mst_coroutines_rd.cpp
```

The high-level API belongs in `master.hpp`. Low-level pin mechanics belong in coroutine `.cpp` files.

### 9.2 Slave or responder agents

Use a slave/responder agent when the VIP responds to transactions initiated by the DUT.

It should own:

- ready/acceptance policy
- response generation
- optional memory model
- response delay or error injection
- accepted-side observation hooks
- deep protocol checks
- optional automatic scoreboard completion
- configuration knobs for realistic stalls and corner cases

Public API style:

```cpp
void set_ready_mode(const std::string& port, ReadyMode mode);
void set_response_value(const std::string& port, unsigned resp);
void set_response_delay_once(const std::string& port, unsigned id, unsigned cycles);
void set_memory_write_enable(const std::string& port, bool enable);
void set_stability_check_enable(const std::string& port, bool enable);
```

Use enum modes when the behavior is likely to grow:

```cpp
enum class ReadyMode : unsigned {
    ALWAYS = 0,
    PULSE_RANDOM = 1,
    SCRIPTED = 2
};
```

Avoid too many fragile booleans when a stable mode enum is clearer.

### 9.3 Monitor-only agents

Use monitor agents when no driving is needed or when a passive checker is useful.

Monitor agents should:

- sample handshakes only at real accepted edges
- never change DUT-visible signals
- forward observed events to functional and rules scoreboards
- optionally build trace records
- expose lightweight history/capture APIs for testcase-specific checks

Example API:

```cpp
void attach_scoreboards(ScbXxxStream* stream, ScbXxxRules* rules);
test::TestBase::RunTask agent(unsigned idx);

std::vector<XxxObservedTxn> get_history(const std::string& port) const;
void clear_history(const std::string& port);
```

### 9.4 Serial or pin-level protocol agents

For UART, MDIO, SPI, I2C-like, or other pin-level protocols, split line-level behavior cleanly:

- TX/source agent: drives a line or set of pins from queued frames
- RX/sink agent: samples a line or set of pins into observed frames
- monitor/checker: optional passive protocol decoder
- timing configuration: explicit bit/cycle timing in params
- error injection: optional and off by default

Serial agents must avoid hidden assumptions about absolute time. If timing is based on a project clock, use clock cycles. If timing is based on raw simulator delay, use `getCoWrite<ticks>()` or `getCoRead<ticks>()` explicitly and document it.

For UART-like protocols:

```text
TX agent:
  idle line setup
  start bit
  data bits
  optional parity
  stop bits
  optional gap/break generation

RX agent:
  wait for start edge
  sample near bit center
  shift data bits
  sample parity if enabled
  sample stop bits
  report byte/frame/error to scoreboard
```

All bit-level sampling must respect RapidVPI read/write phase rules. RX sampling is observation. TX line driving is writing and must be scheduled in a safe write phase before the sampled edge that matters.

### 9.5 Combined convenience agents

A protocol may provide a convenience wrapper that owns multiple agents, but keep the leaf agents reusable.

Example:

```cpp
class UartEndpoint {
public:
    UartTx tx;
    UartRx rx;
};
```

The wrapper is optional. It should not hide useful knobs or prevent direct access to the leaf agents.

---

## 10. Scoreboard architecture

All reusable VIP scoreboards should report through `vip_common` scoreboard/logging facilities when available.

Typical construction:

```cpp
vip::common::Scoreboard scb;
vip::uart::ScbUartStream scb_uart(scb, uart_params);
vip::uart::ScbUartRules scb_uart_rules(scb, uart_params);
```

Each scoreboard should provide:

```cpp
void reset_case();
void end_case_check(bool fail_on_outstanding = true);
void set_verbose(bool en);
```

Rules scoreboards may use:

```cpp
void set_enable(bool en);
void set_warn_only(bool en);
```

Scoreboard event APIs should use raw ticks:

```cpp
void observe_txn(const std::string& port,
                 const XxxTxn& txn,
                 test::sim_tick_t time_tick);
```

### 10.1 Functional scoreboards

Functional scoreboards compare expected protocol transactions against observed protocol transactions.

Examples:

```text
ScbAxiWrite:
  expected writes vs observed write stream, closes on master BRESP

ScbAxiRead:
  expected reads vs observed R stream, closes on master RLAST

ScbUartStream:
  expected TX/RX byte or frame stream vs observed decoded frames
```

Functional scoreboards should provide clear expected-side APIs:

```cpp
void expect_frame(const std::string& port, const UartFrame& frame);
void expect_bytes(const std::string& port, const std::vector<std::uint8_t>& bytes);
```

and clear observed-side APIs:

```cpp
void observe_frame(const std::string& port,
                   const UartFrame& frame,
                   test::sim_tick_t time_tick);

void observe_error(const std::string& port,
                   UartError err,
                   test::sim_tick_t time_tick);
```

End-of-case checks should catch outstanding expectations and unexpected observations.

### 10.2 Rules scoreboards

Rules scoreboards check protocol legality/invariants independent from a specific expected transaction list.

Examples:

- payload stable while valid is asserted and ready is low
- no `WLAST` before expected final beat
- response ID matches an outstanding request
- UART stop bit must be inactive level
- parity bit matches selected parity mode
- MDIO turnaround behavior matches selected clause
- AXIS `TLAST` terminates the packet at legal time

Default policy:

- rules scoreboards may default disabled if they are strict or new
- warning-only mode should exist for bring-up
- fatal mode should exist for locked regression
- noisy checks must be configurable
- event time should be passed as `test::sim_tick_t` when available

### 10.3 Local DUT-specific scoreboards

When a checker depends on one DUT's semantics, keep it in the project, not reusable VIP.

Examples:

```text
src/scoreboard/scb_datamover_status.hpp
src/scoreboard/scb_decoder_routing.hpp
src/scoreboard/scb_uart_bridge_cmd_status.hpp
```

Local scoreboards may use reusable VIP observations, but should not push DUT-only meanings into `vip_xxx`.

---

## 11. Testcase architecture

In a project using reusable VIPs:

```text
src/test.cpp
src/test.hpp
src/init.cpp
src/pindefs.hpp
src/cases/tc_utils.hpp
src/cases/tc_utils.cpp
src/cases/tc_name.hpp
src/cases/tc_name.cpp
```

Policy:

- `src/test.cpp` is thin orchestration only.
- `src/init.cpp` registers VPI nets and widths.
- `src/pindefs.hpp` contains local top-level net names, widths, and DUT constants.
- `src/cases/tc_utils.*` contains shared testcase helpers.
- each `tc_xxx.hpp` registers the testcase
- each `tc_xxx.cpp` contains the testcase coroutine body
- each testcase does its own reset/precondition setup when needed
- each testcase calls agent APIs rather than manually toggling protocol pins

Preferred testcase shape:

```cpp
void register_tc_smoke(vip::common::Runner& runner) {
    runner.add("tc_smoke", [](Test& test) -> Test::RunTask {
        co_await reset_case_local(test);

        const auto ticket = test.agent.enqueue_transfer(...);
        co_await test.agent.wait_done(ticket);

        co_return;
    });
}
```

Do not place large reusable protocol code in `tc_xxx.cpp`. If it becomes reusable, move it to an agent or `tc_utils`.

---

## 12. RapidVPI phase discipline

The most important rule:

After an RO/read-observation await, do not schedule writes that are supposed to affect the same sampled RTL time slot.

Treat these as RO/read-observation awaits:

```cpp
co_await test.getCoRead();
co_await test.getCoRead<test::ns>(...);
co_await test.getCoChange(...);
co_await test.utils.clock(...);
```

Treat these as write-side awaits:

```cpp
co_await test.getCoWrite();
co_await test.getCoWrite<test::ns>(...);
co_await test.utils.write_barrier();
```

Recommended pattern when a condition is observed and a drive must happen next cycle:

```cpp
while (true) {
    auto rd = test.getCoRead();
    rd.read("ready");
    co_await rd;

    const bool ready = (rd.getNum("ready") & 1ULL) != 0ULL;

    co_await test.utils.clock();

    if (ready) {
        break;
    }
}

co_await test.utils.clock_to_write(1, 0);

{
    auto wr = test.getCoWrite();
    wr.write("valid", 1);
    co_await wr;
}
```

Avoid:

```cpp
auto rd = test.getCoRead();
rd.read("ready");
co_await rd;

auto wr = test.getCoWrite();
wr.write("valid", 1);
co_await wr;
```

when the write is meant to affect the same edge just observed.

For RapidVPI/VVP-style phase safety:

- after any RO await, do not schedule writes in the same time slot
- use an edge-sampled RO loop to check conditions
- after the condition is met, perform writes in the next cycle using a fresh `getCoWrite()`
- avoid broad bus-equality waits; use bit masks

---

## 13. Valid-ready and sampled-handshake discipline

For valid-ready protocols:

- drive payload before asserting valid when ready may already be high
- keep payload stable while valid is high
- keep valid asserted until an actual sampled `valid && ready` clock edge
- clear or change payload only after the accepting edge
- do not compress immediately-ready transfers into unsafe post-RO writes

Preferred drive sequence:

```cpp
co_await test.utils.clock_to_write(1, 0);

{
    auto wr = test.getCoWrite();
    write_payload(wr, payload);
    wr.write("valid", 0);
    co_await wr;
}

co_await test.utils.write_barrier();

{
    auto wr = test.getCoWrite();
    write_payload(wr, payload);
    wr.write("valid", 1);
    co_await wr;
}

while (true) {
    co_await test.utils.clock();

    auto rd = test.getCoRead();
    rd.read("valid");
    rd.read("ready");
    co_await rd;

    const bool valid = (rd.getNum("valid") & 1ULL) != 0ULL;
    const bool ready = (rd.getNum("ready") & 1ULL) != 0ULL;

    if (valid && ready) {
        break;
    }
}

co_await test.utils.clock_to_write(1, 0);

{
    auto wr = test.getCoWrite();
    wr.write("valid", 0);
    co_await wr;
}
```

For non-valid-ready sampled protocols, apply the same idea:

- drive before the sampling edge
- sample only after the clock/tick that defines protocol observation
- do not modify outputs in the same phase used for observing inputs
- for line protocols, make bit-center sampling explicit

---

## 14. Wide-vector and packed-field policy

Never use numeric RapidVPI writes or reads for buses wider than 64 bits.

Policy:

- use string/hex or bit-vector helpers for wide writes
- preserve leading zeros where protocol width matters
- preserve exact width in protocol-specific helper objects
- prefer reusable helper types such as `XxxBitVec`, `AxiBitVec`, or protocol-specific byte vectors
- use `getNum()` only for signals that fit within 64 bits
- use `getHexStr()` or protocol helper reads for wider signals
- pack and decode fields exactly as documented
- never silently reverse MSB/LSB field order
- do not make testcases manually slice huge strings if an agent can decode the protocol

A common reusable type pattern:

```cpp
using XxxDataWord = XxxBitVec;
using XxxMask = XxxBitVec;
```

For protocols with byte streams, prefer byte vectors internally and convert to bit vectors only at the VPI boundary.

Wide write example:

```cpp
auto wr = test.getCoWrite();
wr.write("wide_data", "00112233445566778899AABBCCDDEEFF", 16);
co_await wr;
```

Wide read example:

```cpp
auto rd = test.getCoRead();
rd.read("wide_data");
co_await rd;

const std::string data_hex = rd.getHexStr("wide_data");
```

---

## 15. Configuration knobs and additive evolution

Every feature knob should have a clear owner and default.

Good knob categories:

```text
Protocol shape:
  data width, address width, ID width, parity enable, stop bits

Timing:
  bit_clks, sample_clk_index, response delay, valid gap cycles

Backpressure:
  ready mode, bubble period, random pulse mode, scripted stalls

Error injection:
  response value, parity error, framing error, dropped packet, bad CRC

Checking:
  strict mode, warn-only mode, enable deep checks, verbose mode

Tracing:
  trace enable, output path, port filter, compression enable
```

Add knobs in a way that does not break old code once the VIP is stable:

```cpp
void set_verbose(bool en);
void set_warn_only(bool en);
void set_ready_mode(const std::string& port, ReadyMode mode);
void set_error_injection_enable(const std::string& port, bool enable);
```

For one-shot behavior, make it explicit:

```cpp
void arm_next_response_error(const std::string& port, unsigned resp);
void arm_next_frame_error(const std::string& port, UartError err);
```

For persistent behavior, use `set_...`:

```cpp
void set_default_response(const std::string& port, unsigned resp);
void set_read_data_mode(const std::string& port, ReadDataMode mode);
```

For history/capture features:

```cpp
void set_history_enable(bool en);
void clear_history();
std::vector<XxxObservedTxn> get_history(...) const;
```

Do not turn on expensive history by default unless it is cheap and useful.

---

## 16. Error injection, backpressure, timing, and stress hooks

Reusable VIPs should have stress hooks because serious DUTs fail under timing variation, not only under ideal transfers.

Useful hooks:

```text
ready/valid stalls
response delays
random gaps with seed control
one-shot bad response
per-beat response sequence
last-beat stalls
partial packet pauses
line glitches
framing/parity errors
timeout injection
reset during active transfer
```

Rules:

- default behavior should be simple and deterministic
- random behavior must allow a fixed seed
- one-shot injection should affect exactly one transaction/frame unless documented otherwise
- persistent injection should be explicit and easy to disable
- knobs must reset appropriately between cases if they are testcase-local
- persistent configuration may survive across cases only when documented

For serial protocols, include line-level stress hooks later even if first version is minimal:

```text
insert idle gap
send break
force bad stop bit
force parity error
jitter bit timing within safe scripted bounds
glitch RX line for N simulator ticks or N protocol cycles
```

Do not implement all stress hooks in version one if it causes bloat. Design the API so they can be added without breaking the first users.

---

## 17. Memory models and protocol data models

Responder agents often need a backing model.

Examples:

```text
AXI slave:
  byte-addressable memory model
  constant read data mode
  response scripting

AXI-Lite slave:
  register map model
  read/write callback hooks

UART endpoint:
  TX byte queue
  RX byte queue
  frame/error queue

MDIO device:
  register bank per PHY/device/register
```

Policy:

- keep protocol-generic memory/register models inside reusable VIP only if they apply broadly
- keep DUT-specific register meanings project-local
- provide direct helper APIs for setup and inspection
- make byte ordering explicit
- make alignment policy explicit
- ensure memory writes and direct helper writes are consistent
- expose small debug/history helpers, but do not require testcases to inspect internals for normal pass/fail

Example style:

```cpp
void write_mem_byte(const std::string& port, std::uint64_t addr, std::uint8_t data);
bool read_mem_byte(const std::string& port, std::uint64_t addr, std::uint8_t& data) const;
void clear_memory(const std::string& port);
void share_memory(const std::string& dst, const std::string& src);
```

---

## 18. Logging, tracing, and debug controls

Use `vip_common` logging style when available.

Logging policy:

- keep normal logs useful and not noisy
- verbose logs must be controlled by `set_verbose()`
- bring-up logs should be guardable or removable
- include protocol/agent name, port name, and simulation tick when useful
- failure messages should explain expected vs observed values
- do not print huge payloads by default
- converted time values must include their unit in the log key/name

Preferred timestamp style:

```text
[SCB][PASS][tick=123456] expected byte matched
```

Optional human-time debug style:

```text
[DBG][tick=123456][time_ns=123.456] sampled start bit
```

Do not print:

```text
[time=123456 ns]
```

unless the value was actually converted to ns.

Trace policy:

- tracing is optional
- tracing must be configurable per port when practical
- trace output path must be configurable
- trace records should be structured enough to post-process
- trace enable should default off unless the trace is tiny
- trace timestamps should be raw ticks by default

Common trace object style:

```cpp
struct TraceCfg {
    bool enable = false;
    std::string out_path;
    bool compress = true;
};
```

---

## 19. Reset, lifecycle, and runner hooks

Every reusable scoreboard and agent must have clear reset/lifecycle behavior.

Scoreboards:

```cpp
void reset_case();
void end_case_check(bool fail_on_outstanding = true);
```

Agents should provide a way to clear queues or reset testcase-local state:

```cpp
void reset_case();
void clear_pending();
```

The project runner should normally do:

```cpp
runner.set_before_case_hook([this](const Runner::CaseDesc& c) {
    scb.reset_case();

    scb_protocol.reset_case();
    scb_rules.reset_case();

    agent.reset_case();

    scb.start_case(c.name);
});

runner.set_after_case_hook([this](const Runner::CaseDesc&) {
    scb_protocol.end_case_check(true);
    scb_rules.end_case_check(false);

    scb.end_case();
    scb.print_case_summary();
    scb.print_total_summary();
});
```

Agents must handle DUT reset cleanly:

- drive outputs to safe idle when appropriate
- clear active low-level transfer state when reset requires it
- avoid leaving stale command tickets in an ambiguous state
- make reset behavior documented for queued commands
- if reset during transaction is supported, expose testcase helpers for it

---

## 20. Documentation requirements

Every reusable VIP must have a top-level `README.md` with a clickable table of contents.

Top-level README should include:

- what the VIP provides
- dependencies
- how to instantiate in `Test`
- how to configure params
- how to attach scoreboards
- common testcase examples
- known limits
- RapidVPI time/tick policy if the VIP exposes timestamps
- additive integration notes

Each agent folder README should include:

- purpose
- construction
- naming convention
- public API
- timing/phase rules
- common pitfalls
- examples

Each scoreboard folder README should include:

- functional vs rules scoreboards
- wiring in `test.hpp`/`test.cpp`
- lifecycle hooks
- expected-side APIs
- observed-side APIs
- knobs and defaults
- timestamp policy
- end-of-case behavior

For a new VIP, initial documentation can be compact, but must still make the API usable without reading all `.cpp` files.

---

## 21. Creation checklist for a new `vip_xxx`

When creating a new reusable VIP, do this in order:

1. Define the protocol scope.
   - What is version one?
   - What is explicitly future work?
   - What protocol variants are supported?
   - What protocol variants are intentionally not supported yet?

2. Define the transaction model.
   - Frame, packet, beat, command, register access, byte stream, or bus transfer?
   - What fields belong in `common/xxx_types.hpp`?
   - What fields are protocol-generic vs DUT-specific?
   - Which timestamps are needed, and are they stored as raw ticks?

3. Define the parameter object.
   - widths
   - timing
   - feature enables
   - derived helpers
   - avoid `ticks` in names unless it means raw simulator ticks

4. Define agents.
   - master/initiator, slave/responder, source/sink, tx/rx, monitor
   - public enqueue APIs
   - completion APIs
   - port naming convention
   - scoreboards attachment points

5. Define scoreboards.
   - functional expected vs observed
   - rules/protocol legality
   - timestamp type: `test::sim_tick_t`
   - default strictness
   - warn-only behavior
   - end-of-case checks

6. Define CMake structure.
   - root adds subdirectories
   - each section builds an object library
   - object libraries add to `${PROJECT_NAME}`

7. Define documentation.
   - top README
   - agent README
   - scoreboard README
   - RapidVPI API usage examples if custom helpers wrap awaiters

8. Define minimum compile smoke.
   - headers include cleanly
   - object targets compile
   - no full simulation unless explicitly requested by user

9. Define extension hooks.
   - future modes as enums
   - one-shot injection APIs
   - history capture
   - trace config
   - backwards-compatible constructors once stable

10. Keep version one small.
    - Make the skeleton correct.
    - Implement the first real path.
    - Add future hooks only where they avoid API churn.
    - Do not bloat first version with every possible feature.

---

## 22. Example target shape for `vip_uart`

First version should be minimal but extensible.

Scope:

```text
UART 8N1
one start bit
8 data bits
no parity
one stop bit
configurable bit timing
TX driver
RX sampler/decoder
basic stream scoreboard
basic framing error reporting
```

Future extension points:

```text
parity
2 stop bits
break condition
glitch/noise injection
RTS/CTS helper behavior
multi-byte FIFO helper APIs
baud mismatch/jitter stress
```

Suggested files:

```text
vip_uart/
  CMakeLists.txt
  README.md
  common/
    CMakeLists.txt
    uart_params.hpp
    uart_types.hpp
  agents/
    uart_tx/
      CMakeLists.txt
      README.md
      tx.hpp
      tx_agent.cpp
      tx_coroutines.cpp
    uart_rx/
      CMakeLists.txt
      README.md
      rx.hpp
      rx_agent.cpp
      rx_coroutines.cpp
  scoreboard/
    uart_scb/
      CMakeLists.txt
      README.md
      scb_uart_stream.hpp
      scb_uart_stream.cpp
      scb_uart_rules.hpp
      scb_uart_rules.cpp
```

Suggested types:

```cpp
struct UartParams {
    unsigned data_bits = 8;
    unsigned stop_bits = 1;
    bool parity_enable = false;
    bool parity_odd = false;
    unsigned bit_clks = 16;
    unsigned sample_clk_index = 8;
    bool idle_high = true;
};

struct UartFrame {
    std::uint8_t data = 0;
    bool parity_error = false;
    bool framing_error = false;
    test::sim_tick_t start_tick = 0;
    test::sim_tick_t end_tick = 0;
};
```

Suggested TX API:

```cpp
class UartTx {
public:
    using RunTask = test::TestBase::RunTask;

    UartTx(test::TestBase& tb,
           std::vector<std::string> ports,
           UartParams params = UartParams{},
           std::string reset_name = "rst_n");

    void attach_scoreboards(ScbUartStream* stream, ScbUartRules* rules = nullptr);

    RunTask agent(unsigned idx);

    unsigned enqueue_byte(const std::string& port, std::uint8_t data);
    unsigned enqueue_bytes(const std::string& port, const std::vector<std::uint8_t>& data);
    test::TestBase::RunUserTask wait_done(unsigned ticket);

    void set_inter_frame_gap_clks(const std::string& port, unsigned cycles);
    void arm_next_framing_error(const std::string& port);
};
```

Suggested RX API:

```cpp
class UartRx {
public:
    using RunTask = test::TestBase::RunTask;

    UartRx(test::TestBase& tb,
           std::vector<std::string> ports,
           UartParams params = UartParams{},
           std::string reset_name = "rst_n");

    void attach_scoreboards(ScbUartStream* stream, ScbUartRules* rules = nullptr);

    RunTask agent(unsigned idx);

    void set_capture_enable(const std::string& port, bool enable);
    std::vector<UartFrame> get_history(const std::string& port) const;
    void clear_history(const std::string& port);
};
```

Suggested scoreboard API:

```cpp
class ScbUartStream {
public:
    explicit ScbUartStream(vip::common::Scoreboard& scb,
                           UartParams params = UartParams{});

    void reset_case();
    void end_case_check(bool fail_on_outstanding = true);

    void set_verbose(bool en);
    void set_strict_framing(bool en);

    void expect_byte(const std::string& port, std::uint8_t data);
    void expect_bytes(const std::string& port, const std::vector<std::uint8_t>& data);

    void observe_frame(const std::string& port,
                       const UartFrame& frame,
                       test::sim_tick_t time_tick);
};
```

Important first-version constraint:

Do not implement RTS/CTS by changing the stable TX/RX API later. Add it as optional params and optional methods when needed.

---

## 23. Anti-patterns

Avoid these:

- creating a local protocol driver in every testcase
- creating a new VIP that duplicates an existing `ext/vip_xxx`
- hardcoding one DUT's signal names into reusable VIP
- making testcases manually drive valid-ready buses when an agent exists
- putting DUT command/status layout into a protocol-generic VIP
- changing existing constructors when a setter or config object would preserve compatibility in a stable VIP
- turning strict new checks on by default and breaking old tests unexpectedly
- using `getNum()` for wide buses
- using bus-equality waits that can stall
- writing after an RO await and expecting the write to affect the same sampled edge
- adding random behavior without a seed
- requiring a previous `vip_xxx` repo as reference when this guide already defines the style
- creating nested git submodules inside `ext/`
- hiding all implementation in one huge header
- making README stale after adding APIs
- naming raw tick timestamps as `time_ns`
- using old implicit unit APIs such as `getCoWrite(0)`, `getCoRead(0)`, `getCoWrite(10.0)`, or `getTime()`
- using protocol-local `*_ticks` names when the value is really clock cycles
- calling `test::detail::*` helpers from reusable VIP code
- scattering direct `vpi_get_time()` conversions across agents or scoreboards
