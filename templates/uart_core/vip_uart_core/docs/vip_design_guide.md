# RapidVPI reusable VIP design guide

This document defines the default architecture for creating any reusable `vip_xxx` package in the RapidVPI/C++ coroutine ecosystem.

The goal is that a new VIP can be created from this guide without copying another `vip_xxx` repository as a reference. Existing VIPs such as `vip_axi`, `vip_axil`, `vip_axis`, `vip_mdio`, or `vip_eth` may still be inspected when available, but they are examples only. This guide is the policy source.

---

## Table of contents

- [1. Scope and intent](#1-scope-and-intent)
- [2. Core design principles](#2-core-design-principles)
- [3. Standard repository layout](#3-standard-repository-layout)
- [4. CMake integration model](#4-cmake-integration-model)
- [5. Namespaces, file naming, and public headers](#5-namespaces-file-naming-and-public-headers)
- [6. Protocol parameter objects](#6-protocol-parameter-objects)
- [7. Agent architecture](#7-agent-architecture)
  - [7.1 Master or initiator agents](#71-master-or-initiator-agents)
  - [7.2 Slave or responder agents](#72-slave-or-responder-agents)
  - [7.3 Monitor-only agents](#73-monitor-only-agents)
  - [7.4 Serial or pin-level protocol agents](#74-serial-or-pin-level-protocol-agents)
  - [7.5 Combined convenience agents](#75-combined-convenience-agents)
- [8. Scoreboard architecture](#8-scoreboard-architecture)
  - [8.1 Functional scoreboards](#81-functional-scoreboards)
  - [8.2 Rules scoreboards](#82-rules-scoreboards)
  - [8.3 Local DUT-specific scoreboards](#83-local-dut-specific-scoreboards)
- [9. Testcase architecture](#9-testcase-architecture)
- [10. RapidVPI phase discipline](#10-rapidvpi-phase-discipline)
- [11. Valid-ready and sampled-handshake discipline](#11-valid-ready-and-sampled-handshake-discipline)
- [12. Wide-vector and packed-field policy](#12-wide-vector-and-packed-field-policy)
- [13. Configuration knobs and additive evolution](#13-configuration-knobs-and-additive-evolution)
- [14. Error injection, backpressure, timing, and stress hooks](#14-error-injection-backpressure-timing-and-stress-hooks)
- [15. Memory models and protocol data models](#15-memory-models-and-protocol-data-models)
- [16. Logging, tracing, and debug controls](#16-logging-tracing-and-debug-controls)
- [17. Reset, lifecycle, and runner hooks](#17-reset-lifecycle-and-runner-hooks)
- [18. Documentation requirements](#18-documentation-requirements)
- [19. Creation checklist for a new `vip_xxx`](#19-creation-checklist-for-a-new-vip_xxx)
- [20. Example target shape for `vip_uart`](#20-example-target-shape-for-vip_uart)
- [21. Anti-patterns](#21-anti-patterns)

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

### 2.4 Additive extension only

New features must be added without breaking existing users whenever possible.

Default policy:

- keep old constructors working
- keep old method names working
- add new configuration structs or setter methods rather than changing existing signatures when the change is not trivial
- keep stricter scoreboard behavior opt-in unless every existing user requires it
- add new enum values rather than replacing old boolean behavior when the feature has more than two stable modes
- reset new per-case state in `reset_case()`
- document any changed default clearly in README

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

## 3. Standard repository layout

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

## 4. CMake integration model

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

---

## 5. Namespaces, file naming, and public headers

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

---

## 6. Protocol parameter objects

Every reusable VIP should have a central parameter/config object under `common/`.

Example:

```cpp
struct UartParams {
    unsigned data_bits = 8;
    unsigned stop_bits = 1;
    bool parity_enable = false;
    bool parity_odd = false;

    unsigned bit_ticks = 16;
    unsigned sample_tick = 8;

    bool lsb_first = true;
};
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

---

## 7. Agent architecture

An agent is a long-lived object owned by the project `Test` class. It uses RapidVPI coroutines to drive or monitor a protocol interface.

Typical construction:

```cpp
class Test : public vip::common::TestBase {
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
using RunTask = TestBase::RunTask;

RunTask agent(unsigned idx);
void attach_scoreboards(...);
```

For multi-port agents, constructors should accept either a count or explicit base names:

```cpp
XxxMaster(TestBase& tb, unsigned count, XxxParams params = XxxParams{});
XxxMaster(TestBase& tb, std::vector<std::string> names, XxxParams params = XxxParams{});
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
...
```

Never require testcases to hardcode every pin name if a stable prefix convention exists. If the protocol or DUT wrapper does not match the default convention, add a port-map/config object rather than hacking the testcase.

### 7.1 Master or initiator agents

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

RunTask wait_done(unsigned ticket);
bool is_done(unsigned ticket) const;

void set_trace_enable(const std::string& port, bool enable);
void set_backpressure_mode(...); // if the master also sinks responses
```

For protocols with split directions, keep internal split files:

```text
master_wr.cpp
master_rd.cpp
mst_coroutines_wr.cpp
mst_coroutines_rd.cpp
```

The high-level API belongs in `master.hpp`. Low-level pin mechanics belong in coroutine `.cpp` files.

### 7.2 Slave or responder agents

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

### 7.3 Monitor-only agents

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
RunTask agent(unsigned idx);

std::vector<XxxObservedTxn> get_history(const std::string& port) const;
void clear_history(const std::string& port);
```

### 7.4 Serial or pin-level protocol agents

For UART, MDIO, SPI, I2C-like, or other pin-level protocols, split line-level behavior cleanly:

- TX/source agent: drives a line or set of pins from queued frames
- RX/sink agent: samples a line or set of pins into observed frames
- monitor/checker: optional passive protocol decoder
- timing configuration: explicit bit/cycle timing in params
- error injection: optional and off by default

Serial agents must avoid hidden assumptions about absolute time. Use project clock cycles or explicit tick helpers.

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

### 7.5 Combined convenience agents

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

## 8. Scoreboard architecture

All reusable VIP scoreboards should report through `vip::common::Scoreboard`.

Typical construction:

```cpp
vip::common::Scoreboard scb;
vip::uart::ScbUartStream scb_uart(scb, uart_params);
vip::uart::ScbUartRules  scb_uart_rules(scb, uart_params);
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

### 8.1 Functional scoreboards

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
void observe_frame(const std::string& port, const UartFrame& frame, double t_ns);
void observe_error(const std::string& port, UartError err, double t_ns);
```

End-of-case checks should catch outstanding expectations and unexpected observations.

### 8.2 Rules scoreboards

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

### 8.3 Local DUT-specific scoreboards

When a checker depends on one DUT's semantics, keep it in the project, not reusable VIP.

Examples:

```text
src/scoreboard/scb_datamover_status.hpp
src/scoreboard/scb_decoder_routing.hpp
src/scoreboard/scb_uart_bridge_cmd_status.hpp
```

Local scoreboards may use reusable VIP observations, but should not push DUT-only meanings into `vip_xxx`.

---

## 9. Testcase architecture

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

## 10. RapidVPI phase discipline

The most important rule:

After an RO/read-observation await, do not schedule writes that are supposed to affect the same sampled RTL time slot.

Treat these as RO/read-observation awaits:

```cpp
co_await test.getCoRead(...);
co_await test.getCoChange(...);
co_await test.utils.clock(...);
```

Use safe write-phase transition helpers:

```cpp
co_await test.utils.clock_to_write(1, 0);
co_await test.utils.write_barrier();
```

Recommended pattern when a condition is observed and a drive must happen next cycle:

```cpp
while (true) {
    auto r = test.getCoRead(0);
    co_await r;

    const bool ready = (r.getNum(ready_net) & 1u) != 0u;

    co_await test.utils.clock();

    if (ready) {
        break;
    }
}

co_await test.utils.clock_to_write(1, 0);

{
    auto w = test.getCoWrite(0);
    w.write(valid_net, 1);
    co_await w;
}
```

Avoid:

```cpp
auto r = test.getCoRead(0);
co_await r;

auto w = test.getCoWrite(0);
w.write(sig, 1);
co_await w;
```

when the write is meant to affect the same edge just observed.

---

## 11. Valid-ready and sampled-handshake discipline

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
    auto w = test.getCoWrite(0);
    write_payload(w, payload);
    w.write(valid_net, 0);
    co_await w;
}

co_await test.utils.write_barrier();

{
    auto w = test.getCoWrite(0);
    write_payload(w, payload);
    w.write(valid_net, 1);
    co_await w;
}

while (true) {
    co_await test.utils.clock();

    auto r = test.getCoRead(0);
    co_await r;

    const bool valid = (r.getNum(valid_net) & 1u) != 0u;
    const bool ready = (r.getNum(ready_net) & 1u) != 0u;

    if (valid && ready) {
        break;
    }
}

co_await test.utils.clock_to_write(1, 0);

{
    auto w = test.getCoWrite(0);
    w.write(valid_net, 0);
    co_await w;
}
```

For non-valid-ready sampled protocols, apply the same idea:

- drive before the sampling edge
- sample only after the clock/tick that defines protocol observation
- do not modify outputs in the same phase used for observing inputs
- for line protocols, make bit center sampling explicit

---

## 12. Wide-vector and packed-field policy

Never use numeric RapidVPI writes for buses wider than 64 bits.

Policy:

- use string/hex or bit-vector helpers for wide writes
- preserve leading zeros
- preserve exact width
- prefer reusable helper types such as `XxxBitVec`, `AxiBitVec`, or protocol-specific byte vectors
- use `getNum()` only for signals that fit within 64 bits
- use `getHexStr()` or protocol helper reads for wider signals
- pack and decode fields exactly as documented
- never silently reverse MSB/LSB field order

A common reusable type pattern:

```cpp
using XxxDataWord = XxxBitVec;
using XxxMask     = XxxBitVec;
```

For protocols with byte streams, prefer byte vectors internally and convert to bit vectors only at the VPI boundary.

---

## 13. Configuration knobs and additive evolution

Every feature knob should have a clear owner and default.

Good knob categories:

```text
Protocol shape:
  data width, address width, ID width, parity enable, stop bits

Timing:
  bit_ticks, sample_tick, response delay, valid gap cycles

Backpressure:
  ready mode, bubble period, random pulse mode, scripted stalls

Error injection:
  response value, parity error, framing error, dropped packet, bad CRC

Checking:
  strict mode, warn-only mode, enable deep checks, verbose mode

Tracing:
  trace enable, output path, port filter, compression enable
```

Add knobs in a way that does not break old code:

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

## 14. Error injection, backpressure, timing, and stress hooks

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
glitch RX line for N ticks
```

Do not implement all stress hooks in version one if it causes bloat. Design the API so they can be added without breaking the first users.

---

## 15. Memory models and protocol data models

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

## 16. Logging, tracing, and debug controls

Use `vip_common` logging style when available.

Logging policy:

- keep normal logs useful and not noisy
- verbose logs must be controlled by `set_verbose()`
- bring-up logs should be guardable or removable
- include protocol/agent name, port name, and simulation time when useful
- failure messages should explain expected vs observed values
- do not print huge payloads by default

Trace policy:

- tracing is optional
- tracing must be configurable per port when practical
- trace output path must be configurable
- trace records should be structured enough to post-process
- trace enable should default off unless the trace is tiny

Common trace object style:

```cpp
struct TraceCfg {
    bool enable = false;
    std::string out_path;
    bool compress = true;
};
```

---

## 17. Reset, lifecycle, and runner hooks

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

## 18. Documentation requirements

Every reusable VIP must have a top-level `README.md` with a clickable table of contents.

Top-level README should include:

- what the VIP provides
- dependencies
- how to instantiate in `Test`
- how to configure params
- how to attach scoreboards
- common testcase examples
- known limits
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
- end-of-case behavior

For a new VIP, initial documentation can be compact, but must still make the API usable without reading all `.cpp` files.

---

## 19. Creation checklist for a new `vip_xxx`

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

3. Define the parameter object.
   - widths
   - timing
   - feature enables
   - derived helpers

4. Define agents.
   - master/initiator, slave/responder, source/sink, tx/rx, monitor
   - public enqueue APIs
   - completion APIs
   - port naming convention
   - scoreboards attachment points

5. Define scoreboards.
   - functional expected vs observed
   - rules/protocol legality
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

8. Define minimum compile smoke.
   - headers include cleanly
   - object targets compile
   - no full simulation unless explicitly requested by user

9. Define extension hooks.
   - future modes as enums
   - one-shot injection APIs
   - history capture
   - trace config
   - backwards-compatible constructors

10. Keep version one small.
   - Make the skeleton correct.
   - Implement the first real path.
   - Add future hooks only where they avoid API churn.
   - Do not bloat first version with every possible feature.

---

## 20. Example target shape for `vip_uart`

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
    unsigned bit_ticks = 16;
    unsigned sample_tick = 8;
    bool idle_high = true;
};

struct UartFrame {
    std::uint8_t data = 0;
    bool parity_error = false;
    bool framing_error = false;
    double start_time_ns = 0.0;
    double end_time_ns = 0.0;
};
```

Suggested TX API:

```cpp
class UartTx {
public:
    using RunTask = TestBase::RunTask;

    UartTx(TestBase& tb,
           std::vector<std::string> ports,
           UartParams params = UartParams{},
           std::string reset_name = "rst_n");

    void attach_scoreboards(ScbUartStream* stream, ScbUartRules* rules = nullptr);

    RunTask agent(unsigned idx);

    unsigned enqueue_byte(const std::string& port, std::uint8_t data);
    unsigned enqueue_bytes(const std::string& port, const std::vector<std::uint8_t>& data);
    RunTask wait_done(unsigned ticket);

    void set_inter_frame_gap_ticks(const std::string& port, unsigned ticks);
    void arm_next_framing_error(const std::string& port);
};
```

Suggested RX API:

```cpp
class UartRx {
public:
    using RunTask = TestBase::RunTask;

    UartRx(TestBase& tb,
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

    void observe_frame(const std::string& port, const UartFrame& frame);
};
```

Important first-version constraint:

Do not implement RTS/CTS by changing the stable TX/RX API later. Add it as optional params and optional methods when needed.

---

## 21. Anti-patterns

Avoid these:

- creating a local protocol driver in every testcase
- creating a new VIP that duplicates an existing `ext/vip_xxx`
- hardcoding one DUT's signal names into reusable VIP
- making testcases manually drive valid-ready buses when an agent exists
- putting DUT command/status layout into a protocol-generic VIP
- changing existing constructors when a setter or config object would preserve compatibility
- turning strict new checks on by default and breaking old tests unexpectedly
- using `getNum()` for wide buses
- using bus-equality waits that can stall
- writing after an RO await and expecting the write to affect the same sampled edge
- adding random behavior without a seed
- requiring a previous `vip_xxx` repo as reference when this guide already defines the style
- creating nested git submodules inside `ext/`
- hiding all implementation in one huge header
- making README stale after adding APIs
