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

# vip_common — TB core (RapidVPI + C++ coroutines)

`vip_common` is the **shared testbench core** used by all VIP modules and by your project’s `src/` layer.
It provides:

- **VPI-backed logging** (`vip::common::SimLogger`, `vip::common::log_line()`)
- **Scoreboard aggregation** (`vip::common::Scoreboard`) — unified PASS/WARN/FAIL counts + summaries
- **Case runner/orchestrator** (`vip::common::Runner`) — case registry, plans, tag selection, hooks
- **Generic agents**: free-running **clock** (`vip::common::Clock`) and test-driven **POR/reset** (`vip::common::Por`)
- **Small coroutine utilities** (`vip::common::CommonUtils`) — `waitFor()`, `clock()`, phase-safe barriers

`vip_common` is designed to be a **future git submodule**:
- It must not include or depend on your project’s `src/*`.
- It depends only on RapidVPI (`test::TestBase`) and standard C++.

---

## Table of contents
- [1. Quick start](#1-quick-start)
  - [1.1 Minimal wiring in your Test](#11-minimal-wiring-in-your-test)
  - [1.2 Case registration + plans](#12-case-registration--plans)
  - [1.3 Runner hooks](#13-runner-hooks)
- [2. User objects you will touch](#2-user-objects-you-will-touch)
  - [2.1 SimLogger and log_line](#21-simlogger-and-log_line)
  - [2.2 Scoreboard](#22-scoreboard)
  - [2.3 Runner](#23-runner)
  - [2.4 Clock agent](#24-clock-agent)
  - [2.5 POR/reset helper](#25-porreset-helper)
  - [2.6 CommonUtils](#26-commonutils)
- [3. Coroutine discipline](#3-coroutine-discipline)
- [4. Notes for project-specific extensions](#4-notes-for-project-specific-extensions)

---

## 1. Quick start

### 1.1 Minimal wiring in your Test

Your project typically defines a `Test` that derives from RapidVPI `test::TestBase`.
`vip_common` aliases that type as `vip::common::TestBase`.

A typical project layer (`src/test.hpp` / `src/test.cpp`) will own:

- `vip::common::Scoreboard scb;`
- `vip::common::Runner runner;`
- optional infra agents `vip::common::Clock`, `vip::common::Por`

Example (shape only):

```cpp
#include "vip_common/scoreboard/scoreboard.hpp"
#include "vip_common/runner/runner.hpp"
#include "vip_common/runner/case_helpers.hpp"
#include "vip_common/agents/clock/clock.hpp"
#include "vip_common/agents/por/por.hpp"

namespace test {

struct Test : public vip::common::TestBase {
    vip::common::Scoreboard scb;
    vip::common::Runner     runner;

    vip::common::Por   por;
    vip::common::Clock clk;

    Test()
      : scb(*this)
      , runner(*this)
      , por(*this, /*rst_net=*/"rst_n", /*active_low=*/true)
      , clk(*this, /*net_name=*/"clk", /*task_name=*/"clk_run")
    {
        runner.register_tasks();

        // Optional: start/stop clocks and reset from cases.
    }
};

} // namespace test
```

### 1.2 Case registration + plans

`vip::common::Runner` owns a registry of testcases (`CaseDesc`), each case being a **coroutine factory**:

- `CaseDesc.name` — unique case name
- `CaseDesc.tags` — freeform tags used for selection
- `CaseDesc.enabled_by_default` — runs when no explicit plan is set
- `CaseDesc.fn` — `RunUserTask()` coroutine factory

You can either register cases with `CaseDesc` directly, or use the helper `vip::common::add_case()`.

Example registration:

```cpp
vip::common::add_case(
    runner,
    "tc_axis_tx_gmii_basic",
    {"smoke", "gmii", "tx"},
    true,
    [this]() -> vip::common::TestBase::RunUserTask { return tc_axis_tx_gmii_basic(*this); });
```

Plan selection:

- Explicit sequential plan:
  ```cpp
  runner.set_plan({"tc_generic", "tc_axis_tx_gmii_basic"});
  ```

- Tag-based plan (selects cases that match **any** requested tag, in registration order):
  ```cpp
  runner.set_plan_tagged({"mandatory", "baseline"});
  ```

- Clear the plan (falls back to “all enabled_by_default in registration order”):
  ```cpp
  runner.clear_plan();
  ```

Dependencies:

```cpp
runner.add_after("tc_bringup", "tc_por");
// Meaning: tc_por must run before tc_bringup.
```

### 1.3 Runner hooks

Hooks allow project-owned lifecycle wiring without coupling `vip_common` to project code:

- `set_before_case_hook(...)`
- `set_after_case_hook(...)`
- `set_after_all_hook(...)`  (run-level cleanup)

Example: close an AXIS trace at end-of-run:

```cpp
runner.set_after_all_hook([this]() {
    axis_mst.trace_close();
});
```

---

## 2. User objects you will touch

### 2.1 SimLogger and log_line

Header: `vip_common/common/logger.hpp`

- `vip::common::SimLogger` is a minimal **VPI-backed** stream-like logger.
- `vip::common::log_line(src, level, msg)` prints in a stable format with a raw simulator tick:

```text
# [source][level][tick=12345] message...
```

Example:

```cpp
vip::common::log_line("tb", "INFO", "hello");

vip::common::SimLogger log;
log << "raw line" << std::endl;
```

### 2.2 Scoreboard

Header: `vip_common/scoreboard/scoreboard.hpp`

`vip::common::Scoreboard` is the **global event sink**:
- provides `note_info/pass/warn/fail()`
- tracks per-case and total counts
- prints summaries (`print_case_summary()`, `print_total_summary()`)

Typical usage in cases/VIPs:

```cpp
test.scb.note_fail("GMII TX bytes mismatch");
```

Print policy:
- default is **WARN + FAIL only**
- you can enable more:
  ```cpp
  test.scb.set_print_all();
  // or:
  test.scb.enable_print_info(true);
  ```

Case lifecycle is usually driven by your harness/runner (project layer):
- `scb.start_case(name)`
- `scb.end_case()`

(Your current template calls these from the project glue around `Runner`.)

### 2.3 Runner

Header: `vip_common/runner/runner.hpp`

`vip::common::Runner` is the case orchestrator:
- registers a top-level RapidVPI task named **`case_runner`** (`register_tasks()`)
- runs selected cases sequentially
- supports explicit plans, tag selection, and dependencies
- supports hooks

Selection summary:
- if `plan_` is set: run those names (plus prerequisites)
- else: run all `enabled_by_default` cases

Tag selection rule (current implementation):
- `set_plan_tagged({A,B})` selects cases that contain **any** of the tags `{A,B}`.

### 2.4 Clock agent

Header: `vip_common/agents/clock/clock.hpp`

`vip::common::Clock` supports two explicitly selected backends while preserving
the same testcase API:

- `LegacyVpi`: the original three-argument constructor; the Clock task generates
  every waveform edge through RapidVPI and parks `net_name` low when stopped.
- `NativeHdl`: the four-argument constructor takes `Clock::NativeClockCfg` with
  arbitrary `enable_net`, `period_ticks_net`, and `stopped_net` names. It writes
  only enable/full-period controls and observes stopped status; the HDL wrapper
  alone generates the actual `net_name` waveform.

Projects may construct any number of independent native clocks. `net_name`
remains the actual clock used by agents for edge synchronization and observation
even though the native backend never writes it.

```cpp
vip::common::Clock legacy(tb, "clk", "clk_run");
vip::common::Clock native(
    tb, "clk_x", "clk_x_run",
    vip::common::Clock::NativeClockCfg{
        .enable_net = "clk_x_enable",
        .period_ticks_net = "clk_x_period_ticks",
        .stopped_net = "clk_x_stopped",
    });
```

#### 2.4.1 Common controls

The existing controls retain their request-oriented behavior:

- `start<unit>(period)` requests ordinary free-running operation and returns without waiting for the first edge.
- `stop()` requests a stop and returns before the selected backend necessarily reports the net parked low.
- `set_period<unit>(period)` updates the requested full period without stop/restart rephasing.
- `is_running()` reports the requested state. It does not report whether the clock is physically running.

Example:

```cpp
co_await clk.start<test::ns>(10.0);     // 100 MHz
co_await clk.set_period<test::ns>(8.0); // 125 MHz
co_await clk.stop();
```

The period remains clamped to at least two simulator ticks. Both backends use
`period_ticks / 2` for its high interval and assigns any odd extra tick to the low
interval.

#### 2.4.2 Deterministic scheduled start

Use an explicit scheduled start when the first rising edge is part of the
verification contract:

```cpp
co_await clk.start_after<test::ns>(8.0, 20.0); // first rise 20 ns after this call

const auto first = vip::common::sim_time_ticks() + 100u;
co_await clk.start_at<test::ns>(8.0, first);    // first rise at absolute tick first
```

`start_after(period, delay)` and `start_at(period, first_rise_tick)` arm a stopped
Clock and return without waiting for the edge. The Clock remains low until the
target, produces a rising edge at that exact simulator tick, and then uses the
selected backend's free-running period and duty-cycle behavior. In native mode
the control task enables the HDL generator at the target; it never toggles the
actual clock net. Durations are quantized by
the existing `duration_to_ticks()` conversion.

The target must be valid, must be at least one stopped polling interval (currently
10 simulator ticks) in the future when armed, and must not overflow the simulator
tick type. A scheduled start is rejected with a `vip_common::Clock` error if the
Clock is physically running or another scheduled start is pending. It never
silently starts late.

While a scheduled start is armed, `is_running()` is normally true because it
reports the requested state even though the first physical edge has not occurred.
`set_period()` during this interval changes the period used after start without
moving the target tick. `stop()` cancels the pending start, and legacy `start()`
cancels it and restores ordinary legacy-start behavior.

An existing `CommonUtils::delay()` followed by legacy `start()` remains useful
when a test only needs non-coincident clocks. It does not guarantee an exact first
edge because the stopped Clock task consumes legacy start requests on its polling
cadence.

#### 2.4.3 Safe rephasing

`wait_stopped()` does not request a stop. Legacy mode waits until its background
task has completed the Clock-owned park-low write; native mode waits for the
configured HDL `stopped_net` bit to assert. `stop_and_wait()` requests stop,
cancels a pending scheduled start, and waits for the corresponding deterministic
park-low status.

Use `stop_and_wait()` before deterministically rephasing a running clock:

```cpp
co_await clk.stop_and_wait();
co_await clk.start_after<test::ns>(8.0, 2.0);
```

Legacy `stop()` intentionally remains request-only and is not a deterministic
rephase barrier.

#### 2.4.4 Exact relative phase between multiple clocks

Arm independent stopped clocks against one common absolute anchor:

```cpp
const auto now = vip::common::sim_time_ticks();
const auto lead = vip::common::duration_to_ticks<test::ns>(test, 20.0);
const auto offset = vip::common::duration_to_ticks<test::ns>(test, 2.0);
const auto anchor = now + lead;

co_await clk_a.start_at<test::ns>(8.0, anchor);
co_await clk_b.start_at<test::ns>(8.0, anchor + offset);
```

The first rising edges are separated by exactly `offset` simulator ticks, after
which each Clock free-runs at its own requested period.

#### 2.4.5 Scheduling constraints and misuse

Scheduled starts are only for clocks that are physically stopped; they do not
phase-shift a running clock. Issue control calls from one logical controller per
Clock instance rather than relying on same-tick arbitration between concurrent
coroutines. A reference obtained from a read-only observation should be used to
choose a positive future target; the Clock agent performs the actual signal write
through its normal write-phase mechanism.

### 2.5 POR/reset helper

Header: `vip_common/agents/por/por.hpp`

`vip::common::Por` is test-driven reset control:
- no autonomous task
- writes reset only when cases call its methods

Example:

```cpp
co_await por.pulse_reset<test::ns>(50.0, 50.0);
```

Supports active-low and active-high via constructor:

```cpp
vip::common::Por por(*this, "rst_n", /*active_low=*/true);
vip::common::Por por2(*this, "rst",  /*active_low=*/false);
```

### 2.6 CommonUtils

Header: `vip_common/common/common.hpp`

`vip::common::CommonUtils` is a small set of coroutine helpers:
- `waitFor(net, val)` — waits until net equals value (returns immediately if already equal)
- `clock(n, edge)` — waits `n` clock edges on a configured default clock net
- `delay<unit>(t)` — write-phase delay with an explicit RapidVPI time unit

Phase-safe helpers:
- `write_barrier()`
- `clock_to_write()`

These exist to make it harder to accidentally violate the project’s RO/WO scheduling discipline.

---

## 3. Coroutine discipline

This project follows a strict RapidVPI/VVP phase rule:

- After any **read-only await** (`getCoRead`, `getCoChange`, `clock()`), **do not schedule writes in the same time slot**.
- Use edge-sampled loops (read → await → clock), then perform writes on the next cycle using a fresh write phase.

All agents in `vip_common` follow this rule internally. When writing your own cases:
- prefer `waitFor()` when you need “wait until value” (does not require a change)
- avoid equality checks on wide buses; prefer masks where possible

---

## 4. Notes for project-specific extensions

It’s normal for a master project to add custom, non-reusable pieces under `src/`:

- `src/agents/` — project-specific agents
- `src/scoreboard/` — project-specific checkers/scoreboards

Those modules should report into the global `vip::common::Scoreboard` via `note_*()` so totals/summaries remain unified.

If a project-specific module needs lifecycle, wire it via `Runner` hooks (before/after case, after all).
