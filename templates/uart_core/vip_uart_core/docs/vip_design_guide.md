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

# RapidVPI reusable VIP design guide

> This is the normative architecture for reusable RapidVPI/C++ coroutine VIP. Its scheduling
> rules are qualified against event-driven and compiled simulators. Simulator callback ordering
> is not a protocol contract. The single-writer, single-completion, setup-sample/paired-edge,
> bounded-progress, and non-vacuity rules apply to every protocol; AXI-family interfaces are
> detailed applications, not exceptions.

Use this guide without inspecting installed RapidVPI headers or copying another VIP repository.
Existing `vip_axi`, `vip_axil`, `vip_axis`, `vip_mdio`, `vip_eth`, and `vip_uart` repositories are
examples only; this file is the policy source.

---

## Contents

1. [Scope and mandatory architecture](#1-scope-and-mandatory-architecture)
2. [RapidVPI API, time, and phase primitives](#2-rapidvpi-api-time-and-phase-primitives)
3. [Repository, build, naming, parameters, and vectors](#3-repository-build-naming-parameters-and-vectors)
4. [Project clock architecture](#4-project-clock-architecture)
5. [Agent and signal ownership](#5-agent-and-signal-ownership)
6. [Phase, sampling, handshake, and retirement](#6-phase-sampling-handshake-and-retirement)
7. [Protocol profiles](#7-protocol-profiles)
8. [Scoreboards and event authority](#8-scoreboards-and-event-authority)
9. [Testcase construction and non-vacuity](#9-testcase-construction-and-non-vacuity)
10. [Reset, case isolation, and runner lifecycle](#10-reset-case-isolation-and-runner-lifecycle)
11. [Configuration, stress, models, logging, and tracing](#11-configuration-stress-models-logging-and-tracing)
12. [Cross-simulator qualification](#12-cross-simulator-qualification)
13. [Documentation and creation checklist](#13-documentation-and-creation-checklist)
14. [Prohibited patterns](#14-prohibited-patterns)

---

## 1. Scope and mandatory architecture

### 1.1 Package boundary

A reusable `vip_xxx` is protocol-level verification infrastructure installed normally as:

```text
<project>/ext/vip_xxx
```

It normally depends on `<project>/ext/vip_common` and may contain:

- protocol agents that hide pin-level mechanics;
- protocol parameters, transaction types, and optional data models;
- functional and rules scoreboards;
- optional passive monitors, trace/history, and debug helpers;
- documentation and backward-compatible extension points.

It must not contain:

- one DUT's address/register/command layout unless explicitly configurable and protocol-generic;
- project testcase intent that belongs in `src/cases`;
- hardcoded top paths beyond a documented prefix/port-map convention;
- another copy of `vip_common`;
- simulator launch policy;
- raw VPI callback registration unless intentionally adding a low-level RapidVPI primitive.

Protocol-generic examples include valid-ready mechanics, AXI burst counting, UART framing, MDIO
serialization, and AXIS packet/TLAST handling. DUT-specific routing, status folding, address maps,
wrapper debug fields, and command encodings remain project-local.

### 1.2 Non-negotiable contract

| Rule | Required meaning |
| --- | --- |
| Tests express intent | Testcases configure, enqueue, wait with bounds, and check; agents perform protocol mechanics. |
| One physical writer | Every DUT-visible VIP output has exactly one writer at a time, including reset/idle handling. |
| One completion owner | One worker owns accepted-item counting, terminal recognition, response capture, and ticket retirement. |
| One expectation and terminal observation | A logical transaction creates one functional expectation and one closing functional observation. |
| RO causality | A read/change/clock observation cannot authorize a write that is claimed to affect the already sampled slot. |
| Paired-edge classification | Capture candidate state in its setup phase, cross the qualified active edge, then classify from the capture. |
| First legal retirement | Replace or retire accepted source state in the first legal post-edge write phase; do not add an unexplained barrier. |
| Bounded progress | Every behavior-dependent testcase wait and cleanup acknowledgement has an explicit derived budget. |
| Construct concurrency before waiting | Configure and arm observation, enqueue every required contender, then perform the first behavior-dependent await. |
| Non-vacuity | Directed tests fail unless their named mechanism demonstrably activates. |
| Event-relative timing | Timing hooks and exact-cycle tests anchor to protocol events, never enqueue/coroutine wake time. |
| Semantic qualification | Questa and Verilator must agree on activation witnesses and normalized protocol-event ledgers. |

These rules are architectural, not optional debugging preferences.

### 1.3 Responsibility boundaries

**Agents own:** queues, tickets, DUT-visible outputs assigned to them, accepted-event mechanics,
response capture/generation, terminal retirement, optional expected/observed scoreboard calls, and
protocol timestamps.

**Scoreboards own:** reusable comparison policy, protocol legality, outstanding expectations, and
end-of-case checking. Agents may forward events; random testcase code must not duplicate generic
checking policy.

**Tests own:** scenario selection, configuration, command enqueue, bounded orchestration,
non-vacuity requirements, and DUT-specific final checks. Tests never become a second raw driver or
completion observer when an agent exists.

**Monitors own only what is declared:** a passive monitor may be the sole functional observer, or
may provide history/trace/rules data beside an active agent. It must never duplicate an event side
effect already owned by the active agent.

### 1.4 Stable extension policy

For a mature VIP:

- keep established constructors and method names working;
- prefer additive config structs, setters, and new enum modes to signature churn;
- keep new strict checks opt-in unless all established users require them;
- reset new per-case state in `reset_case()`;
- document changed defaults;
- use typed runtime configuration so one compiled VIP can cover several DUT configurations.

Deliberate early breaking cleanup is acceptable when establishing a policy source. Do not preserve
misleading names such as `time_ns` for raw ticks.

Simulator portability is correctness. Never repair scheduling with simulator-name branches,
arbitrary zero-time barriers, tiny offsets, host sleeps, or weakened expectations. Fix the actual
writer, phase, monitor, or completion owner.

---

## 2. RapidVPI API, time, and phase primitives

### 2.1 Public include, namespace, task types, and units

```cpp
#include "testbase.hpp"

class Test : public test::TestBase {
public:
    using RunTask = test::TestBase::RunTask;
    using RunUserTask = test::TestBase::RunUserTask;
    void initNets() override;
};
```

Public RapidVPI testbase symbols are under `test`. In public VIP headers, prefer explicit
`test::TestBase`, `test::sim_tick_t`, and unit names; do not export `using namespace test;`.

```cpp
namespace test {
using sim_tick_t = std::uint64_t;
enum class TimeUnit { ticks, ps, ns, us, ms };
inline constexpr auto ticks = TimeUnit::ticks;
inline constexpr auto ps = TimeUnit::ps;
inline constexpr auto ns = TimeUnit::ns;
inline constexpr auto us = TimeUnit::us;
inline constexpr auto ms = TimeUnit::ms;
}
```

`ticks` means raw VPI simulator ticks at the effective simulator precision; it never inherently
means nanoseconds. `ps/ns/us/ms` are explicit converted units.

`RunTask` starts immediately and is used for top-level testcase and long-lived agent coroutines.
`RunUserTask` starts suspended and is used for awaited reset, polling, protocol sub-step, and
cleanup helpers. Do not manually destroy coroutine handles.

### 2.2 Test object and net registration

`TestBase` requires `initNets()`:

```cpp
void Test::initNets() {
    addNet("clk", 1);
    addNet("rst_n", 1);
    addNet("s_axis_tvalid", 1);
    addNet("s_axis_tready", 1);
    addNet("s_axis_tdata", 32);
}
```

| API | Contract |
| --- | --- |
| `setDutName(name)` | Called by project setup before `initNets()`; establishes path prefix. |
| `addNet(key, width)` | Resolves `dutName + "." + key`; width controls vector packing. |
| `getNetHandle(key)` | Returns registered VPI handle. |
| `getNetLength(key)` | Returns registered bit width. |
| `vpiTimePrecisionExp10()` | Effective VPI precision metadata. |
| `vpiTickPeriodSeconds()` | Raw tick duration metadata. |

Use stable short keys such as `clk`, `rst_n`, and `m0_awvalid`. Missing/misspelled nets cause
failed reads or callbacks. Precision metadata is for startup/debug validation, not VIP-local
conversion logic.

### 2.3 Write awaiter

Factories:

```cpp
TestBase::AwaitWrite getCoWrite();
template <test::TimeUnit U>
TestBase::AwaitWrite getCoWrite(test::delay_arg_t<U> delay);
```

Methods:

```cpp
write(net, numeric_value);
write(net, string_value, base = 2);
force(net, numeric_or_string_value, base = 2);
release(net);
setDelay();
setDelay<U>(delay);
```

Canonical forms:

```cpp
auto wr = test.getCoWrite();
wr.write("valid", 1);
wr.write("data", 0x55);
co_await wr;

auto delayed = test.getCoWrite<test::ns>(10.0);
delayed.write("valid", 0);
co_await delayed;

auto raw = test.getCoWrite<test::ticks>(10000);
raw.write("valid", 1);
co_await raw;
```

Queue every write before `co_await`. Actual `vpi_put_value()` calls occur when the awaiter resumes.
Do not queue two values for the same net in one awaiter. Numeric access is limited to 64-bit nets;
use binary/hex strings for wider or four-state values. Base defaults to binary; pass `16` for hex.
`force()`/`release()` use VPI force/release. Prefer delay at factory construction; `setDelay()` is
available but uncommon.

Do not use obsolete implicit forms:

```cpp
// Wrong with the current API:
// getCoWrite(0)
// getCoWrite(10.0)
```

### 2.4 Read awaiter

Factories and methods:

```cpp
TestBase::AwaitRead getCoRead();
template <test::TimeUnit U>
TestBase::AwaitRead getCoRead(test::delay_arg_t<U> delay);

read(net);
getNum(net);
getBinStr(net);
getHexStr(net);
getTime<U>();
setDelay();
setDelay<U>(delay);
```

```cpp
auto rd = test.getCoRead();
rd.read("ready");
rd.read("data");
co_await rd;

const bool ready = (rd.getNum("ready") & 1ULL) != 0ULL;
const auto data = rd.getNum("data");
const test::sim_tick_t tick = rd.getTime<test::ticks>();
```

Call `read()` before the await and getters after it. `getNum()` is for at most 64 bits and consumes
stored numeric chunks; do not call it repeatedly for one sampled value. Use `getBinStr()` or
`getHexStr()` for wide buses. Binary output may be padded to a 32-bit chunk boundary; hex output
trims leading zeros. Convert raw protocol values into typed protocol fields inside the agent,
instead of making testcases slice strings.

Delayed forms require explicit units: `getCoRead<test::ns>(10.0)` or
`getCoRead<test::ticks>(10000)`. Do not use `getCoRead(0)` or an explicit zero-unit delay merely to
mean the next read phase; use `getCoRead()`.

### 2.5 Change awaiter and safe polling

```cpp
auto any = test.getCoChange("irq");
co_await any;
const bool irq = (any.getNum() & 1ULL) != 0ULL;

auto targeted = test.getCoChange("done", 1);
co_await targeted;
```

`AwaitChange` provides `getNum()`, `getBinStr()`, `getHexStr()`, and `getTime<U>()`. Use a targeted
change mainly for scalar/small status. Do not use bus-equality waits for multi-field state and do
not count handshakes from VALID/READY changes; a handshake is an edge event.

If a read/change result may lead to a later write, use the conservative edge-sampled pattern:

```cpp
while (true) {
    auto rd = test.getCoRead();
    rd.read("status");
    co_await rd;
    const auto status = rd.getNum("status");

    // Cross the required future edge even on the successful iteration.
    co_await clock_once(test);

    if ((status & DONE_MASK) != 0ULL) {
        break;
    }
}

auto wr = test.getCoWrite();
wr.write("command", 1);
co_await wr;
```

The control-path default is:

```text
read net -> await RO -> cross required future edge -> bit-test -> fresh legal writer
```

A purely passive monitor may have a protocol-specific observation cadence because it never writes;
do not reuse that exception in a control/driver path.

### 2.6 Value conversion

```cpp
TestBase::bin_to_hex_char(bin);
TestBase::bin_to_hex(bin_string);
TestBase::hex_to_bin(hex_string);
```

`hex_to_bin()` accepts `0-9`, `a-f`, `A-F`, `x/X`, and `z/Z`; X and Z expand to four identical
binary state characters. `bin_to_hex()` supports X/Z; a mixed-state nibble without an exact single
hex representation becomes X. Preserve binary strings for exact mixed four-state patterns.

### 2.7 Time and logging policy

`getTime<ticks>()` returns exact `test::sim_tick_t`; `getTime<ps/ns/us/ms>()` returns `double`.
Store, compare, trace, and pass event time as raw ticks:

```cpp
test::sim_tick_t start_tick = rd.getTime<test::ticks>();
```

Use converted units only for human display or an explicitly physical-unit API:

```cpp
const double t_ns = rd.getTime<test::ns>();
std::printf("[DBG][tick=%llu][time_ns=%.3f] event\n",
            static_cast<unsigned long long>(rd.getTime<test::ticks>()), t_ns);
```

Rules:

- never name raw ticks `time_ns` or divide ticks by an assumed constant;
- use `<ns/us/...>` for intentional human-unit delays and `<ticks>` only for simulator-native
  delays;
- use no-argument read/write factories for zero-delay phase transitions;
- scoreboards, histories, CSV, and traces use raw ticks; convert only when printing and print the
  unit;
- normal logs prefer `[tick=123456]`; print precision metadata once if useful;
- outside an awaiter, use one centralized `vip_common::sim_time_ticks()`-style helper;
- never call internal `test::detail::current_vpi_time_ticks()` or scatter `vpi_get_time()`;
- RapidVPI reads but does not override simulator precision.

Precision examples: exponent `-12` is 1 ps/tick, `-9` is 1 ns/tick, and `-6` is 1 us/tick.
When an event timestamp is optional, use a typed sentinel such as
`std::numeric_limits<test::sim_tick_t>::max()`, not `double time_ns = -1.0`.
The simulator/HDL launch configuration owns effective resolution through controls such as Questa
`-t` and HDL `timeunit/timeprecision`; RapidVPI only reads that result.

### 2.8 Registration and completion of simulation

```cpp
extern "C" void userRegisterFactory() {
    core::registerTestFactory([]() -> std::unique_ptr<test::TestBase> {
        auto t = std::make_unique<Test>();
        t->setDutName("tb.dut");
        return t;
    });
}
```

Test registration ultimately supplies a `RunTask` coroutine handle, directly or through the
project runner. The normal shared library retains the complete registered testcase set.

```cpp
registerTest("tc_smoke", [this]() {
    return tc_smoke().handle;
});
```

After the complete selected plan and final cleanup, project `src/test.cpp` calls once:

```cpp
runner.set_after_all_hook([this]() {
    trace_close();
    core::finishSimulation();
});
```

Do not call raw `vpi_control(vpiFinish, ...)`, finish from a testcase/agent/`after_case`, or hardwire
finish into the generic Runner. Simulator GUI/batch finish policy belongs to launch configuration.
For example, Questa `-onfinish stop` may leave an interactive GUI available while batch mode exits
through its normal path. `core::finishSimulation()` ends simulation; it does not prove PASS.

A zero simulator, CMake, or launcher exit status proves only that execution terminated normally
unless the project explicitly propagates scoreboard status. Every regression flow must consume one
machine-readable final runner/scoreboard result and classify any non-zero failed-case or fail-event
count as failure. Prefer propagating that result to a non-zero process status when the surrounding
flow supports it; otherwise the regression driver must parse the final status explicitly. Never let
`Built target ...`, `$finish`, or exit code zero override `[SCB][TOTAL] ... failed>0`.

---

## 3. Repository, build, naming, parameters, and vectors

### 3.1 Default shape

Match the protocol; do not create empty layers merely to match this tree:

```text
vip_xxx/
  CMakeLists.txt
  README.md
  AGENTS.md                         optional local rules
  docs/                             optional notes/user guide
  common/
    xxx_params.hpp
    xxx_types.hpp
    xxx_bitvec.hpp                  only when needed
    xxx_vpi_utils.hpp               optional
    xxx_trace.hpp/.cpp              optional
  agents/
    xxx_master/                     or source/tx/controller
      master.hpp
      master_agent.cpp
      master_wr.cpp / master_rd.cpp
      mst_coroutines_wr.cpp / mst_coroutines_rd.cpp
    xxx_slave/                      or sink/rx/responder
      slave.hpp
      slave_agent.cpp
      slave_wr.cpp / slave_rd.cpp
      slv_coroutines_wr.cpp / slv_coroutines_rd.cpp
      slv_checks_wr.cpp / slv_checks_rd.cpp   optional
    xxx_monitor/                    optional passive agent
  scoreboard/xxx_scb/
    scb_xxx_tx.hpp/.cpp
    scb_xxx_rx.hpp/.cpp
    scb_xxx_rules.hpp/.cpp
```

For a simple serial interface, use `uart_tx`, `uart_rx`, and `uart_scb`. For packet protocols use
`packet_source`, `packet_sink`, and `packet_monitor`. Use established master/slave terms consistently
where they are the protocol's standard notation.

### 3.2 CMake contract

Root CMake adds compact subdirectories. Each subdirectory contributes a uniquely named PIC object
library to the parent `${PROJECT_NAME}` shared library:

```cmake
add_subdirectory(common)
add_subdirectory(agents/xxx_master)
add_subdirectory(agents/xxx_slave)
add_subdirectory(scoreboard/xxx_scb)

set(TEST_SUBFOLDER vip_xxx_master)
add_library(${TEST_SUBFOLDER} OBJECT master_agent.cpp master_wr.cpp master_rd.cpp)
target_include_directories(${TEST_SUBFOLDER} PRIVATE
    ${CMAKE_SOURCE_DIR}/ext
    /usr/local/include/rapidvpi/core
    /usr/local/include/rapidvpi/scheduler
    /usr/local/include/rapidvpi/testbase
    /usr/local/include/rapidvpi/testmanager
    ${vpi_include_dir})
set_property(TARGET ${TEST_SUBFOLDER} PROPERTY POSITION_INDEPENDENT_CODE ON)
target_sources(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:${TEST_SUBFOLDER}>)
```

Do not rebuild `vip_common`, create nested submodules, create unrelated executables, or put
simulator launch targets in a reusable VIP. Include through `${CMAKE_SOURCE_DIR}/ext`; keep target
names globally unique and CMake predictable.

Backend-specific project plugin build trees are independent artifacts. A project-level simulator
launcher that loads a shared VIP plugin must print the exact loaded `.so` path and must either depend
on rebuilding that exact target or document a mandatory pre-build step. Building or relinking the
RTL simulator executable does not imply that a sibling VIP plugin was rebuilt, and building
`cmake-build-release/libvip_xxx.so` does not update
`cmake-build-verilator/libvip_xxx.so`. After any VIP source change, rebuild the exact artifact named
by the launch log before qualification. During stale-artifact triage, compare source/object/plugin
timestamps or search the loaded binary for a mechanically searchable changed diagnostic; do not
infer freshness from a newly executed simulation.

### 3.3 Namespaces and public headers

Use one namespace such as `vip::uart` or `vip::axi`. Use explicit class names (`UartTx`, `AxiMaster`,
`ScbAxiRead`). Include public headers from the ext root:

```cpp
#include "vip_uart/common/uart_params.hpp"
#include "vip_uart/agents/uart_tx/tx.hpp"
```

Public agent APIs live in their agent headers; parameters/transactions live under `common`;
private helper structs stay in `.cpp` unless shared. Avoid giant public headers and broad namespace
imports.

### 3.4 Parameters and typed configuration

Centralize protocol shape, timing, widths, and derived helpers:

```cpp
struct XxxParams {
    unsigned data_bits = 32;
    unsigned addr_bits = 32;
    unsigned id_bits = 4;
    unsigned data_bytes() const { return (data_bits + 7u) / 8u; }
};
```

Name protocol clock counts `*_clk`, `*_clks`, or `*_cycles`; reserve `*_tick(s)` for raw simulator
ticks compatible with `test::sim_tick_t`. Prefer runtime-configurable cheap-to-copy objects over
templates unless templates clearly help. Do not scatter width math.

Use typed C++ constants, not macros:

```cpp
inline constexpr unsigned TID_BITS = 4u;
inline constexpr bool USE_RGMII = true;
```

For CMake/elaboration values, use `configure_file()` to generate a small header of typed
`inline constexpr` values.
Use `#if/#ifdef` only for genuine source exclusion, not ordinary configuration spread through
agents, scoreboards, pin definitions, and tests.

### 3.5 Wide and packed values

Never use numeric RapidVPI access above 64 bits. Use string/hex or protocol bit-vector helpers;
preserve width, leading zeros, byte order, and MSB/LSB definition. Prefer byte vectors internally
for byte streams and convert only at the VPI boundary. Testcases should not manually slice large
strings when an agent can decode them. Reusable aliases such as `XxxBitVec`, `AxiBitVec`,
`XxxDataWord`, and `XxxMask` are appropriate.

```cpp
auto wr = test.getCoWrite();
wr.write("wide_data", "00112233445566778899AABBCCDDEEFF", 16);
co_await wr;

auto rd = test.getCoRead();
rd.read("wide_data");
co_await rd;
const std::string value = rd.getHexStr("wide_data");
```

---

## 4. Project clock architecture

Use `vip::common::Clock` for external/free-running DUT input clocks that verification must start,
stop, rephase, or retime. The project `Test` owns clock controllers; reusable protocol agents
normally observe the actual clock net and drive only protocol data/control.

### 4.1 Classify ownership first

| Clock class | Owner and rule |
| --- | --- |
| Project/VIP-owned DUT input | Create exactly one `Clock`; no protocol driver also toggles it. |
| DUT-generated output | Observation-only; never create a controller for it. |
| Protocol-associated input already owned by `Clock` | Agent uses the real edge but drives only payload/control. |

This is the same one-writer rule used for every other signal.

### 4.2 Backends

| Backend | Construction | Waveform owner | Use |
| --- | --- | --- | --- |
| `LegacyVpi` | three arguments | `Clock` writes every edge through VPI | compatibility only |
| `NativeHdl` | plus `NativeClockCfg` | `dut_wrapper.sv` generates edges; `Clock` writes controls | default for standard/new projects |

```cpp
vip::common::Clock legacy(tb, "clk", "clk_run");

vip::common::Clock native(
    tb, "clk", "clk_run",
    vip::common::Clock::NativeClockCfg{
        .enable_net = "sim_clk_enable",
        .period_ticks_net = "sim_clk_period_ticks",
        .stopped_net = "sim_clk_stopped",
    });
```

Native mode removes repetitive edge traffic from VPI and is normally much faster. The actual
clock net remains registered and observed even though `Clock` does not write it.

Create one independent `Clock` for each independently controlled external clock. Every instance
needs its actual net, a unique RapidVPI task name, and its own native control triplet. Construction
registers the long-lived `clk_run()` task automatically; never register or launch it again.
Mutually exclusive elaborated clocks may use dynamic ownership, but the same rules apply.
For example, the active controller may be held by `std::unique_ptr<vip::common::Clock>`.

### 4.3 Native control contract

For actual `<clock>` use:

```text
sim_<clock>_enable        1=run; 0=stop and park low
sim_<clock>_period_ticks  64-bit full period in wrapper-control ticks
sim_<clock>_stopped       1 only when fully stopped and parked low
```

The standard `dut_wrapper.sv` uses 1 ps precision; then one control tick is 1 ps and effective VPI
precision must be `-12`. If `Clock` forwards `test::sim_tick_t` directly, VPI and wrapper ticks
must represent the same duration. Validate this at startup. A mismatch is infrastructure failure,
not something testcases silently convert. Any future scale conversion belongs in the clock backend.

Standard HDL waveform semantics:

- initialize disabled and low;
- clamp full period to at least two ticks;
- `high_ticks = period_ticks / 2`;
- `low_ticks = period_ticks - high_ticks` (odd extra tick is low);
- disable parks low before asserting `stopped`;
- after the HDL clock process observes enable, it produces the first rising edge according to the
  wrapper implementation and thereafter preserves the requested full period.

The native backend schedules control writes; it does not own HDL evaluation ordering. In
particular, `start_at()` schedules the enable/period control request at its absolute target tick. It
does not portably guarantee that the first physical edge observed through VPI has that same tick.
With stock Verilator VPI execution, the write may become visible to the HDL clock generator at a
later evaluation event even though every subsequent physical period is exact. Tests whose contract
depends on the waveform qualify the actual HDL clock net as described in Section 4.6.

Do not reimplement these semantics in an agent.

### 4.4 Required project wiring

`pindefs.hpp` defines typed `inline constexpr` names for every actual clock and native
enable/period/stopped net. `initNets()` registers them with widths 1/64/1 plus the actual 1-bit
clock. Do not register mutually exclusive nets absent from the elaborated wrapper.

`test.hpp` owns one controller per external clock; `test.cpp` constructs each with actual net,
unique task name, and matching triplet. Project documentation classifies every clock as external
or DUT-generated.

`Clock` controls run/period/phase; `CommonUtils` observes actual edges:

```cpp
vip::common::Clock rx_clock(*this, rx_clk, "rx_clk_run", native_cfg);
vip::common::CommonUtils rx_utils(*this, rx_clk);
```

### 4.5 Public control semantics

| Call | Exact meaning |
| --- | --- |
| `start<U>(period)` | Native mode writes period then enable; does not promise first edge was observed and does not re-anchor an already-running native clock. |
| `set_period<U>(period)` | Change requested full period; no deterministic stop/rephase. |
| `stop()` | Request disable; not a fully parked barrier. |
| `wait_stopped()` | Observe stopped state; does not request stop. |
| `stop_and_wait()` | Cancel scheduled start, request stop, and wait until physically parked low. |
| `is_running()` | Requested state only; may be true before a scheduled first edge. |
| `period_ticks()` | Requested full period in raw simulator ticks. |
| `start_after<U>(period, delay)` | Schedule the native-clock control request relative to now; the actual first HDL edge must be observed when it matters. |
| `start_at<U>(period, tick)` | Schedule the native-clock control request at an absolute raw tick; it does not guarantee an identically timestamped observed first edge. |

Scheduled control requests require physical stop, no other pending start, a future target, and at
least ten raw ticks of lead in the current implementation. Invalid or already missed request
targets fail instead of being silently rescheduled. A valid request can nevertheless become visible
to a native HDL generator at a later backend evaluation event; that is distinct from rescheduling
the request. Never call `start_at()` on a running clock.

Deterministic rephase request:

```cpp
co_await clk.stop_and_wait();
co_await clk.start_after<test::ns>(8.0, 2.0);
```

For several clocks, stop all, choose one common future absolute anchor, and call `start_at()` for
each with its intended phase offset. One logical controller owns commands for each instance; do
not rely on same-tick arbitration between concurrent start/stop/period commands.

```cpp
const auto anchor = vip::common::sim_time_ticks() +
    vip::common::duration_to_ticks<test::ns>(test, 100.0);
co_await clk_a.start_at<test::ns>(8.0, anchor);
co_await clk_b.start_at<test::ns>(8.0, anchor + phase_ticks);
```

The anchor and offsets express requested control timing. When functional coverage depends on the
resulting physical period, phase, or edge relationship, observe and qualify the actual clock nets
before launching stimulus.

### 4.6 Physical waveform qualification

A testcase that merely calls `start()`, `start_at()`, or `start_after()` has proved only that it
issued a control request. When clock rate, phase, or CDC relationship is part of the testcase
contract:

1. stop and park every clock whose phase will be controlled, and ensure observation is persistent
   or synchronously armed before the relevant waveform;
2. issue the intended period and phase requests with adequate lead time;
3. observe the actual HDL clock nets and capture multiple consecutive physical edges per domain;
4. require at least three consecutive observed period deltas to equal the configured physical
   period unless the testcase intentionally applies bounded jitter;
5. derive the cross-domain relationship from observed edges. For rational periods, a GCD/LCM
   repeat calculation may prove the expected phase sweep; for equal-frequency domains, require a
   measured non-zero offset when non-alignment is the intended stress condition;
6. record requested timing separately from observed edge ticks so logs expose backend propagation
   without treating it as protocol failure;
7. run the complete functional CDC workload only after waveform qualification and require its
   data, control, status, counter, and completion witnesses.

One callback timestamp or equality between a requested start tick and the first observed edge is
not a valid waveform qualification. Physical-clock qualification is a precondition to functional
CDC coverage, not a substitute for traffic crossing the domains.

### 4.7 Reset and testcase lifecycle

- explicitly start every destination clock needed for synchronous reset/CDC propagation;
- keep clocks running during reset-based cancellation, graceful quiescence, and cleanup;
- centralize nominal start/period setup;
- normally keep free-running clocks running between ordinary cases;
- restore requested period/enable separately from phase;
- do not re-anchor between tests to conceal a phase-sensitive VIP race;
- when deterministic phase isolation is the actual test contract, use
  `stop_and_wait()` plus `start_at()`/`start_after()` explicitly, then qualify the actual waveform;
- preserve intentional inherited phase stress and its regression order.

To retrofit a legacy project: first apply the standard `rtl_design_guide.md` native-clock
architecture to RTL `dut_wrapper.sv`; map
every external clock and triplet; retain/register actual clock nets; register native controls;
change three-argument constructors to native four-argument constructors; remove clock generation
from protocol drivers; leave DUT clocks observation-only; preserve existing testcase clock API
calls unless intent changes; do not refactor unrelated behavior. The retrofit changes waveform
ownership, not protocol coverage.

---

## 5. Agent and signal ownership

Agents are long-lived `Test` members. Constructors normally accept a port count or explicit base
names and derive pins from stable suffixes (`m0_awvalid`, `uart0_tx_o`). Use a port-map/config object
when naming differs; do not hardcode every pin in testcases.

Typical public shape:

```cpp
using RunTask = test::TestBase::RunTask;
RunTask agent(unsigned idx);
void attach_scoreboards(...);
```

### 5.1 Mandatory per-net declaration

Before coding coroutines, every agent design and README declares:

| Net/group | DUT direction | Sole physical writer | Setup/sample point | Accepted-event owner | Terminal/completion owner | Passive consumers |
| --- | --- | --- | --- | --- | --- | --- |
| request VALID/payload | input | initiator agent | setup before active edge | transaction worker | transaction worker | monitor/trace/rules |
| request READY | by DUT role | responder agent or DUT | setup before active edge | same declared request owner | none independently | monitor/trace |
| response group | by DUT role | responder agent or DUT | protocol-defined | response worker | transaction worker | designated scoreboard/monitor |

Group nets only when writer, acceptance, reset, and lifecycle are identical. Review the table
against `pindefs.hpp`, `initNets()`, constructors, reset paths, testcase helpers, monitors, and
scoreboard attachments. A testcase may configure the owner, enqueue commands, and inspect passive
history; it may not temporarily take over an owned output.

Ownership transfer, if supported, must be explicit and quiescent. Reset is handled by the existing
owner and never creates a second driver.

### 5.2 Initiator/source/master

It owns:

- command queues and ticket allocation;
- all declared initiator outputs and their reset/idle state;
- split direction/channel workers when required;
- acceptance and terminal counters used for its tickets;
- response capture and completion state;
- optional trace/history;
- expected-side calls when enqueue is the declared expectation creator;
- observed terminal calls when it is the declared observer.

```cpp
unsigned enqueue_write(...);
unsigned enqueue_read(...);
bool is_done(unsigned ticket) const;
RunUserTask wait_done(unsigned ticket); // low-level; testcase must bound it externally
void set_backpressure_mode(...);
```

Expose `is_done()` or equivalent so `tc_utils` can impose a derived budget. After enqueue, the
testcase must not drive source pins, count accepted items, recognize terminal markers, call a
duplicate terminal scoreboard path, or retire the ticket.

For split protocols, keep high-level API in `master.hpp` and mechanics in focused `.cpp` files such
as `master_wr.cpp`, `master_rd.cpp`, and coroutine files.

### 5.3 Responder/sink/slave

It owns declared responder outputs, acceptance/ready policy, response generation, optional memory,
delay/error injection, accepted-side events, stability/deep checks, and exactly one response
retirement path.

```cpp
enum class ReadyMode { ALWAYS, PULSE_RANDOM, SCRIPTED };
void set_ready_mode(const std::string& port, ReadyMode mode);
void set_response_delay_once(const std::string& port, unsigned id, unsigned cycles);
void set_response_value(const std::string& port, unsigned response);
```

Configure the responder before stimulus can arrive. Tests select policy through APIs; they do not
write READY, response VALID/payload, serial return data, or another responder output. If both
endpoints are VIP components, both may observe a handshake for local state, but only the declared
owner performs each ticket/scoreboard side effect.

### 5.4 Passive monitor

A passive monitor:

- never writes DUT-visible signals and needs no write barrier;
- captures a complete candidate in the setup phase;
- crosses the one qualified sample/active edge;
- classifies from the capture, never from a later live re-read;
- emits each accepted/terminal observation once;
- forwards functional events only when declared the sole observer; otherwise history/trace/rules;
- exposes optional capture enable, history, and clear/reset APIs.

Do not count handshakes with `getCoChange(VALID/READY)`. A persistent monitor starts with project
agents; a one-shot monitor must be synchronously armed or acknowledge readiness within a bound
before stimulus. Late launch cannot recover a completed event.

### 5.5 Serial/pin-level and convenience agents

For UART, MDIO, SPI, I2C-like, and other pin protocols, split source/TX driving, sink/RX sampling,
optional passive decoding, timing config, and optional error injection. Use clock cycles when based
on a project clock; use explicitly unit-tagged RapidVPI delays when based on simulation time.

UART-style TX owns idle, start, data, optional parity, stop, gap, and break. RX owns start-edge
detection, bit-center sampling, shift/parity/stop decode, and one frame/error observation. Every
sample/drive follows Section 6.

Convenience wrappers may aggregate leaf agents (for example `UartEndpoint { tx; rx; }`) but must
not hide useful knobs or prevent direct leaf access.

---

## 6. Phase, sampling, handshake, and retirement

### 6.1 Causality rule

Treat these as RO observations:

```cpp
co_await test.getCoRead();
co_await test.getCoRead<test::ns>(...);
co_await test.getCoChange(...);
co_await utils.clock(...);
```

Treat these as write-side operations:

```cpp
auto wr = test.getCoWrite();
co_await wr;
co_await test.getCoWrite<test::ns>(...);
co_await utils.write_barrier();
```

An observation describes a slot/edge that has happened. A subsequent write cannot retroactively
affect it. Move through the protocol-required future edge/setup interval and use a fresh writer for
each deliberate source-state update. Do not use bus equality waits or simulator-specific delays.

Every write helper documents:

- allowed entry phase;
- internal RO/clock/write awaits;
- which physical state it applies;
- which future edge may consume that state;
- return phase;
- whether caller or helper establishes the safe phase.

Neither `clock -> writer`, `clock_to_write -> writer`, nor `write_barrier -> writer` is a universal
recipe. The caller/helper contract determines what is required.

### 6.2 Safe helper layering

Separate a helper that assumes the first legal post-edge phase from an arbitrary-entry wrapper:

```cpp
RunUserTask drive_idle_now_() {
    // Precondition: caller crossed the final acceptance/sample edge.
    auto wr = tb_.getCoWrite();
    wr.write("valid", 0);
    wr.write("data", 0);
    co_await wr;
}

RunUserTask drive_idle_from_arbitrary_phase_() {
    co_await synchronize_to_protocol_safe_write_phase();
    co_await drive_idle_now_();
}
```

`*_now_()` adds no unconditional edge/barrier. A terminal path already in the legal post-edge
phase calls it directly. An arbitrary cleanup path synchronizes exactly once. Do not build
unexplained `barrier -> writer -> barrier -> writer` chains: an extra transition can postpone
retirement beyond another sampling edge and duplicate the terminal beat/symbol.

### 6.3 Setup-sample/paired-edge monitor template

For a rising-edge valid-ready interface:

```cpp
while (true) {
    co_await utils.clock(1, 0); // setup/falling edge

    auto rd = test.getCoRead();
    rd.read(valid_net);
    rd.read(ready_net);
    rd.read(payload_net);
    rd.read(last_net);
    co_await rd;

    ObservedBeat captured{};
    captured.valid = (rd.getNum(valid_net) & 1ULL) != 0ULL;
    captured.ready = (rd.getNum(ready_net) & 1ULL) != 0ULL;
    captured.payload = read_payload(rd, payload_net);
    captured.last = (rd.getNum(last_net) & 1ULL) != 0ULL;

    co_await utils.clock(1, 1); // only edge qualified by the capture

    if (captured.valid && captured.ready) {
        observe_accepted_once(captured); // record paired rising-edge tick
    }
}
```

For falling-edge, DDR, bit-center, or asynchronous protocols, substitute the defined setup and
sample points while keeping:

```text
capture complete candidate -> cross its one qualified sample point -> classify capture once
```

### 6.4 Valid-ready source lifecycle

Required sequence:

1. Cross a known active edge while the new item is absent.
2. Present VALID and payload with one fresh legal post-edge writer.
3. Hold every source field stable during stall.
4. Sample READY in the setup phase preceding one candidate active edge.
5. Cross that active edge; count acceptance only if the captured READY was asserted.
6. In the first legal post-acceptance write phase, retire VALID or replace the item for documented
   zero-gap chaining.

```cpp
co_await utils.clock(1, 1);

{
    auto wr = test.getCoWrite();
    write_payload(wr, payload);
    wr.write("valid", 1);
    co_await wr;
}

while (true) {
    co_await utils.clock(1, 0);

    auto rd = test.getCoRead();
    rd.read("ready");
    co_await rd;
    const bool ready = (rd.getNum("ready") & 1ULL) != 0ULL;

    co_await utils.clock(1, 1);
    if (ready) break;
    // No write while stalled; VALID/payload remain stable.
}

{
    auto wr = test.getCoWrite();
    wr.write("valid", 0);
    co_await wr;
}
```

Do not drive at a falling edge and then wait another falling edge before the first READY sample;
that can skip an eligible rising edge. Do not read READY immediately after the source write and do
not add a generic barrier between the accepting edge and first post-edge retirement.

### 6.5 Terminal and ticket semantics

The terminal accepted/sample edge is followed by retirement/replacement in the first legal
post-edge writer before another active edge can consume the same state. This prevents duplicate
AXIS TLAST beats, extra GMII/FCS bytes, and repeated serial terminal symbols.

Ticket `done` means documented physical progress, never queue insertion or coroutine wakeup:

- final required accepted/sample event occurred;
- required response was captured;
- required source/ready idle or chaining update was applied;
- terminal functional observation happened once.

Expose `first_accept_tick`, `last_accept_tick`, or `terminal_sample_tick` when exact timing matters.

---

## 7. Protocol profiles

### 7.1 Generic valid-ready and AXI-Stream

Use Section 6.4 for every source and Section 6.3 for passive observation. The sink/READY owner uses
the same setup/paired-edge acceptance convention. Source payload includes every stability-bound
field; for AXIS this includes TDATA/TKEEP/TSTRB/TID/TDEST/TUSER/TLAST as implemented. Advance beat
and packet state only on accepted edges. TLAST belongs only to the exact terminal accepted beat.

Backpressure is configured through the READY owner. A last-beat stall test must prove at least one
sampled `VALID && !READY` cycle on the terminal beat and one later accepted terminal edge; elapsed
cycles after enqueue are not a substitute.

### 7.2 AXI-Lite

Requests and responses have separate ownership.

**AW/W/AR requests:** cross a known rising edge with request VALID low; present VALID/payload in one
fresh post-rising write; preserve a real low half-cycle; sample corresponding READY at the falling
edge; count only the paired rising edge. AW and W are independent: track and retire each separately
while preserving the other stalled channel.

**B/R responses for one-outstanding helpers:** keep BREADY/RREADY low until VALID and payload are
observed/captured. The slave must hold VALID. In a later legal write phase assert READY, cross the
known accepting rising edge, then apply the documented ready state before completing. A directed
held-response helper may intentionally return with READY low only when the caller owns the bounded
scenario that resolves it. Held BVALID is useful proof that a write launched while acceptance is
deliberately postponed.

### 7.3 Full AXI4 physical ownership

Signal writers are determined by protocol role, whether that role is DUT or VIP:

| Channel | Master-role sole outputs | Slave-role sole outputs | Acceptance |
| --- | --- | --- | --- |
| AW | `AWVALID`, `AWID`, address/control | `AWREADY` | captured `AWVALID && AWREADY` at paired rising edge |
| W | `WVALID`, `WDATA`, `WSTRB`, `WLAST`, sidebands | `WREADY` | captured `WVALID && WREADY` at paired rising edge |
| B | `BREADY` | `BVALID`, `BID`, `BRESP`, sidebands | captured `BVALID && BREADY` at paired rising edge |
| AR | `ARVALID`, `ARID`, address/control | `ARREADY` | captured `ARVALID && ARREADY` at paired rising edge |
| R | `RREADY` | `RVALID`, `RID`, `RDATA`, `RRESP`, `RLAST`, sidebands | captured `RVALID && RREADY` at paired rising edge |

No testcase, monitor, trace, scoreboard, or helper may write an output already owned by the active
master/slave agent. Configure backpressure through the owner of READY.

### 7.4 Full AXI4 master lifecycle

| Lifecycle | Sole logical owner | Completion rule |
| --- | --- | --- |
| write command/expectation | write transaction owner | one ticket and one expected original write |
| AW | AW worker under transaction owner | stable until one acceptance; retire AWVALID once |
| W | W worker under transaction owner | advance only on acceptance; WLAST on exact terminal burst beat |
| B | B worker under transaction owner | one matching accepted BID/BRESP and one terminal observation |
| write completion | write transaction owner | required AW/W accepted, B accepted, required driver retirement applied |
| read command/expectation | read transaction owner | one ticket and one expected original read |
| AR | AR worker under transaction owner | stable until one acceptance; retire ARVALID once |
| R | R worker under transaction owner | count/capture only accepted beats; validate RID/order/response/RLAST |
| read completion | read transaction owner | one legal accepted terminal R beat and required ready/driver update |

AW and W are independent. Never require same-cycle acceptance, retire both because one handshook,
or advance W without acceptance. The agent, not testcase, owns BREADY/RREADY, response capture,
ID/tag correlation, R beat count, RLAST, scoreboard observation, and ticket retirement.

### 7.5 Full AXI4 slave lifecycle

| Lifecycle | Sole logical owner | Rule |
| --- | --- | --- |
| AW capture/context | slave AW worker | capture once; create/order write context per supported profile |
| W capture | slave W/context owner | capture accepted beat once; enforce WLAST and associate context |
| B source | slave B worker | one response per completed write; stable while stalled; retire on acceptance |
| AR capture/context | slave AR worker | capture once and allocate read context |
| R source | slave R/context owner | stable while stalled; advance on acceptance; exact RLAST |

Memory models, generators, and scoreboards may consume events but cannot independently close the
same transaction. Declare write-context assembly and read-response retirement ownership explicitly,
especially with multiple queues/schedulers.

### 7.6 AXI IDs, outstanding work, segmentation, and aggregation

- allocate stable internal ticket/transaction identity at enqueue/acceptance;
- track AXI ID separately; multiple tickets may share an ID subject to ordering;
- preserve per-ID ordering while allowing legal inter-ID concurrency/reordering;
- map responses only to outstanding contexts; unexpected/duplicate BID/RID, excess beats, illegal
  LAST, or response without context is failure;
- an original transaction split into internal segments still has one original expectation and one
  original completion;
- internal segment events may be traced but cannot create duplicate expectations or early done;
- aggregate segmented B results according to the documented model; never complete on the first
  internal B;
- count R only in the designated R owner; require legal RLAST for each actual burst/segment and
  complete the original only under the documented aggregation policy;
- do not assume each segment equals the maximum length: require non-zero bounded segments, exact
  total original beats, and address/control continuity where the scenario checks segmentation.

Master R ownership structurally remains one path:

```cpp
// RREADY is maintained only by its legal ready-policy writer.
co_await utils.clock(1, 0);
auto rd = capture_r_setup();
co_await rd;
const auto beat = decode_r_setup(rd);
co_await utils.clock(1, 1);

if (beat.valid && beat.ready) {
    correlate_id_or_fail(beat.id);
    append_data_and_advance_once(beat);
    if (beat.last) {
        validate_terminal_or_fail();
        observe_terminal_once(beat);
        complete_ticket_once();
    }
}
```

### 7.7 Exact-cycle tests

Anchor to actual protocol acceptance/sample edges, never `enqueue()`, queue insertion, coroutine
wakeup, or host call time. Prefer ticket metadata such as `first_accept_tick` and calibrate from
protocol-event deltas.

For a native-clock start or rephase, the `start_at()` target is a control-request timestamp, not a
portable physical-edge timestamp. Exact-cycle protocol targets must therefore be derived from an
observed, qualified clock edge or from a prior accepted protocol event. Never calculate a claimed
physical acceptance tick directly from the requested native-clock anchor.

Before requiring a target acceptance edge, derive the minimum causal lead from every preceding
control-plane handshake, destination-clock sample, CDC/publication step, and setup interval. If the
selected interface rates cannot physically complete that preparation before the target, the
testcase construction is infeasible; it is not evidence that the DUT cannot operate with those
clock rates. Arm or launch earlier, choose a later protocol-relative target, or redesign the
scenario. Do not raise a normally slow control clock merely to preserve an accidental old schedule,
and do not weaken the collision witness.

For an exact AXI-Lite target: present AW/W in the safe phase preceding the target; sample READY at
the intervening falling edge; cross the specified rising acceptance edge; record and require its
actual tick; hold BREADY low until response is visible, then accept deliberately. Absolute target
ticks may differ between simulators if software scheduling differs; the intended protocol collision
and result must match. Never add simulator-specific offsets.

### 7.8 Non-valid-ready sampled protocols

For GMII/RGMII, UART, SPI-like, DDR, and pin protocols:

- drive a complete symbol before its defined sample edge and hold through it;
- change state only in the legal post-sample writer;
- retire the final symbol immediately after its final sample, without an extra generic barrier;
- do not complete before delivery/retirement;
- make bit-center, rising/falling, and DDR edge choices explicit.

Conceptual byte-per-clock terminal path:

```text
drive final byte -> rising edge samples it -> first legal post-sample write deasserts active
```

No second active edge may sample the same final byte.

---

## 8. Scoreboards and event authority

Use `vip_common` reporting when available. Reusable scoreboards normally provide:

```cpp
void reset_case();
void end_case_check(bool fail_on_outstanding = true);
void set_verbose(bool enable);
```

Rules scoreboards may additionally expose `set_enable()` and `set_warn_only()`. Event APIs accept
raw `test::sim_tick_t`.

### 8.1 Scoreboard classes

**Functional scoreboards** compare expected and observed transactions: AXI writes close on the
master's accepted B; reads close on accepted terminal R; UART streams compare decoded frames.
Provide clear `expect_*()` and `observe_*()` methods and fail at case end for outstanding or
unexpected items.

**Rules scoreboards** check legality independent of expected lists: payload stability during stall,
LAST position, ID/context validity, UART parity/stop, MDIO turnaround, packet termination. New/noisy
strict checks may default disabled during compatibility rollout, but locked regression supports
fatal mode. Timestamp events in raw ticks.

**Local scoreboards** contain DUT semantics such as routing, command status, and register meanings.
They consume reusable observations without contaminating `vip_xxx`.

### 8.2 One expectation and one terminal observation

Choose one expectation policy per transaction API:

| Policy | Sole creator | Testcase action |
| --- | --- | --- |
| automatic | enqueue API/agent | enqueue only; never call `expect_*()` again |
| manual | testcase/local scenario helper | call once before activation; agent does not auto-expect |
| model-derived | designated reference monitor/model | configure model; no duplicate manual/enqueue expectation |

Choose one functional terminal observer:

| Transaction | Sole closing observer |
| --- | --- |
| valid-ready packet | designated sink/packet worker on accepted terminal beat |
| independent request/response | transaction owner on accepted terminal response |
| UART frame | RX decoder on final stop-bit sample |
| AXI write | master write owner on accepted B |
| AXI read | master R owner on accepted RLAST |

Bad duplication:

```cpp
test.scb_axi.expect_read(req);              // manual expectation
const auto ticket = test.axi_mst.enqueue_read(req); // also auto-expects
co_await testcase_owned_r_beat_counter();   // duplicate completion path
test.scb_axi.observe_master_rlast(...);     // agent also calls this
```

Correct ownership:

```cpp
const auto ticket = test.axi_mst.enqueue_read(req);
bool completed = false;
co_await wait_ticket_bounded(
    test, test.axi_mst, ticket, budget_cycles, "read completion", completed);
if (!completed) co_return;

const auto history = test.axi_mon.get_history("m0"); // diagnostics only
```

Do not add scoreboard de-duplication to hide duplicate ownership; it can conceal a real repeated
DUT event. Remove the extra expectation/observation path.

---

## 9. Testcase construction and non-vacuity

### 9.1 File and responsibility shape

```text
src/test.cpp                 thin orchestration and runner hooks
src/test.hpp                 Test-owned agents/scoreboards/clocks
src/init.cpp                 VPI net registration
src/pindefs.hpp              typed top-level names/widths/DUT constants
src/cases/tc_utils.hpp/.cpp  shared bounded scenario helpers
src/cases/tc_name.hpp/.cpp   registration and testcase body
```

Each testcase performs its required reset/precondition, configures APIs, enqueues commands, waits
with explicit bounds, proves activation, and performs scenario-specific checks. Reusable mechanics
move into agents or `tc_utils`.

```cpp
void register_tc_smoke(vip::common::Runner& runner) {
    runner.add("tc_smoke", [](Test& test) -> Test::RunTask {
        co_await reset_case_local(test);
        const auto ticket = test.agent.enqueue_transfer(...);

        bool completed = false;
        co_await wait_ticket_bounded(
            test, test.agent, ticket, transfer_budget, "smoke", completed);
        if (!completed) co_return;
        co_return;
    });
}
```

### 9.2 Bounded waits

Every wait that can fail because of DUT/VIP behavior is bounded, including ticket, frame/packet
count, status, monitor completion, and between-case cleanup. The caller passes a scenario-derived
budget; helpers do not hide huge generic defaults.

Derive in the observed domain:

- source: beats/symbols + configured gaps + permitted backpressure + retirement margin;
- physical frame: wire duration + intentional hold/pause + bounded pipeline/CDC margin;
- sink drain: expected accepted items + configured stalls + bounded elasticity;
- status/event: inducing latency + bounded CDC/publication margin.

`RunUserTask` does not return bool, so a compact helper may return completion by reference:

```cpp
template <typename DoneFn>
static TestBase::RunUserTask wait_condition_bounded(
    Test& test,
    CommonUtils& utils,
    DoneFn done,
    unsigned budget_cycles,
    const std::string& context,
    bool& completed) {
    completed = false;
    for (unsigned cycle = 0; cycle < budget_cycles; ++cycle) {
        if (done()) {
            completed = true;
            co_return;
        }
        co_await utils.clock();
    }
    test.scb.note_fail(context + " timed out after " +
                       std::to_string(budget_cycles) + " protocol cycles");
    co_return;
}
```

The predicate reads C++ agent state such as `is_done()`, not raw VPI. Net polling follows Section
2.5. On timeout report context, domain/budget/elapsed amount, expected/observed state/count, and
ticket/frame identifiers; do not log each cycle. The caller immediately exits the dependent
subcase.

For a known long interval, use an explicit simulation-time/cycle delay for the uninteresting prefix
and a short bounded edge observation around the expected transition. Never use host sleep.

### 9.3 Case selection is not build membership

The normal `.so` contains all normal testcase sources and registrations. Temporary isolation
changes only the execution plan, for example by commenting a `smoke_plan.emplace_back(...)` line.
Do not remove sources from CMake, includes, or `register_tc_*()` calls unless an explicitly requested
special one-off build requires it.

### 9.4 Scenario activation and non-vacuity

A directed test passes only if the named mechanism activated:

| Scenario | Mandatory witness |
| --- | --- |
| arbitration/contention | at least two eligible requesters coexist for the same arbitration decision |
| segmentation/preemption | real segment boundary and another eligible requester in the decision window |
| backpressure | intended sampled `VALID && !READY` stall on the target channel |
| reorder | required multiple outstanding IDs/routes and permitted order change |
| error injection | arm consumed and intended error/response observed |
| timeout/recovery | timeout precondition persists for budget and recovery path runs |
| clock/CDC phase | multiple actual HDL edges qualify every period; the intended relative relationship is measured from the waveform, not inferred from enqueue or a `start_at()` request |

Missing required activation is failure, never warning-only success. Snapshot/reset counters per
case so prior activity cannot satisfy the witness. Counters come from the declared agent metadata,
passive monitor, rules checker, or event ledger; a testcase must not duplicate beat/terminal
accounting.

```cpp
const auto before = test.mon.snapshot_activation_counters();
// Configure, enqueue, and wait through agent-owned paths.
const auto after = test.mon.snapshot_activation_counters();

const auto segments = after.segment_boundaries - before.segment_boundaries;
const auto contention =
    after.contended_admission_windows - before.contended_admission_windows;

if (segments == 0 || contention == 0) {
    test.scb.note_fail("segmentation scenario did not activate");
    co_return;
}
```

A negative test first proves its stimulus precondition and bounded observation window, then requires
the forbidden count to remain zero.

### 9.5 Configure, arm, enqueue, then wait

For multiple participants:

1. apply responder, route, delay, backpressure, and one-shot injection policies;
2. reset/snapshot passive history and activation counters;
3. ensure persistent observation is running or synchronously arm it;
4. enqueue every independent contender with no await between enqueues;
5. enter passive observation/completion as the first behavior-dependent await;
6. use bounded agent-owned completion paths;
7. require activation witnesses and final scoreboard state.

If arming requires a coroutine, obtain a bounded readiness acknowledgement before stimulus. Merely
launching an unacknowledged coroutine depends on scheduler order.

Bad contention test:

```cpp
const auto first = test.mst0.enqueue_read(req0);
co_await test.mst0.wait_done(first);
const auto second = test.mst1.enqueue_read(req1); // cannot contend with first
```

Correct construction:

```cpp
test.slv0.set_response_policy(policy);
test.mon.clear_history("s0");
test.mon.arm_capture("s0");

const auto target = test.mst0.enqueue_read(req0);
const auto guard = test.mst1.enqueue_read(req1); // no await between

bool target_done = false;
co_await wait_ticket_bounded(
    test, test.mst0, target, target_budget, "target", target_done);
if (!target_done) co_return;

bool guard_done = false;
co_await wait_ticket_bounded(
    test, test.mst1, guard, guard_budget, "guard", guard_done);
if (!guard_done) co_return;

if (test.mon.contended_admission_count("s0") == 0) {
    test.scb.note_fail("contention did not activate");
    co_return;
}
```

The exact monitor API is project-specific; ordering and ownership are mandatory.

---

## 10. Reset, case isolation, and runner lifecycle

Persistent agents may retain active/queued work after a failing testcase. Before the next case:

1. run end-of-case scoreboard checks while real outstanding expectations remain visible;
2. under the documented reset/quiesce condition, cancel or drain external stimulus;
3. keep clocks running where synchronous reset/cleanup requires them;
4. wait for ownership acknowledgement with an explicit bound;
5. clear histories, one-shot knobs, queues, tickets, and mutable per-case state;
6. start the next case only when quiescence is proven.

If cleanup cannot prove ownership within budget, report infrastructure failure, leave reset
asserted, and stop rather than continue with contaminated state. `agent.reset_case()` is not proof
of physical ownership cleanup and must not silently cancel old traffic before scoreboards inspect it.

Canonical hook order:

```cpp
runner.set_before_case_hook([this](const Runner::CaseDesc& c) {
    scb.reset_case();
    scb_protocol.reset_case();
    scb_rules.reset_case();
    agent.reset_case(); // local bookkeeping/config only
    scb.start_case(c.name);
});

runner.set_after_case_hook([this](const Runner::CaseDesc&) {
    scb_protocol.end_case_check(true);
    scb_rules.end_case_check(false);
    scb.end_case();
    scb.print_case_summary();
    scb.print_total_summary();
});

runner.set_between_case_hook([this](const Runner::CaseDesc& c)
                             -> TestBase::RunUserTask {
    co_await cleanup_case_stimulus_bounded(c);
    co_return;
});

runner.set_after_all_hook([this]() {
    final_trace_cleanup();
    core::finishSimulation();
});
```

Agents document reset behavior for active and queued commands, drive outputs to safe idle when
required, clear low-level active state, avoid ambiguous stale tickets, and expose reset-during-
transaction helpers when supported.

---

## 11. Configuration, stress, models, logging, and tracing

### 11.1 Knob design

Every knob has one owner, documented default, reset lifetime, and deterministic meaning.

| Category | Examples |
| --- | --- |
| protocol shape | data/address/ID width, parity, stop bits |
| timing | bit clocks, sample index, response delay, gap cycles |
| backpressure | ready mode, bubble period, seeded random, scripted stalls |
| injection | response, parity/framing error, drop, CRC corruption |
| checking | strict, warn-only, deep checks, verbose |
| tracing | enable, path, port filter, compression |

Use enums for growing mode sets, `set_*` for persistent configuration, and `arm_next_*` for
one-shot behavior. A one-shot affects exactly one documented transaction/frame and reports or
records consumption. Expensive history defaults off unless demonstrably cheap.

```cpp
void set_ready_mode(const std::string&, ReadyMode);
void set_default_response(const std::string&, unsigned);
void arm_next_response_error(const std::string&, unsigned);
void set_history_enable(bool);
void clear_history();
```

Random behavior requires an explicit reproducible seed. Testcase-local knobs reset between cases;
persistent configuration survives only when documented.

### 11.2 Event-relative stress

Stress hooks include valid/ready stalls, response delays, gaps, per-beat responses, last-beat
stalls, packet pauses, line glitches, framing/parity errors, timeout injection, and reset during
transfer. Timing-sensitive hooks trigger from a protocol event such as first VALID presentation,
first acceptance, terminal acceptance, or decoded frame start—not an arbitrary delay from enqueue.

```cpp
test.master.set_response_ready_policy(
    port,
    ResponseReadyPolicy{
        .trigger = ReadyTrigger::FIRST_VALID_PRESENTATION,
        .stall_cycles = 32,
    });
const auto ticket = test.master.enqueue_request(port, request);
```

The agent owns READY and consumes the trigger. The testcase never estimates a delay and writes
READY directly. Serial extensions may add idle gaps, break, bad stop/parity, bounded jitter, and
line glitches specified explicitly in protocol cycles or raw simulator ticks. Keep version one
small but make additions possible without breaking the stable API.

### 11.3 Protocol data and memory models

Responder models may provide byte-addressable memory, constant/scripted responses, register
callbacks, UART frame queues, or MDIO register banks. Keep generic models reusable and DUT meanings
local. Make byte order, alignment, and direct-helper consistency explicit; expose small setup and
inspection APIs without forcing testcases to inspect internal implementation.

```cpp
void write_mem_byte(const std::string&, std::uint64_t, std::uint8_t);
bool read_mem_byte(const std::string&, std::uint64_t, std::uint8_t&) const;
void clear_memory(const std::string&);
void share_memory(const std::string& dst, const std::string& src);
```

### 11.4 Logging and trace

- normal logs are concise; verbose/bring-up logs are controlled and removable;
- include agent/protocol, port, and raw tick where useful;
- failures state expected versus observed and relevant ticket/ID/beat;
- never print each polling cycle or giant payloads by default;
- converted time always includes a unit key;
- temporary diagnostics have mechanically searchable markers and are removed after closure.

Preferred:

```text
[SCB][FAIL][tick=123456] expected response=0 observed=2 ticket=7
[DBG][tick=123456][time_ns=123.456] sampled terminal symbol
```

Trace is optional/configurable, preferably per port, with configurable path and optional
compression. Structured records use raw tick timestamps and enough semantic fields for event-ledger
comparison. Default trace off unless tiny.

```cpp
struct TraceCfg {
    bool enable = false;
    std::string out_path;
    bool compress = true;
};
```

---

## 12. Cross-simulator qualification

### 12.1 Closure sequence

For scheduler-sensitive work:

1. identify the exact backend-specific VIP shared object loaded by the launcher and make its build
   provenance visible;
2. reproduce with the smallest ordering that exposes the failure;
3. capture scenario activation and a protocol-event ledger before editing;
4. fix the reusable writer/phase/owner instead of DUT or testcase expectations without evidence;
5. rebuild the exact loaded VIP artifact and rerun the focused reproducer on the affected simulator;
6. run the complete regression there;
7. rebuild the reference-backend artifact and run the complete regression on the reference
   simulator;
8. compare activation witnesses and normalized semantic ledgers;
9. remove temporary diagnostics and repeat focused plus full qualification.

Both flows compiling and showing equal PASS counts are insufficient. A test may pass vacuously.

### 12.2 Event ledger

Record from existing event owners or passive monitors, never a second driver/counter:

```text
testcase
clock domain
raw tick and protocol-relative edge/cycle
channel and port
event: PRESENT, ACCEPT, STALL, TERMINAL, RESPONSE, COMPLETE
internal ticket
protocol ID/tag
beat/symbol index
terminal marker
response/status
route/target
payload digest or selected semantic fields
```

Compare causally ordered per-channel events. Independent events at the same timestamp may appear in
different textual order and should compare as an unordered same-edge set when the protocol imposes
no order.

Required semantic agreement includes:

- testcase/seed/configuration/order and scoreboard classification;
- activation witnesses and required non-zero feature counts;
- observed physical clock periods and required relative phase/ratio witnesses for clock-sensitive
  cases;
- ordered channel PRESENT/ACCEPT/STALL/TERMINAL/RESPONSE/COMPLETE events;
- AXI-Lite `(address, data, strobe)` writes;
- logical read-state sequence after collapsing consecutive duplicate polls;
- TX terminal and RX physical-resolution outcomes;
- sticky-event, pause, snapshot, clear, final status, and statistics;
- protocol-defined on-wire/sample-edge durations.

Do not require identical absolute coroutine timestamps, native-clock command-to-first-edge
propagation latency, redundant poll counts, independent clock-domain log interleaving, or
visibility of superseded configuration applications when final protocol behavior is equal.

### 12.3 First-divergence triage

1. Align case, seed, configuration, reset release, and clock period/phase policy.
2. Confirm required activation exists in both runs.
3. Find the last matching and first missing, duplicated, or reclassified semantic event.
4. Inspect that event's setup capture, paired active edge, and first legal post-edge update.
5. Classify it as physical writer, accepted-event owner, terminal owner, or monitor error.
6. Repair that owner and return to the focused reproducer before full regression.

Do not begin from aggregate counts thousands of events later. The first divergence normally marks
where an incidental scheduler convenience entered the design.

---

## 13. Documentation and creation checklist

### 13.1 Required documentation

The top README states package scope, dependencies, construction, parameters, scoreboard wiring,
common examples, limits, timestamp policy, and integration. Every agent README includes its
per-net ownership table, naming, API, phase preconditions, expectation policy, ticket/completion
meaning, exposed acceptance timestamps, monitor classification policy, pitfalls, and examples.
Scoreboard documentation identifies expected/observed APIs, strictness knobs, lifecycle, and the
single expectation/terminal authority.

Project clock documentation distinguishes requested controller timing from observed physical HDL
edges, states which tests qualify physical periods/phase, and identifies every backend-specific
shared-library artifact consumed by simulation launchers.

Directed stress-case documentation states the mechanism, required activation witnesses,
concurrent participants, monitor arming, derived budgets, and ledger fields.

### 13.2 Creation checklist

Before calling a new or reworked `vip_xxx` complete:

1. **Scope:** define supported/unsupported protocol variants and future work.
2. **Transaction:** define fields, protocol versus DUT semantics, raw-tick timestamps, and exact
   meaning of done.
3. **Ownership:** declare one writer per DUT-visible output, one accepted-event owner, one terminal
   owner, reset owner, passive consumers, and any quiescent transfer procedure.
4. **Parameters:** centralize widths/timing/features and derived helpers; distinguish cycles from
   raw ticks.
5. **Clocks:** classify every clock; one controller per external clock; DUT clocks observation-only;
   register actual/native controls and validate tick scale; distinguish control-request timing from
   physical-edge observation and qualify actual waveforms in clock-sensitive tests.
6. **Agents:** define enqueue/completion APIs, port mapping, helper entry/return phases, acceptance
   timestamps, setup/paired-edge logic, and first-post-edge terminal retirement.
7. **Scoreboards:** define functional versus rules checks, raw-tick APIs, strictness, exactly one
   expectation creator, one terminal observer, and end-of-case checks.
8. **Build:** use unique PIC object targets under the parent `.so`; retain complete normal testcase
   source and registration membership; identify and rebuild the exact backend-specific plugin loaded
   by each simulator flow.
9. **Tests:** configure/arm first, enqueue all contenders before waiting, use derived bounds, stop
   dependent work on timeout, and require non-vacuity.
10. **Lifecycle:** check outstanding expectations before bounded cancellation/reset; prove quiescence
    with required clocks running; isolate per-case knobs/history/tickets.
11. **Finish:** call `core::finishSimulation()` once from project `after_all`, after final cleanup;
    never treat finish/exit alone as PASS, and ensure the regression driver consumes the final
    machine-readable scoreboard result.
12. **Qualification:** focused hostile ordering, full affected/reference regressions, activation and
    normalized ledgers, first-divergence analysis, diagnostic removal, final reruns.
13. **Documentation:** update ownership, timing, completion, examples, limits, and checklist.
14. **Evolution:** prefer enums, one-shot APIs, trace/history hooks, and backward-compatible
    constructors once stable.
15. **Restraint:** implement the smallest correct first version; add hooks only where they avoid
    predictable API churn.

### 13.3 Compact non-AXI example

A first UART VIP may support configurable 8N1, TX driver, RX bit-center decoder, framing reporting,
and stream scoreboard, with future parity/two-stop/break/glitch/RTS-CTS extensions.

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

class UartTx {
public:
    RunTask agent(unsigned idx);
    unsigned enqueue_byte(const std::string& port, std::uint8_t data);
    bool is_done(unsigned ticket) const;
    void set_inter_frame_gap_clks(const std::string&, unsigned);
    void arm_next_framing_error(const std::string&);
};

class UartRx {
public:
    RunTask agent(unsigned idx);
    void set_capture_enable(const std::string&, bool);
    std::vector<UartFrame> get_history(const std::string&) const;
};

class ScbUartStream {
public:
    void reset_case();
    void end_case_check(bool fail_on_outstanding = true);
    void expect_byte(const std::string&, std::uint8_t);
    void observe_frame(const std::string&, const UartFrame&, test::sim_tick_t);
};
```

The testcase uses `is_done()` through a frame-length-derived bounded helper. TX owns physical
delivery and idle retirement; RX owns decode and one frame observation. Add RTS/CTS later through
optional params/methods without breaking the stable TX/RX API.

---

## 14. Prohibited patterns

Do not:

- create a local raw protocol driver in each testcase or duplicate an existing `vip_xxx`;
- hardcode one DUT's names/semantics into reusable protocol code;
- let a testcase write an agent-owned output, including AXI `ARVALID` or `RREADY` beside a master;
- let a testcase count R beats, recognize RLAST, call `observe_master_rlast()`, or otherwise
  duplicate agent completion;
- combine a manual expectation with an enqueue API that auto-expects;
- feed one functional terminal event from both active agent and passive monitor;
- retain dead raw pin-driving, beat-counting, or terminal helpers after ownership moves;
- launch/arm a monitor after an event may already handshake;
- serialize participants in a testcase intended to prove concurrency;
- call a segmentation/admission/arbitration test valid with one eligible requester;
- accept missing feature activation as a warning;
- use unbounded behavior-dependent `wait_done()`, frame/packet count, status, or cleanup waits;
- use `getCoChange()` or later live re-read as a handshake counter;
- write after RO and claim the write affected that sampled edge;
- stack an unexplained barrier/edge before terminal retirement;
- complete a ticket before final physical acceptance/sample and required retirement;
- calibrate exact timing from enqueue/coroutine wakeup;
- use bus equality polling where bit masks and edge samples are required;
- use numeric VPI access above 64 bits or manually reverse packed field order;
- name raw ticks as ns, hand-convert ticks, call internal time helpers, or scatter raw VPI time;
- use obsolete implicit-unit APIs such as `getCoWrite(0)`, `getCoRead(0)`, `getCoWrite(10.0)`, or
  `getTime()`;
- use `#define` for ordinary typed C++ configuration;
- add random behavior without a reproducible seed;
- turn strict new checks on by default and silently break stable users;
- drive one clock from both protocol code and `Clock`, control a DUT-generated clock, omit actual
  clock registration in native mode, reuse clock task names, or ignore tick-scale mismatch;
- assume `is_running()` proves an edge exists or `start()` re-anchors a running native clock;
- treat a native `start_at()`/`start_after()` target as a guaranteed physical first-edge timestamp;
- qualify a clock/CDC testcase from requested settings or one first-edge callback instead of
  repeated actual HDL edges followed by functional traffic;
- call `start_at()` on a running clock rather than `stop_and_wait()` first;
- re-anchor clocks merely to hide a VIP race;
- finish from testcase/agent/`after_case`, use raw `vpi_control`, or treat finish/process exit zero
  as PASS without the final scoreboard result;
- assume rebuilding the RTL simulator or another CMake tree refreshed the backend-specific VIP
  `.so` actually named by the launch log;
- disable a testcase by removing CMake source or registration rather than the execution plan;
- add simulator-specific scheduling branches, delays, or offsets;
- classify equal logical state with different redundant poll counts as a mismatch;
- declare closure from compilation/PASS totals without activation witnesses and ledgers;
- debug from final aggregate counts instead of the first semantic divergence;
- leave temporary per-cycle diagnostics or stale README/API documentation behind.

---

This guide is complete when its rules can be applied without copying an older VIP, depending on a
specific simulator's callback convenience, or assigning one physical/logical event to two owners.
