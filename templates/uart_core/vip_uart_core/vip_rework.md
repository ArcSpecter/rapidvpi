# RapidVPI VIP rework guide after explicit time/tick API refactor

This document is a generic rework manual for updating existing `vip_xxx` projects after the RapidVPI time API cleanup.

It was written against the current `vip_uart_core` project shape, but the policy is intentionally universal. Apply the same steps to any existing `vip_axi`, `vip_axil`, `vip_axis`, `vip_mdio`, `vip_eth`, or other `vip_xxx` project that still uses the old implicit nanosecond RapidVPI APIs or ambiguous timestamp names.

The main goals are:

- replace obsolete RapidVPI calls such as `getCoWrite(0)` and `getCoRead(0)`
- replace implicit nanosecond delays with explicit unit templates
- store observed timestamps as raw simulator ticks
- stop naming raw tick values as `time_ns`
- print case start/end/delta timing uniformly through the runner
- print final runner time and total simulated runtime
- keep protocol-local clock counts distinct from raw VPI simulator ticks
- make each VIP project compile cleanly against the new RapidVPI core without agents digging through `/usr/local/include/rapidvpi`

Testing and documentation refresh can happen after this rework. This file is focused on source code rework.

---

## Table of contents

- [1. RapidVPI API baseline](#1-rapidvpi-api-baseline)
- [2. Global search audit](#2-global-search-audit)
- [3. Universal replacement table](#3-universal-replacement-table)
- [4. Rework `vip_common` first](#4-rework-vip_common-first)
  - [4.1 Logger and current-time helpers](#41-logger-and-current-time-helpers)
  - [4.2 Centralized tick conversion helpers](#42-centralized-tick-conversion-helpers)
  - [4.3 Scoreboard timestamp model](#43-scoreboard-timestamp-model)
  - [4.4 Runner case timing and final runtime print](#44-runner-case-timing-and-final-runtime-print)
  - [4.5 CommonUtils delay and phase helpers](#45-commonutils-delay-and-phase-helpers)
  - [4.6 Clock agent](#46-clock-agent)
  - [4.7 POR/reset helper](#47-porreset-helper)
- [5. Rework reusable `vip_xxx` protocol packages](#5-rework-reusable-vip_xxx-protocol-packages)
  - [5.1 Rename protocol-local ticks that are actually clock cycles](#51-rename-protocol-local-ticks-that-are-actually-clock-cycles)
  - [5.2 Transaction/frame timestamp fields](#52-transactionframe-timestamp-fields)
  - [5.3 TX/source agents](#53-txsource-agents)
  - [5.4 RX/sink/monitor agents](#54-rxsinkmonitor-agents)
  - [5.5 Protocol scoreboards and rules checkers](#55-protocol-scoreboards-and-rules-checkers)
- [6. Rework project-local DUT agents and scoreboards](#6-rework-project-local-dut-agents-and-scoreboards)
- [7. Rework testcase files](#7-rework-testcase-files)
  - [7.1 Do not manually duplicate case start/end timing in every `tc_*.cpp`](#71-do-not-manually-duplicate-case-startend-timing-in-every-tc_cpp)
  - [7.2 Keep subcase logs, but let runner own case timing](#72-keep-subcase-logs-but-let-runner-own-case-timing)
  - [7.3 If a project does not use `vip_common::Runner`](#73-if-a-project-does-not-use-vip_commonrunner)
- [8. `vip_uart_core`-specific file checklist](#8-vip_uart_core-specific-file-checklist)
- [9. Smart timestamp usage rules](#9-smart-timestamp-usage-rules)
- [10. Final grep checklist](#10-final-grep-checklist)
- [11. Expected output style](#11-expected-output-style)
- [12. Anti-patterns to eliminate](#12-anti-patterns-to-eliminate)

---

## 1. RapidVPI API baseline

The current RapidVPI core uses explicit time units:

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

Use the new API forms:

```cpp
auto wr = tb.getCoWrite();
auto rd = tb.getCoRead();

auto wr_delay_ns = tb.getCoWrite<test::ns>(10.0);
auto rd_delay_us = tb.getCoRead<test::us>(1.0);
auto wr_delay_ticks = tb.getCoWrite<test::ticks>(10000);

const test::sim_tick_t t_tick = rd.getTime<test::ticks>();
const double t_ns = rd.getTime<test::ns>();
```

The old forms are obsolete and must be removed from VIP projects:

```cpp
// obsolete
getCoWrite(0)
getCoRead(0)
getCoWrite(10.0)
getCoRead(10.0)
rd.getTime()
chg.getTime()
```

The important distinction:

```text
Observed timestamps  -> raw simulator ticks by default
Intended delays      -> explicit ns/us/ms/ps or explicit raw ticks
Zero phase barrier   -> no-argument getCoWrite()/getCoRead()
```

---

## 2. Global search audit

Before editing, run these searches from the project root.

```bash
grep -R "getCoWrite(0\|getCoRead(0" -n src ext --exclude-dir=.git

grep -R "getCoWrite([0-9.]\|getCoRead([0-9.]" -n src ext --exclude-dir=.git

grep -R "\.getTime()\|getTime()" -n src ext --exclude-dir=.git

grep -R "delay_ns\|sim_time_ns\|time_ns\|start_time_ns\|end_time_ns" -n src ext --exclude-dir=.git

grep -R "bit_ticks\|sample_tick\|idle_poll_ticks\|frame_ticks\|inter_frame_gap_ticks\|rts_wait_timeout_ticks" -n src ext --exclude-dir=.git
```

For the current `vip_uart_core` snapshot, the stale categories include:

```text
ext/vip_common/common/logger.hpp
ext/vip_common/common/common.hpp/.cpp
ext/vip_common/runner/runner.hpp/.cpp
ext/vip_common/scoreboard/scoreboard.hpp/.cpp
ext/vip_common/agents/clock/clock.hpp/.cpp
ext/vip_common/agents/por/por.hpp/.cpp
ext/vip_uart/common/uart_params.hpp
ext/vip_uart/common/uart_types.hpp
ext/vip_uart/agents/uart_tx/*
ext/vip_uart/agents/uart_rx/*
ext/vip_uart/scoreboard/uart_scb/*
src/agents/uart_core_intf/*
src/scoreboard/scb_uart_core.*
src/cases/tc_*.cpp
src/test.cpp
```

Do not fix these with compatibility aliases. This is a cleanup pass. Compile errors from old call sites are useful because they force all stale code to be modernized.

---

## 3. Universal replacement table

Apply this table throughout `src/` and `ext/`.

| Old pattern | New pattern | Notes |
|---|---|---|
| `tb.getCoWrite(0)` | `tb.getCoWrite()` | Mandatory beautification. Zero-delay write phase has no unit. |
| `tb.getCoRead(0)` | `tb.getCoRead()` | Mandatory beautification. Zero-delay read phase has no unit. |
| `tb.getCoWrite(delay)` | `tb.getCoWrite<test::ns>(delay)` | Only when old `delay` was intended to be ns. |
| `tb.getCoRead(delay)` | `tb.getCoRead<test::ns>(delay)` | Only when old `delay` was intended to be ns. |
| `utils.delay_ns(x)` | `utils.delay<test::ns>(x)` | Replace helper name with explicit-unit template. |
| `utils.delay_ns(ps / 1000.0)` | `utils.delay<test::ps>(ps)` | Do not convert ps to ns manually. |
| `rd.getTime()` | `rd.getTime<test::ticks>()` or `<test::ns>()` | Use ticks for storage, ns for display. |
| `chg.getTime()` | `chg.getTime<test::ticks>()` or `<test::ns>()` | Same policy as read awaiter. |
| `vip::common::sim_time_ns()` | `vip::common::sim_time_ticks()` | Existing helper returns raw ticks, not ns. Rename. |
| `double time_ns` for event storage | `test::sim_tick_t time_tick` | Raw event time is ticks. |
| `start_time_ns` / `end_time_ns` | `start_tick` / `end_tick` | For frame/transaction timestamps. |
| `case_start_time_ns_` / `case_end_time_ns_` | `case_start_tick_` / `case_end_tick_` | Scoreboard/runner storage. |
| `bit_ticks` when meaning clock edges | `bit_clks` | Do not confuse protocol clock counts with raw VPI ticks. |
| `sample_tick` when meaning clock index | `sample_clk_index` | UART/sample terminology. |
| `idle_poll_ticks` when meaning clock edges | `idle_poll_clks` | Protocol-local cycles/clks. |
| `frame_ticks()` when meaning clock edges | `frame_clks()` | UART frame length in clock edges. |
| `inter_frame_gap_ticks` | `inter_frame_gap_clks` | Unless it is truly raw simulator ticks. |
| `rts_wait_timeout_ticks` | `rts_wait_timeout_clks` | In current UART agent this is clock count. |

Allowed remaining physical-unit names:

```text
CLK_PERIOD_NS       OK: physical project clock period constant
phase_offset_ps     OK: physical phase offset in picoseconds
bit_time_ns         OK if explicitly used with getCoWrite<ns>() / utils.delay<ns>()
```

But raw timestamp fields must not be named `*_ns`.

---

## 4. Rework `vip_common` first

Do this before touching protocol VIP packages or project-local agents. Most other changes become mechanical once `vip_common` exposes the right helpers.

### 4.1 Logger and current-time helpers

File:

```text
ext/vip_common/common/logger.hpp
```

Current stale helper:

```cpp
inline std::uint64_t sim_time_ns();
```

This function actually returns raw VPI simulator ticks. Rename it and make the output label explicit.

Recommended public helper set:

```cpp
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

#include <rapidvpi/testbase/testbase.hpp>
#include <vpi_user.h>

namespace vip::common {

using TestBase = ::test::TestBase;
using sim_tick_t = ::test::sim_tick_t;

inline constexpr sim_tick_t INVALID_TICK =
    std::numeric_limits<sim_tick_t>::max();

[[nodiscard]] inline sim_tick_t sim_time_ticks() noexcept {
    s_vpi_time t{};
    t.type = vpiSimTime;
    vpi_get_time(nullptr, &t);

    const auto high = static_cast<std::uint32_t>(t.high);
    const auto low  = static_cast<std::uint32_t>(t.low);

    return (static_cast<sim_tick_t>(high) << 32u)
         | static_cast<sim_tick_t>(low);
}

[[nodiscard]] inline bool valid_tick(const sim_tick_t tick) noexcept {
    return tick != INVALID_TICK;
}

inline void log_line(const std::string& src,
                     const std::string& level,
                     const std::string& msg) {
    SimLogger log;
    log << "# [" << src << "][" << level << "][tick=" << sim_time_ticks()
        << "] " << msg << std::endl;
}

} // namespace vip::common
```

Notes:

- Do not keep `sim_time_ns()` as an alias unless the project is intentionally maintaining compatibility. For this cleanup pass, remove it and fix all call sites.
- Use `s_vpi_time t{}` instead of manually initializing `high/low`.
- Cast VPI `high/low` through `std::uint32_t` before packing.
- All generic log lines should show `[tick=...]`, not `[123]` and not `[time_ns=...]`.

### 4.2 Centralized tick conversion helpers

Add conversion helpers in `vip_common`, not inside every protocol agent.

Good location:

```text
ext/vip_common/common/logger.hpp
```

or a new small header:

```text
ext/vip_common/common/time.hpp
```

If creating `time.hpp`, include it from `logger.hpp`, `scoreboard.hpp`, `runner.hpp`, and `common.hpp`.

Suggested helpers:

```cpp
namespace vip::common {

[[nodiscard]] inline long double ticks_to_seconds(const TestBase& tb,
                                                  const sim_tick_t ticks) noexcept {
    return static_cast<long double>(ticks) * tb.vpiTickPeriodSeconds();
}

[[nodiscard]] inline long double ticks_to_ns_ld(const TestBase& tb,
                                                const sim_tick_t ticks) noexcept {
    return ticks_to_seconds(tb, ticks) / 1.0e-9L;
}

[[nodiscard]] inline double ticks_to_ns(const TestBase& tb,
                                        const sim_tick_t ticks) noexcept {
    return static_cast<double>(ticks_to_ns_ld(tb, ticks));
}

[[nodiscard]] inline sim_tick_t delta_ticks(const sim_tick_t start,
                                            const sim_tick_t end) noexcept {
    if (!valid_tick(start) || !valid_tick(end) || end < start) {
        return 0;
    }
    return end - start;
}

[[nodiscard]] inline double delta_ticks_to_ns(const TestBase& tb,
                                              const sim_tick_t start,
                                              const sim_tick_t end) noexcept {
    return ticks_to_ns(tb, delta_ticks(start, end));
}

[[nodiscard]] inline std::string format_ns(const double ns_value,
                                           const unsigned precision = 3u) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(static_cast<int>(precision)) << ns_value;
    return oss.str();
}

[[nodiscard]] inline std::string tick_unit_label(const TestBase& tb) {
    switch (tb.vpiTimePrecisionExp10()) {
        case -15: return "1fs";
        case -12: return "1ps";
        case -9:  return "1ns";
        case -6:  return "1us";
        case -3:  return "1ms";
        case 0:   return "1s";
        default:  return "1e" + std::to_string(tb.vpiTimePrecisionExp10()) + "s";
    }
}

} // namespace vip::common
```

For helpers that need to convert a user-specified duration to raw ticks, implement it centrally in `vip_common` only if needed by utilities that must store periods internally, such as `Clock`.

```cpp
template <test::TimeUnit U>
[[nodiscard]] inline sim_tick_t duration_to_ticks(const TestBase& tb,
                                                  const test::delay_arg_t<U> delay) {
    if constexpr (U == test::TimeUnit::ticks) {
        return delay;
    }
    else {
        if (delay <= 0.0) {
            return 0;
        }

        const long double raw_ticks =
            static_cast<long double>(delay) * test::TimeUnitSeconds<U>::value /
            tb.vpiTickPeriodSeconds();

        // Match RapidVPI's policy: never schedule early.
        constexpr long double rel_eps = 1.0e-12L;
        const long double nearest = std::floor(raw_ticks + 0.5L);
        const long double diff = raw_ticks > nearest ? raw_ticks - nearest : nearest - raw_ticks;
        const long double scale = raw_ticks > 1.0L ? raw_ticks : 1.0L;
        const long double rounded = (diff <= rel_eps * scale) ? nearest : std::ceil(raw_ticks);

        return static_cast<sim_tick_t>(rounded);
    }
}
```

Use this helper only in `vip_common` components that really need stored tick durations. Protocol agents should normally call RapidVPI directly through `getCoWrite<U>()`, `getCoRead<U>()`, or `CommonUtils::delay<U>()`.

### 4.3 Scoreboard timestamp model

Files:

```text
ext/vip_common/scoreboard/scoreboard.hpp
ext/vip_common/scoreboard/scoreboard.cpp
```

Replace the old model:

```cpp
double time_ns = -1.0;
double case_start_time_ns_ = -1.0;
double case_end_time_ns_ = -1.0;
```

with raw tick fields:

```cpp
vip::common::sim_tick_t time_tick = vip::common::INVALID_TICK;
vip::common::sim_tick_t case_start_tick_ = vip::common::INVALID_TICK;
vip::common::sim_tick_t case_end_tick_ = vip::common::INVALID_TICK;
```

Recommended public API:

```cpp
class Scoreboard {
public:
    explicit Scoreboard(TestBase& tb);

    void start_case(const std::string& case_name,
                    sim_tick_t time_tick = INVALID_TICK);
    void end_case(sim_tick_t time_tick = INVALID_TICK);

    void note_info(const std::string& msg,
                   sim_tick_t time_tick = INVALID_TICK);
    void note_pass(const std::string& msg,
                   sim_tick_t time_tick = INVALID_TICK);
    void note_warn(const std::string& msg,
                   sim_tick_t time_tick = INVALID_TICK);
    void note_fail(const std::string& msg,
                   sim_tick_t time_tick = INVALID_TICK);

private:
    TestBase& tb_;
};
```

Implementation policy:

- If `start_case()` receives `INVALID_TICK`, stamp it with `sim_time_ticks()`.
- If `end_case()` receives `INVALID_TICK`, stamp it with `sim_time_ticks()`.
- If `note_*()` receives `INVALID_TICK`, store/print the message without an event tick.
- If `note_*()` receives a valid tick, print `[tick=...]`.
- `print_case_summary()` should include delta when start/end are valid.

Example event print:

```text
[FAIL][tick=25000] uart_core RX byte-side mismatch expected ... observed ...
```

Example case summary:

```text
[SCB][CASE] 'tc_basic' info=2 pass=14 warn=0 fail=0 start_tick=0 end_tick=50000 delta_ticks=50000 delta_ns=50.000
```

Update project construction from:

```cpp
, scb()
```

to:

```cpp
, scb(*this)
```

Do not pass converted nanoseconds into scoreboard APIs. Scoreboard APIs should receive `test::sim_tick_t`.

### 4.4 Runner case timing and final runtime print

Files:

```text
ext/vip_common/runner/runner.hpp
ext/vip_common/runner/runner.cpp
```

Rename private helper:

```cpp
static std::uint64_t sim_time_ns_();
```

to:

```cpp
static sim_tick_t sim_time_ticks_();
```

`Runner` owns `TestBase& tb_`, so it is the best universal place to print per-case start/end timing and final run timing.

Add runner-level timing fields or locals:

```cpp
sim_tick_t runner_start_tick = sim_time_ticks_();
```

At the start of `case_runner()`, print simulator precision metadata once:

```text
# [runner][INFO][tick=0] vpi_precision_exp10=-12 tick_unit=1ps
```

Around every testcase body:

```cpp
if (before_case_) {
    before_case_(c);
}

const sim_tick_t case_start_tick = sim_time_ticks_();
log_line_("INFO", "case " + std::to_string(k + 1) + "/"
                  + std::to_string(order.size()) + " start: " + c.name);

co_await c.fn();

const sim_tick_t case_end_tick = sim_time_ticks_();
const sim_tick_t case_delta_tick = delta_ticks(case_start_tick, case_end_tick);
const double case_delta_ns = ticks_to_ns(tb_, case_delta_tick);

log_line_("INFO",
          "case " + std::to_string(k + 1) + "/"
          + std::to_string(order.size()) + " end: " + c.name
          + " delta_ticks=" + std::to_string(case_delta_tick)
          + " delta_ns=" + format_ns(case_delta_ns));

if (after_case_) {
    after_case_(c);
}
```

This means every `tc_` prints start/end/delta without adding boilerplate to every `tc_*.cpp`.

At final runner completion:

```cpp
const sim_tick_t runner_end_tick = sim_time_ticks_();
const sim_tick_t runner_delta_tick = delta_ticks(runner_start_tick, runner_end_tick);
const double runner_delta_ns = ticks_to_ns(tb_, runner_delta_tick);

log_line_("INFO",
          "runner done. total_delta_ticks=" + std::to_string(runner_delta_tick)
          + " total_delta_ns=" + format_ns(runner_delta_ns));
```

Make sure `log_line_()` itself prints `[tick=current_tick]`.

Example final output:

```text
# [runner][INFO][tick=0] vpi_precision_exp10=-12 tick_unit=1ps
# [runner][INFO][tick=0] case 1/8 start: tc_basic
# [runner][INFO][tick=724000] case 1/8 end: tc_basic delta_ticks=724000 delta_ns=724.000
# [runner][INFO][tick=9134000] runner done. total_delta_ticks=9134000 total_delta_ns=9134.000
```

### 4.5 CommonUtils delay and phase helpers

Files:

```text
ext/vip_common/common/common.hpp
ext/vip_common/common/common.cpp
```

Current stale API:

```cpp
RunUserTask delay_ns(double delay) const;
```

Replace with explicit-unit template:

```cpp
template <test::TimeUnit U>
RunUserTask delay(test::delay_arg_t<U> delay) const {
    co_await tb_.getCoWrite<U>(delay);
    co_return;
}
```

Keep `write_barrier()` and `clock_to_write()`, but update internals:

```cpp
CommonUtils::RunUserTask CommonUtils::write_barrier() const {
    co_await tb_.getCoWrite();
    co_return;
}
```

Update `waitFor()`:

```cpp
auto rd = tb_.getCoRead();
```

not:

```cpp
auto rd = tb_.getCoRead(0);
```

Update call sites:

```cpp
co_await utils_.delay<test::ns>(10.0);
co_await utils_.delay<test::ps>(500.0);
co_await utils_.delay<test::ticks>(10000);
```

Do not keep `delay_ns()` as a compatibility alias for this cleanup pass.

### 4.6 Clock agent

Files:

```text
ext/vip_common/agents/clock/clock.hpp
ext/vip_common/agents/clock/clock.cpp
```

The old public API hides ns:

```cpp
RunUserTask start(double period_ns);
RunUserTask set_period(double period_ns);
double period_ns() const;
```

Replace it with explicit-unit API:

```cpp
template <test::TimeUnit U>
RunUserTask start(test::delay_arg_t<U> period);

template <test::TimeUnit U>
RunUserTask set_period(test::delay_arg_t<U> period);

RunUserTask stop();

test::sim_tick_t period_ticks() const { return period_req_ticks_; }
```

Recommended internal storage:

```cpp
test::sim_tick_t period_req_ticks_ = 10;
test::sim_tick_t period_applied_ticks_ = 10;
```

Convert the user-provided period once using centralized `vip_common::duration_to_ticks<U>(tb_, period)`.

Clock run loop should use raw tick delays internally:

```cpp
const test::sim_tick_t high_ticks = period_applied_ticks_ / 2u;
const test::sim_tick_t low_ticks  = period_applied_ticks_ - high_ticks;

co_await utils_.delay<test::ticks>(high_ticks);
co_await utils_.delay<test::ticks>(low_ticks);
```

Minimum period policy:

- reject or clamp periods smaller than two raw simulator ticks
- do not allow a free-running clock to loop with zero-delay high/low phases

For example:

```cpp
if (period_ticks < 2u) {
    period_ticks = 2u;
}
```

Update all old call sites:

```cpp
co_await test.clock_agent.start<test::ns>(CLK_PERIOD_NS);
co_await test.clock_agent.set_period<test::ns>(CLK_PERIOD_NS);
```

### 4.7 POR/reset helper

Files:

```text
ext/vip_common/agents/por/por.hpp
ext/vip_common/agents/por/por.cpp
```

Old API:

```cpp
RunUserTask assert_reset(double hold_ns = 0.0);
RunUserTask deassert_reset(double settle_ns = 0.0);
RunUserTask pulse_reset(double hold_ns, double settle_ns = 0.0);
```

New API should separate no-delay phase operation from explicit unit waits:

```cpp
RunUserTask assert_reset();
RunUserTask deassert_reset();

template <test::TimeUnit U>
RunUserTask assert_reset(test::delay_arg_t<U> hold);

template <test::TimeUnit U>
RunUserTask deassert_reset(test::delay_arg_t<U> settle);

template <test::TimeUnit U>
RunUserTask pulse_reset(test::delay_arg_t<U> hold,
                        test::delay_arg_t<U> settle = 0);
```

Implementation pattern:

```cpp
Por::RunUserTask Por::assert_reset() {
    auto wr = tb_.getCoWrite();
    wr.write(rst_net_, assert_val_);
    co_await wr;
    co_return;
}

template <test::TimeUnit U>
Por::RunUserTask Por::assert_reset(test::delay_arg_t<U> hold) {
    co_await assert_reset();
    if (vip::common::duration_to_ticks<U>(tb_, hold) != 0u) {
        co_await utils_.delay<U>(hold);
    }
    co_return;
}
```

Update call sites:

```cpp
co_await test.por.assert_reset<test::ns>(CLK_PERIOD_NS * 4.0);
co_await test.por.deassert_reset();
co_await test.por.pulse_reset<test::ns>(100.0, 50.0);
```

Do not write:

```cpp
co_await test.por.deassert_reset(0.0);
```

Use no-argument `deassert_reset()`.

---

## 5. Rework reusable `vip_xxx` protocol packages

After `vip_common` compiles, update protocol VIPs.

### 5.1 Rename protocol-local ticks that are actually clock cycles

In `vip_uart`, names like `bit_ticks` are not raw VPI simulator ticks. They are counts of testbench clock rising edges per UART bit. Rename them.

Current stale UART names:

```cpp
unsigned bit_ticks;
unsigned sample_tick;
unsigned idle_poll_ticks;
unsigned frame_ticks() const;
unsigned inter_frame_gap_ticks;
unsigned rts_wait_timeout_ticks;
RunUserTask wait_ticks_(unsigned ticks);
void set_inter_frame_gap_ticks(...);
void set_rts_wait_timeout_ticks(...);
void observe_rts_blocked(... waited_ticks);
```

Recommended names:

```cpp
unsigned bit_clks;
unsigned sample_clk_index;
unsigned idle_poll_clks;
unsigned frame_clks() const;
unsigned inter_frame_gap_clks;
unsigned rts_wait_timeout_clks;
RunUserTask wait_clks_(unsigned clks);
void set_inter_frame_gap_clks(...);
void set_rts_wait_timeout_clks(...);
void observe_rts_blocked(... waited_clks);
```

Update all project code using these names:

```cpp
params.bit_clks * 4u
params.frame_clks() + HANDSHAKE_TIMEOUT_CYCLES
```

Not:

```cpp
params.bit_ticks * 4u
params.frame_ticks()
```

Rule:

```text
Use *_ticks only for raw VPI simulator ticks.
Use *_clks or *_cycles for protocol clock-edge counts.
```

### 5.2 Transaction/frame timestamp fields

File examples:

```text
ext/vip_uart/common/uart_types.hpp
src/agents/uart_core_intf/intf_types.hpp
```

Replace:

```cpp
double start_time_ns = -1.0;
double end_time_ns = -1.0;
double time_ns = -1.0;
```

with:

```cpp
test::sim_tick_t start_tick = vip::common::INVALID_TICK;
test::sim_tick_t end_tick = vip::common::INVALID_TICK;
test::sim_tick_t time_tick = vip::common::INVALID_TICK;
```

Headers that define these structs should include RapidVPI or `vip_common` time definitions:

```cpp
#include "vip_common/common/logger.hpp"
```

or a dedicated time header if one was created:

```cpp
#include "vip_common/common/time.hpp"
```

Do not store both tick and ns fields by default. Store ticks; convert only when printing.

### 5.3 TX/source agents

File examples:

```text
ext/vip_uart/agents/uart_tx/tx.hpp
ext/vip_uart/agents/uart_tx/tx_agent.cpp
ext/vip_uart/agents/uart_tx/tx_coroutines.cpp
```

Mechanical API updates:

```cpp
auto wr = tb_.getCoWrite();
auto rd = tb_.getCoRead();
co_await utils_.delay<test::ns>(item.bit_time_ns);
co_await utils_.delay<test::ps>(item.phase_offset_ps);
```

not:

```cpp
auto wr = tb_.getCoWrite(0);
auto rd = tb_.getCoRead(0);
co_await utils_.delay_ns(item.bit_time_ns);
co_await utils_.delay_ns(static_cast<double>(item.phase_offset_ps) / 1000.0);
```

For UART TX, keep physical-time phase fields if they really mean physical time:

```cpp
double bit_time_ns = 0.0;
std::uint64_t phase_offset_ps = 0u;
```

Those names are valid because they are physical units, not event timestamps.

But rename protocol clock-count fields:

```cpp
inter_frame_gap_ticks -> inter_frame_gap_clks
rts_wait_timeout_ticks -> rts_wait_timeout_clks
wait_ticks_ -> wait_clks_
```

For generated frame history:

```cpp
sent.start_tick = vip::common::sim_time_ticks();
...
sent.end_tick = vip::common::sim_time_ticks();
```

When a timestamp is tied to an existing read/change awaiter, prefer `getTime<ticks>()`. For pure write-side events where no read awaiter exists, `vip::common::sim_time_ticks()` is appropriate.

### 5.4 RX/sink/monitor agents

File examples:

```text
ext/vip_uart/agents/uart_rx/rx.hpp
ext/vip_uart/agents/uart_rx/rx_agent.cpp
ext/vip_uart/agents/uart_rx/rx_coroutines.cpp
```

Mechanical updates:

```cpp
auto rd = tb_.getCoRead();
auto wr = tb_.getCoWrite();
```

not:

```cpp
auto rd = tb_.getCoRead(0);
auto wr = tb_.getCoWrite(0);
```

For helper functions that sample a line, expose the sample timestamp when useful:

```cpp
RunUserTask sample_line_(PortState& port,
                         bool& value,
                         test::sim_tick_t* time_tick = nullptr);
```

Implementation:

```cpp
auto rd = tb_.getCoRead();
rd.read(port.cfg.rx_net);
co_await rd;

value = (rd.getNum(port.cfg.rx_net) & 1u) != 0u;
if (time_tick != nullptr) {
    *time_tick = rd.getTime<test::ticks>();
}
```

Then capture frame timestamps from actual sampled read events:

```cpp
test::sim_tick_t sample_tick = vip::common::INVALID_TICK;
co_await sample_line_(port, start_sample, &sample_tick);
frame.start_tick = sample_tick;
```

For end-of-frame:

```cpp
co_await sample_line_(port, stop_sample, &sample_tick);
frame.end_tick = sample_tick;
```

If no awaiter timestamp is available, use `vip::common::sim_time_ticks()`.

### 5.5 Protocol scoreboards and rules checkers

File examples:

```text
ext/vip_uart/scoreboard/uart_scb/scb_uart_stream.hpp/.cpp
ext/vip_uart/scoreboard/uart_scb/scb_uart_rules.hpp/.cpp
```

Update all observed timestamp usage:

```cpp
scb_.note_fail(msg, frame.end_tick);
scb_.note_warn(msg, frame.end_tick);
scb_.note_pass(msg, frame.end_tick);
```

not:

```cpp
scb_.note_fail(msg, frame.end_time_ns);
```

Rules checker helper:

```cpp
void note_rule_(const std::string& msg,
                test::sim_tick_t time_tick = vip::common::INVALID_TICK);
```

not:

```cpp
void note_rule_(const std::string& msg, double time_ns = -1.0);
```

Update wording of clock-count logs:

```cpp
"RTS wait timeout clks=" + std::to_string(waited_clks)
```

not:

```cpp
"RTS wait timeout ticks="
```

unless it is truly raw VPI tick count.

---

## 6. Rework project-local DUT agents and scoreboards

Project-local agents usually have the most stale code because they directly poke DUT pins.

For `vip_uart_core`, update:

```text
src/agents/uart_core_intf/intf.hpp
src/agents/uart_core_intf/intf.cpp
src/agents/uart_core_intf/intf_types.hpp
src/scoreboard/scb_uart_core.hpp
src/scoreboard/scb_uart_core.cpp
```

Mechanical awaiter changes:

```cpp
auto rd = tb_.getCoRead();
auto wr = tb_.getCoWrite();
```

Event timestamp changes:

```cpp
rec.time_tick = rd.getTime<test::ticks>();
```

not:

```cpp
rec.time_ns = static_cast<double>(vip::common::sim_time_ns());
```

Important: when the event is detected from a read awaiter, use that awaiter's timestamp. It is already captured and avoids another VPI time query.

Example in `pop_rx_byte()`:

```cpp
auto rd = tb_.getCoRead();
rd.read(rx_byte_valid);
rd.read(rx_byte_data);
rd.read(rx_byte_frame_error);
rd.read(rx_byte_parity_error);
rd.read(rx_byte_break_detect);
co_await rd;

if ((rd.getNum(rx_byte_valid) & 1u) != 0u) {
    rec.valid = true;
    rec.data = static_cast<std::uint8_t>(rd.getNum(rx_byte_data) & 0xffu);
    rec.frame_error = (rd.getNum(rx_byte_frame_error) & 1u) != 0u;
    rec.parity_error = (rd.getNum(rx_byte_parity_error) & 1u) != 0u;
    rec.break_detect = (rd.getNum(rx_byte_break_detect) & 1u) != 0u;
    rec.time_tick = rd.getTime<test::ticks>();
    break;
}
```

Update local scoreboard calls:

```cpp
scb_.note_fail("uart_core unexpected RX byte-side record "
               + rx_record_string_(rec),
               rec.time_tick);
```

not:

```cpp
scb_.note_fail(..., rec.time_ns);
```

---

## 7. Rework testcase files

### 7.1 Do not manually duplicate case start/end timing in every `tc_*.cpp`

The user-visible requirement is:

```text
Each tc must print when it starts, when it ends, and how many ns of simulated time it took.
The final runner done line must also print current tick and total simulated runtime in ns.
```

Do this in `vip_common::Runner`, not by copy-pasting boilerplate into every `tc_*.cpp`.

Reason:

- every project already registers cases through `Runner`
- one runner implementation handles all `tc_` files uniformly
- no testcase can forget to print its end line
- exceptions/future failure handling can be centralized later
- output format stays consistent across all VIP projects

After runner timing is implemented, remove manual top-level start/end lines from testcase bodies:

```cpp
vip::common::log_line("tc_basic", "INFO", "start");
...
vip::common::log_line("tc_basic", "INFO", "end");
```

These become redundant because runner prints:

```text
# [runner][INFO][tick=...] case N/M start: tc_basic
# [runner][INFO][tick=...] case N/M end: tc_basic delta_ticks=... delta_ns=...
```

### 7.2 Keep subcase logs, but let runner own case timing

Keep useful subcase logs such as:

```cpp
vip::common::log_line("tc_cfg", "INFO", "subcase cfg_enable");
```

Those are still valuable breadcrumbs inside long tests.

But top-level `tc_xxx start` and `tc_xxx end` should be removed or changed to avoid duplication.

If keeping them temporarily during migration, they must print `[tick=...]` through the fixed `vip::common::log_line()` and must not claim ns.

### 7.3 If a project does not use `vip_common::Runner`

For older projects without `Runner`, add a small helper and use it at the top/bottom of every testcase coroutine.

```cpp
inline test::sim_tick_t tc_start_log(test::TestBase& tb,
                                     const std::string& name) {
    const auto start = vip::common::sim_time_ticks();
    vip::common::log_line(name, "INFO", "start");
    return start;
}

inline void tc_end_log(test::TestBase& tb,
                       const std::string& name,
                       const test::sim_tick_t start) {
    const auto end = vip::common::sim_time_ticks();
    const auto delta = vip::common::delta_ticks(start, end);
    const auto delta_ns = vip::common::ticks_to_ns(tb, delta);
    vip::common::log_line(name,
                          "INFO",
                          "end delta_ticks=" + std::to_string(delta)
                          + " delta_ns=" + vip::common::format_ns(delta_ns));
}
```

Then:

```cpp
TestBase::RunUserTask tc_basic(Test& test) {
    const auto tc_start_tick = vip::common::tc_start_log(test, "tc_basic");

    ... testcase body ...

    vip::common::tc_end_log(test, "tc_basic", tc_start_tick);
    co_return;
}
```

Do this only for projects without the common runner. For `vip_uart_core`, runner-level timing is preferred.

---

## 8. `vip_uart_core`-specific file checklist

Apply these concrete edits to the attached current `vip_uart_core` tree.

### `ext/vip_common/common/logger.hpp`

- Rename `sim_time_ns()` to `sim_time_ticks()`.
- Add `INVALID_TICK`, `valid_tick()`, `ticks_to_ns()`, `delta_ticks()`, `delta_ticks_to_ns()`, `format_ns()`, and `tick_unit_label()` helpers.
- Change `log_line()` to print `[tick=...]`.

### `ext/vip_common/common/common.hpp/.cpp`

- Replace `delay_ns(double)` with `template <test::TimeUnit U> delay(test::delay_arg_t<U>)`.
- Use `tb_.getCoWrite()` in `write_barrier()`.
- Use `tb_.getCoRead()` in `waitFor()`.

### `ext/vip_common/runner/runner.hpp/.cpp`

- Rename `sim_time_ns_()` to `sim_time_ticks_()`.
- Print precision metadata at runner start.
- Print per-case start immediately before `co_await c.fn()`.
- Print per-case end immediately after `co_await c.fn()` with `delta_ticks` and `delta_ns`.
- Print final `runner done` with current tick and total `delta_ns`.

### `ext/vip_common/scoreboard/scoreboard.hpp/.cpp`

- Store ticks, not doubles.
- Rename all `time_ns` fields and parameters to `time_tick`.
- Use `INVALID_TICK` sentinel.
- Print `[tick=...]` on event lines.
- Include case summary delta if `start_tick/end_tick` are valid.

### `ext/vip_common/agents/clock/*`

- Replace `start(double period_ns)` with `start<unit>(period)`.
- Replace `set_period(double period_ns)` with `set_period<unit>(period)`.
- Store periods as raw ticks internally.
- Use `utils_.delay<test::ticks>(...)` inside the clock loop.
- Update call sites to `start<test::ns>(CLK_PERIOD_NS)`.

### `ext/vip_common/agents/por/*`

- Add no-arg `assert_reset()` and `deassert_reset()`.
- Add explicit-unit `assert_reset<unit>()`, `deassert_reset<unit>()`, and `pulse_reset<unit>()`.
- Replace internal `getCoWrite(0)` with `getCoWrite()`.
- Update call sites such as `deassert_reset(0.0)` to `deassert_reset()`.

### `ext/vip_uart/common/uart_params.hpp`

Rename:

```text
bit_ticks       -> bit_clks
sample_tick     -> sample_clk_index
idle_poll_ticks -> idle_poll_clks
frame_ticks()   -> frame_clks()
```

Update comments to say clock edges, not ticks.

### `ext/vip_uart/common/uart_types.hpp`

Rename:

```text
start_time_ns -> start_tick
end_time_ns   -> end_tick
```

Use `test::sim_tick_t`.

### `ext/vip_uart/agents/uart_tx/*`

Rename protocol clock-count fields/methods:

```text
inter_frame_gap_ticks      -> inter_frame_gap_clks
rts_wait_timeout_ticks     -> rts_wait_timeout_clks
set_inter_frame_gap_ticks  -> set_inter_frame_gap_clks
set_rts_wait_timeout_ticks -> set_rts_wait_timeout_clks
wait_ticks_                -> wait_clks_
```

Update RapidVPI calls:

```text
getCoWrite(0) -> getCoWrite()
getCoRead(0)  -> getCoRead()
delay_ns(x)   -> delay<test::ns>(x)
delay_ns(ps/1000.0) -> delay<test::ps>(ps)
```

Frame history:

```text
sent.start_time_ns -> sent.start_tick
sent.end_time_ns   -> sent.end_tick
```

### `ext/vip_uart/agents/uart_rx/*`

Rename:

```text
wait_ticks_ -> wait_clks_
params_.bit_ticks -> params_.bit_clks
params_.sample_tick -> params_.sample_clk_index
params_.idle_poll_ticks -> params_.idle_poll_clks
```

Update RapidVPI calls:

```text
getCoWrite(0) -> getCoWrite()
getCoRead(0)  -> getCoRead()
```

Prefer returning sample timestamps from `sample_line_()` and storing them as `frame.start_tick/end_tick`.

### `ext/vip_uart/scoreboard/uart_scb/*`

- Replace all `frame.end_time_ns` with `frame.end_tick`.
- Replace `double time_ns` rule helper parameters with `test::sim_tick_t time_tick`.
- Replace `waited_ticks` strings with `waited_clks` where the value is a clock count.

### `src/agents/uart_core_intf/*`

- Replace all `getCoWrite(0)` / `getCoRead(0)`.
- In `pop_rx_byte()`, use `rd.getTime<test::ticks>()` for `rec.time_tick`.
- Rename `UartCoreRxByte::time_ns` to `time_tick`.

### `src/scoreboard/scb_uart_core.*`

- Use `rec.time_tick` and `frame.end_tick`.
- No local scoreboard should pass a double ns value to `vip_common::Scoreboard`.

### `src/cases/tc_*.cpp`

- Replace any remaining `getCoRead(0)` with `getCoRead()`.
- Replace any remaining `getCoWrite(0)` with `getCoWrite()`.
- Replace parameter names/calls: `bit_ticks`, `frame_ticks()`, etc.
- Remove redundant top-level `start`/`end` log lines if runner now prints them.
- Keep subcase logs.

### `src/test.cpp`

Update construction:

```cpp
, scb(*this)
```

Update clock/POR calls indirectly through cases:

```cpp
co_await test.clock_agent.start<test::ns>(CLK_PERIOD_NS);
co_await test.por.assert_reset<test::ns>(CLK_PERIOD_NS * 4.0);
co_await test.por.deassert_reset();
```

Update before/after hooks:

```cpp
runner.set_before_case_hook([this](const vip::common::Runner::CaseDesc& c) {
    scb.reset_case();
    ...
    scb.start_case(c.name); // auto-stamps current tick
});

runner.set_after_case_hook([this](const vip::common::Runner::CaseDesc&) {
    ... checks ...
    scb.end_case(); // auto-stamps current tick
    scb.print_case_summary();
    scb.print_total_summary();
});
```

Runner will separately print testcase body start/end/delta.

---

## 9. Smart timestamp usage rules

Use timestamps intelligently to avoid needless VPI calls and needless conversions.

### Situation A: you just awaited a read

Use the awaiter's stored timestamp:

```cpp
auto rd = tb.getCoRead();
rd.read("rx_valid");
rd.read("rx_data");
co_await rd;

const auto tick = rd.getTime<test::ticks>();
```

This is best for observed DUT events. It avoids calling `vpi_get_time()` again.

### Situation B: you just awaited a change

Use the change awaiter's stored timestamp:

```cpp
auto chg = tb.getCoChange("irq", 1);
co_await chg;

const auto tick = chg.getTime<test::ticks>();
```

### Situation C: generic log line not tied to a read/change awaiter

Use `vip::common::sim_time_ticks()` through the common logger.

Examples:

```text
runner starting case
runner done
agent queue depth report
subcase breadcrumb
```

Use:

```cpp
vip::common::log_line("tc_cfg", "INFO", "subcase parity_mode");
```

Do not call raw VPI directly in agents.

### Situation D: scoreboard event storage

Store ticks:

```cpp
scb.observe_frame(port, frame, frame.end_tick);
```

Do not store converted ns.

### Situation E: final human-readable delta

Compute from raw ticks once, at the final reporting point:

```cpp
const auto delta = vip::common::delta_ticks(start_tick, end_tick);
const double delta_ns = vip::common::ticks_to_ns(tb, delta);
```

Print both when useful:

```text
delta_ticks=724000 delta_ns=724.000
```

### Situation F: one-off debug print from an awaiter

Use converted time only if the print explicitly labels the unit:

```cpp
const double t_ns = rd.getTime<test::ns>();
log << "[DBG][time_ns=" << t_ns << "] ..." << std::endl;
```

Do not pass this converted double into generic scoreboards.

---

## 10. Final grep checklist

After edits, these should be zero or only appear in documentation explaining old APIs.

```bash
grep -R "getCoWrite(0\|getCoRead(0" -n src ext --exclude-dir=.git

grep -R "getCoWrite([0-9.]\|getCoRead([0-9.]" -n src ext --exclude-dir=.git

grep -R "\.getTime()\|getTime()" -n src ext --exclude-dir=.git

grep -R "sim_time_ns\|start_time_ns\|end_time_ns\|\btime_ns\b" -n src ext --exclude-dir=.git

grep -R "delay_ns" -n src ext --exclude-dir=.git
```

For UART-like projects, these should also be zero after clock-count rename:

```bash
grep -R "bit_ticks\|sample_tick\|idle_poll_ticks\|frame_ticks\|inter_frame_gap_ticks\|rts_wait_timeout_ticks" -n src ext --exclude-dir=.git
```

Allowed exceptions:

```text
CLK_PERIOD_NS       physical constant, OK
bit_time_ns         physical bit time for baud-derived delay, OK if used with <ns>
phase_offset_ps     physical phase offset, OK if used with <ps>
```

Build checks:

```bash
rm -rf cmake-build-release
cmake -S . -B cmake-build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release -j"$(nproc)"
```

If the project builds against an installed RapidVPI, make sure the installed RapidVPI is the new refactored version. Old RapidVPI will not have `getCoWrite()` no-arg or `<ticks>` support.

---

## 11. Expected output style

Desired output style after rework:

```text
# [runner][INFO][tick=0] vpi_precision_exp10=-12 tick_unit=1ps
# [runner][INFO][tick=0] running 8 case(s)
# [runner][INFO][tick=0] case 1/8 start: tc_basic
# [tc_utils][INFO][tick=0] local reset start
# [tc_utils][INFO][tick=80000] local reset done
[PASS][tick=250000] uart_core observed RX byte-side data=0x55 frame_error=0 parity_error=0 break=0
# [runner][INFO][tick=724000] case 1/8 end: tc_basic delta_ticks=724000 delta_ns=724.000
[SCB][CASE] 'tc_basic' info=2 pass=14 warn=0 fail=0 start_tick=0 end_tick=724000 delta_ticks=724000 delta_ns=724.000
[SCB][TOTAL] cases_run=1 passed=1 failed=0 warn_events=0 fail_events=0
# [runner][INFO][tick=9134000] runner done. total_delta_ticks=9134000 total_delta_ns=9134.000
```

Key points:

- every timestamp label says `tick=` if it is raw simulator tick
- every converted nanosecond value says `delta_ns=` or `time_ns=`
- case start/end are automatically printed by `Runner`
- final runner line includes current tick and total simulated runtime in ns

---

## 12. Anti-patterns to eliminate

Do not leave these behind:

```cpp
auto wr = tb.getCoWrite(0);
auto rd = tb.getCoRead(0);
co_await tb.getCoWrite(10.0);
co_await utils.delay_ns(10.0);
const double t = rd.getTime();
const double t = static_cast<double>(vip::common::sim_time_ns());
scb.note_fail(msg, frame.end_time_ns);
double time_ns = -1.0;
```

Do not write protocol-local counters like this when they mean clock edges:

```cpp
unsigned bit_ticks;
unsigned idle_poll_ticks;
```

Use:

```cpp
unsigned bit_clks;
unsigned idle_poll_clks;
```

Do not scatter direct VPI calls in agents:

```cpp
s_vpi_time t{};
vpi_get_time(nullptr, &t);
```

Only `vip_common` should own generic current-time helpers. If an agent already has an `AwaitRead` or `AwaitChange`, use `getTime<test::ticks>()` from that awaiter.

Do not convert raw ticks to ns manually in random files:

```cpp
const double ns = ticks / 1000.0; // wrong unless tick is known 1ps
```

Use:

```cpp
const double ns = vip::common::ticks_to_ns(tb, ticks);
```

or if using an awaiter directly:

```cpp
const double ns = rd.getTime<test::ns>();
```

---

## Final implementation order

Use this order for Codex or any automated agent:

1. Update `vip_common` logger/time helpers.
2. Update `vip_common` scoreboard to raw ticks.
3. Update `vip_common` runner to print per-case start/end/delta and final runner runtime.
4. Update `CommonUtils` to explicit-unit `delay<unit>()` and no-arg write/read phase usage.
5. Update `Clock` and `Por` APIs.
6. Update reusable protocol VIPs under `ext/vip_xxx`.
7. Rename protocol-local `*_ticks` names that mean cycles/clocks.
8. Update project-local DUT agents and scoreboards.
9. Update testcase source files and remove duplicate manual top-level start/end logs.
10. Run grep checklist until stale API patterns are gone.
11. Build.

After this rework, `vip_uart_core` should become the reference project shape for updating other existing `vip_xxx` projects.
