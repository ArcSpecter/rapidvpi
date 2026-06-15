# RapidVPI VIP Cosim Project Design Guide - Compact

## Table of Contents

1. [Purpose](#1-purpose)
2. [Non-Negotiable Rules](#2-non-negotiable-rules)
3. [Required Project Layout](#3-required-project-layout)
4. [File Responsibilities](#4-file-responsibilities)
5. [Reusable VIP Policy](#5-reusable-vip-policy)
6. [Testcase Organization](#6-testcase-organization)
7. [README Policy](#7-readme-policy)
8. [RapidVPI Phase Discipline](#8-rapidvpi-phase-discipline)
9. [Clock Helpers](#9-clock-helpers)
10. [Valid-Ready Drive Discipline](#10-valid-ready-drive-discipline)
11. [Wide-Vector Policy](#11-wide-vector-policy)
12. [Clock, Reset, and POR](#12-clock-reset-and-por)
13. [Scoreboard Policy](#13-scoreboard-policy)
14. [Agent Policy](#14-agent-policy)
15. [Command and Status Helpers](#15-command-and-status-helpers)
16. [Logging and Debug](#16-logging-and-debug)
17. [CMake and Build Policy](#17-cmake-and-build-policy)
18. [Codex Implementation Checklist](#18-codex-implementation-checklist)
19. [Common Failure Patterns](#19-common-failure-patterns)
20. [Final Acceptance Checklist](#20-final-acceptance-checklist)

---

## 1. Purpose

This guide defines the required structure and coding rules for RapidVPI-based VIP/cosim projects.

It applies to projects such as:

```text
vip_axi_datamover
vip_axi_mdio_master
vip_axis_rt
vip_mac_1g
vip_<new_ip>
```

The goal is to let a human engineer, Codex, or another LLM build or modify a VIP project without inventing local one-off patterns.

A VIP project must be a structured verification environment, not a random pile of C++ coroutines.

---

## 2. Non-Negotiable Rules

1. `test.cpp` stays thin and only orchestrates project setup.
2. Testcase bodies live under `src/cases`.
3. Shared testcase helpers live in `src/cases/tc_utils.hpp/.cpp`.
4. Register each testcase from its own `tc_xxx.hpp`.
5. Reuse `ext/vip_common`, `ext/vip_axi`, `ext/vip_axis`, or other `ext/vip_xxx` before creating local agents.
6. Create local agents/scoreboards only for project-specific behavior.
7. Every README must have a clickable table of contents.
8. After RO awaits, do not assume a following write affects the already-sampled clock edge.
9. For valid-ready channels, keep payload stable before and during the accepting edge.
10. For buses wider than 64 bits, use hex-string/bit-vector APIs, not numeric writes.

---

## 3. Required Project Layout

Use this default layout:

```text
vip_<ip_name>/
  CMakeLists.txt
  README.md

  src/
    init.cpp
    test.hpp
    test.cpp
    pindefs.hpp

    cases/
      README.md
      CMakeLists.txt
      tc_utils.hpp
      tc_utils.cpp
      tc_generic.hpp
      tc_generic.cpp
      tc_<feature>.hpp
      tc_<feature>.cpp

  ext/
    vip_common/
    vip_axi/
    vip_axis/
    vip_axil/
    vip_mdio/
    vip_<protocol>/
```

For larger IPs, split tests by feature:

```text
tc_generic.*
tc_reset.*
tc_backpressure.*
tc_alignment.*
tc_status_errors.*
tc_ordering.*
tc_corners.*
```

Do not place reusable protocol behavior under `src`. Reusable protocol behavior belongs in `ext/vip_xxx`.

---

## 4. File Responsibilities

### 4.1 `src/test.hpp`

Declares the project `Test` class.

It may contain:

- DUT name support
- common utilities
- clock/reset/POR agents
- protocol params
- reusable protocol agents
- reusable scoreboards
- runner
- project-level resources shared by cases

Keep it declarative. Do not put testcase bodies here.

### 4.2 `src/test.cpp`

`test.cpp` must be thin orchestration.

Allowed:

```cpp
runner.register_tasks();
agent.attach_scoreboards(...);
scb.enable_print_info(true);
runner.set_before_case_hook(...);
runner.set_after_case_hook(...);
register_tc_generic(*this, {"smoke", "baseline"}, true, "tc_generic");
```

Not allowed:

```cpp
co_await send_command(...);
co_await drive_packet(...);
co_await expect_status(...);
```

That belongs in `tc_xxx.cpp` or `tc_utils`.

### 4.3 `src/init.cpp`

Owns RapidVPI factory registration and net registration.

It should implement:

```cpp
extern "C" void userRegisterFactory();
void Test::initDutName();
void Test::initNets();
```

`initNets()` must register every DUT signal that VIP touches using `addNet(name, width)`.

Never guess widths. Put signal names and widths in `pindefs.hpp` or protocol params.

### 4.4 `src/pindefs.hpp`

Single source of truth for:

- DUT name
- clock/reset net names
- protocol base names
- DUT signal names
- data widths
- ID widths
- command/status widths
- local opcode/flag constants

Avoid scattered string literals in agents and tests.

### 4.5 `src/cases/tc_utils.hpp/.cpp`

Use `tc_utils` for project-specific helpers only.

Allowed:

- project command packing/unpacking
- project status unpacking
- byte/payload generation
- small compare helpers
- local reset helper
- small project-specific sideband helpers

Not allowed:

- full AXI/AXIS/MDIO agents
- generic protocol scoreboards
- generic rule checkers
- reusable protocol behavior

If a helper is reusable across projects, move it to `ext/vip_common` or `ext/vip_xxx`.

---

## 5. Reusable VIP Policy

Always inspect and reuse `ext` before writing local infrastructure.

Standard packages:

```text
ext/vip_common
  runner
  common scoreboard orchestration
  clock/POR/reset agents
  logging
  common utilities

ext/vip_axi
  AXI params
  AXI bit vectors
  AXI VPI wide-vector helpers
  AXI slave/memory model
  AXI read/write scoreboards
  AXI read/write rule checkers

ext/vip_axis
  AXIS params
  AXIS bit vectors
  AXIS master source
  AXIS slave sink
  AXIS stream scoreboard
  AXIS rule checker
```

Create or extend protocol packages when behavior is reusable:

```text
ext/vip_axil
ext/vip_mdio
ext/vip_eth
ext/vip_<protocol>
```

Local project code is acceptable only for IP-specific semantics, such as a custom command/status word or a proprietary descriptor format.

---

## 6. Testcase Organization

All testcases live in:

```text
src/cases/
```

Each testcase uses:

```text
tc_<name>.hpp
tc_<name>.cpp
```

The `.hpp` owns registration. The `.cpp` owns the coroutine body.

A testcase should read like a test plan:

```cpp
TestBase::RunUserTask tc_generic(Test& test) {
    co_await tc_local_reset(test);

    build_payloads();
    setup_expected_scoreboard_entries();
    co_await send_command(test, cmd);
    co_await drive_traffic(test, payload);
    co_await wait_for_outputs(test);
    check_status_and_memory();

    co_return;
}
```

Keep protocol mechanics inside agents and helpers. Keep testcase intent visible.

---

## 7. README Policy

Every README must have a clickable TOC.

Required READMEs:

```text
README.md
src/cases/README.md
ext/vip_xxx/README.md
```

Top-level `README.md` must include DUT purpose, supported interfaces, project structure, reusable VIP dependencies, testcase list, build/run instructions, known limitations, and how to add a testcase.

`src/cases/README.md` must include cases folder purpose, testcase summary table, per-test intent, helper split, and how to add cases.

`ext/vip_xxx/README.md` must include protocol scope, agents, scoreboards, params/configuration, limitations, examples, and integration checklist.

---

## 8. RapidVPI Phase Discipline

RapidVPI uses coroutine awaiters for simulator interaction:

```cpp
auto w = test.getCoWrite(0);
auto r = test.getCoRead(0);
auto c = test.getCoChange(clk, 1);
```

### 8.1 WO and RO

WO means write-oriented interaction through `getCoWrite()`.

RO means read-only observation through `getCoRead()`, `getCoChange()`, or clock helpers based on change/read observation.

Core rule:

```text
After any RO await, do not assume a following getCoWrite(0) affects the same RTL sampling edge that was just observed.
```

Safe mental model:

```text
RO/clock await returns after simulator observation.
A following write may occur in a later simulation region of the same timestamp.
RTL always_ff sampling may already have happened.
```

Same timestamp does not mean same simulation region.

A waveform can show final settled values at a timestamp while RTL sampled earlier values at the active clock edge.

### 8.2 Do not mix read decision and write response unsafely

Avoid patterns that assume a write after RO changes the already-sampled edge:

```cpp
auto rd = test.getCoRead(0);
rd.read(ready);
co_await rd;

if (rd.getNum(ready)) {
    auto w = test.getCoWrite(0);
    w.write(valid, 0);
    co_await w;
}
```

This is okay only if the write is explicitly intended for a future edge.

### 8.3 `getCoChange()` usage

Use `getCoChange()` only when a future transition is required or guaranteed.

Do not use `getCoChange()` when the value may already be true and no future transition is guaranteed. Use read-first or a `waitFor()` helper instead.

---

## 9. Clock Helpers

Prefer shared helpers from `vip_common::CommonUtils`:

```cpp
co_await test.utils.clock();
co_await test.utils.clock(4, 1);
co_await test.utils.clock_to_write(1, 0);
co_await test.utils.clock_to_write(1, 1);
co_await test.utils.write_barrier();
```

### 9.1 `clock()`

`clock()` waits for clock observation. Treat it as an RO await.

```cpp
co_await test.utils.clock();
co_await test.utils.clock(4, 1);
```

The second argument selects the edge value:

```text
edge = 1: rising/high edge
edge = 0: falling/low edge
```

After `clock()`, do not assume a following `getCoWrite(0)` affects the edge that just happened.

### 9.2 `clock_to_write(cycles, edge)`

`clock_to_write(cycles, edge)` means:

1. wait for the requested clock edge
2. pass through a write-safe barrier
3. resume in a phase intended for driving signals after that observed edge

This does not mean the just-observed edge samples the new values.

### 9.3 `clock_to_write(1, 0)`

Use this to drive after a falling edge, typically during the low phase, so the DUT can sample on the next rising edge.

```text
falling edge:
  VIP resumes and drives valid/data

next rising edge:
  RTL samples valid/data
```

This is a simple safe policy for command/control channels sampled on rising edge.

### 9.4 `clock_to_write(1, 1)`

Use this to drive after a rising edge for next-cycle sampling.

It does not make the current rising edge sample the values.

Correct model:

```text
140 ns rising edge:
  RTL samples old values

140 ns after-edge write phase:
  VIP drives new valid/data

150 ns next rising edge:
  RTL samples new valid/data
```

Use `clock_to_write(1, 1)` only when next-cycle drive after a rising edge is intended.

Do not use it when the intent is for the current rising edge to sample the new values. That is already too late.

### 9.5 `write_barrier()`

`write_barrier()` separates write phases without requiring another full clock edge.

Use it when payload and qualifier must not become visible in an unsafe order.

Common pattern:

```cpp
{
    auto w = test.getCoWrite(0);
    axi::write_bitvec(w, cmd_data_net, cmd_word, CMD_W);
    w.write(cmd_valid_net, 0);
    co_await w;
}

co_await test.utils.write_barrier();

{
    auto w = test.getCoWrite(0);
    axi::write_bitvec(w, cmd_data_net, cmd_word, CMD_W);
    w.write(cmd_valid_net, 1);
    co_await w;
}
```

This is especially important for wide vectors written through string/hex APIs.

---

## 10. Valid-Ready Drive Discipline

For any DUT-sampled valid-ready channel:

```text
DATA must be stable before the edge where VALID && READY is sampled.
VALID must stay high until a real sampled handshake edge occurs.
DATA must remain stable while VALID is high.
VALID/DATA may only be dropped or changed after the accepting edge.
```

Immediate-ready channels are dangerous. If `ready` is already high, the first cycle where `valid` is seen high can immediately accept the transfer.

### 10.1 Safe command/control drive pattern

Use this for command/status-style valid-ready payloads, especially when `ready` may already be high:

```cpp
co_await test.utils.clock_to_write(1, 0);

{
    auto w = test.getCoWrite(0);
    axi::write_bitvec(w, cmd_data_net, cmd_word, CMD_W);
    w.write(cmd_valid_net, 0);
    co_await w;
}

co_await test.utils.write_barrier();

{
    auto w = test.getCoWrite(0);
    axi::write_bitvec(w, cmd_data_net, cmd_word, CMD_W);
    w.write(cmd_valid_net, 1);
    co_await w;
}
```

Then wait for a sampled handshake edge:

```cpp
while (true) {
    auto rd = test.getCoRead(0);
    rd.read(rst_n);
    rd.read(cmd_valid_net);
    rd.read(cmd_ready_net);
    rd.read(cmd_data_net);
    co_await rd;

    const bool rst_high = rd.getNum(rst_n) != 0u;
    const bool accepted = rst_high &&
                          (rd.getNum(cmd_valid_net) != 0u) &&
                          (rd.getNum(cmd_ready_net) != 0u);

    co_await test.utils.clock();

    if (!rst_high || accepted) {
        break;
    }

    co_await test.utils.clock_to_write(1, 0);
}
```

Clear after the accepting edge:

```cpp
co_await test.utils.clock_to_write(1, 0);

{
    auto w = test.getCoWrite(0);
    w.write(cmd_valid_net, 0);
    axi::write_bitvec(w, cmd_data_net, axi::AxiBitVec::zero(CMD_W), CMD_W);
    co_await w;
}
```

### 10.2 Do not compress unsafe command drives

Avoid this for immediately-ready command channels:

```cpp
auto w = test.getCoWrite(0);
axi::write_bitvec(w, cmd_data_net, cmd_word, CMD_W);
w.write(cmd_valid_net, 1);
co_await w;
```

It can produce a waveform that looks legal after the timestamp settles while RTL sampled the old value at the active edge.

### 10.3 Inactive-edge drive is not mandatory

Driving on the inactive edge is a simple safe policy, not a universal requirement.

The real requirement is:

```text
payload and valid must settle before the RTL edge that accepts the transaction
```

Driving after a rising edge can also be correct if acceptance is intended on the next rising edge.

---

## 11. Wide-Vector Policy

### 11.1 Writes

Do not use numeric writes for vectors wider than 64 bits.

Bad:

```cpp
w.write(s2mm_cmd_data, value_u64);
```

Good:

```cpp
w.write(s2mm_cmd_data, cmd_hex, 16);
```

Preferred:

```cpp
axi::write_bitvec(w, s2mm_cmd_data, cmd_word, S2MM_CMD_W);
```

### 11.2 Reads

Do not use `getNum()` for vectors wider than 64 bits.

Use:

```cpp
rd.getHexStr(net);
rd.getBinStr(net);
axi::read_bitvec(rd, net, width);
```

`getNum()` is fine for scalar and small fields that fit inside 64 bits.

### 11.3 Fixed-width formatting

Command/status helpers must preserve leading zeros.

Example:

```text
S2MM_CMD_W = 152 bits = 38 hex digits
MM2S_CMD_W = 200 bits = 50 hex digits
```

Do not trim leading zeros from packed command/status words.

### 11.4 Bit packing

Pack fields in the same order as the RTL table.

If RTL uses MSB-first table order:

```systemverilog
cmd_data = {
  opcode,
  flags,
  tag,
  group_tag,
  addr,
  btt,
  extra_fields
};
```

then C++ must append fields in the same order.

Do not silently reverse field order.

---

## 12. Clock, Reset, and POR

Use reusable clock and POR/reset agents from `vip_common`.

Each testcase should start with a deterministic reset helper:

```cpp
co_await tc_local_reset(test);
```

A local reset helper should:

1. clear reusable memories and packet captures
2. reset scoreboard/agent local state where needed
3. start the clock agent
4. drive DUT inputs to safe defaults
5. assert reset
6. deassert reset
7. wait several clean cycles before traffic

Do not start tests with stale agent memory, stale packets, or stale scoreboard expectations.

---

## 13. Scoreboard Policy

Use `vip_common::Scoreboard` as the project-level scoreboard aggregator.

Use protocol-specific scoreboards for protocol behavior:

```text
vip_axi::ScbAxiWrite
vip_axi::ScbAxiRead
vip_axi::ScbAxiWriteRules
vip_axi::ScbAxiReadRules
vip_axis::ScbAxisStream
vip_axis::ScbAxisStreamRules
```

Runner hooks should reset per-case state before each case and finalize checks after each case.

Create a local scoreboard only for IP-specific semantics such as descriptor retirement, custom command/status matching, or custom error aggregation.

Do not create local scoreboards for plain AXI or AXIS behavior.

---

## 14. Agent Policy

Agents own protocol mechanics. Testcases own intent.

Preferred testcase style:

```cpp
test.axis_src.enqueue_packet_words(...);
co_await test.axis_src.wait_done(...);
```

Avoid manually toggling every protocol signal inside testcase bodies when reusable agents exist.

Agents must:

- keep data stable while valid is high
- handle ready backpressure
- use wide-vector helpers for wide payloads
- avoid bus-equality waits that can stall
- expose clear APIs to testcases
- optionally attach to scoreboards
- allow base-name driven signal binding

Generic agents go in `ext/vip_xxx`.

Project-only tiny shims may live in `tc_utils`. Larger local-only agents may live under `src/agents`, but only when they truly have no reuse value.

---

## 15. Command and Status Helpers

Command/status buses may be local direct valid-ready payload buses. For those, local `tc_utils` helpers are acceptable.

Command helpers must:

- pack fields exactly according to RTL table order
- log exact command hex during bring-up
- use wide-vector write APIs
- drive payload before valid when ready may already be high
- hold payload stable until handshake
- clear only after handshake
- optionally read back payload before/at handshake for debug

Status helpers must:

- drive status ready safely
- sample status only when valid is high
- unpack fields according to RTL table order
- compare status code, flags, tags, byte counts, and response summaries

Keep debug logs concise and remove/guard noisy temporary logs before regression.

---

## 16. Logging and Debug

Use shared logging helpers from `vip_common`.

Permanent logs should include:

- testcase start/end
- reset start/end
- major transaction start/end
- status completions
- final scoreboard summary

Temporary bring-up logs may include raw command hex, FIFO push/pop debug, payload sample points, immediate vs strobe samples, and per-cycle protocol internals.

When waveform and RTL logs disagree, trust sampled RTL debug prints at the relevant `always_ff` edge first.

A waveform can show final settled values for a timestamp while RTL sampled earlier values at the active clock edge.

---

## 17. CMake and Build Policy

A VIP project builds a shared library loaded by simulator VPI/PLI.

Top-level CMake should:

- require C++23
- require `VPI_INCLUDE_DIR`
- find installed `rapidvpi`
- include `src` and `ext`
- add reusable VIP subdirectories
- link `rapidvpi::rapidvpi.vpi`
- link simulator VPI library if needed

`src/cases/CMakeLists.txt` should add testcase sources to the main target, not create unrelated executables.

---

## 18. Codex Implementation Checklist

Before modifying or creating a VIP project, Codex must check:

- top-level `CMakeLists.txt`
- `src/test.hpp`
- `src/test.cpp`
- `src/init.cpp`
- `src/pindefs.hpp`
- `src/cases`
- available `ext/vip_xxx` packages
- relevant READMEs

Before creating an agent or scoreboard, answer:

```text
Is this protocol behavior reusable across projects?
```

If yes, extend `ext/vip_xxx`. If no, keep it local under `src/cases/tc_utils` or a project-local agent.

Codex must preserve:

- thin `test.cpp`
- registration wrappers in `tc_xxx.hpp`
- sequence bodies in `tc_xxx.cpp`
- command/status helpers in `tc_utils`
- RO/WO phase safety
- wide-vector rules
- README TOCs

---

## 19. Common Failure Patterns

### 19.1 Command FIFO samples zero

Symptom:

```text
cmd_valid=1
cmd_ready=1
waveform later shows nonzero command
RTL FIFO debug shows push=1 in=0x0
```

Likely causes:

- valid became visible at accepting edge before wide command data was stable
- write occurred in later simulation region than RTL sampling
- numeric write truncated wide command to 64 bits

Fix:

- use bit-vector/hex-string write helper
- drive data before valid
- use `write_barrier()`
- hold data until after accepted edge

### 19.2 `getCoChange()` stalls

Cause: signal already had desired value before `getCoChange()` was armed.

Fix: read first, or use a `waitFor()` style helper.

### 19.3 Scoreboard checks too early

Symptom:

```text
observed beats < expected
```

Likely causes:

- testcase checks before full transaction completed
- RTL splits transaction but expectation assumes one large transaction
- scoreboard finalizes too aggressively

Fix:

- wait for real completion condition
- model split bursts if RTL emits them
- register expectations before traffic starts

### 19.4 AXIS data mismatch

Likely causes:

- expected data order wrong
- byte-endianness mismatch
- memory preload order mismatch
- packet boundary/TLAST mismatch
- sink checks wrong packet/beat

Fix:

- dump captured packet words
- compare beat indices
- verify byte order helpers
- verify TLAST boundaries

### 19.5 BRESP for unknown transaction

Likely causes:

- write scoreboard expected model does not match AW/W/B behavior
- BID/ID mismatch
- scoreboard consumed transaction too early
- response arrives before expectation registered

Fix:

- inspect observed AW/W/B queues
- verify expectations are registered before traffic
- model split bursts
- verify ID width and encoded ID policy

---

## 20. Final Acceptance Checklist

Before calling a VIP project ready:

- `test.cpp` is thin orchestration only.
- `init.cpp` owns factory and net registration.
- `pindefs.hpp` owns signal names and widths.
- `src/cases` owns testcase bodies.
- `tc_xxx.hpp` owns registration wrappers.
- `tc_xxx.cpp` owns sequence body only.
- `tc_utils` owns only project-specific helpers.
- Reusable behavior is in `ext/vip_xxx`.
- Every README has a clickable TOC.
- Every new test is documented.
- Wide vectors use string/bit-vector APIs.
- Valid-ready helpers obey data-stable-before-sampled-edge discipline.
- No helper assumes a post-RO write changes an already-sampled edge.
- Clock/reset sequence is deterministic.
- Scoreboard reset/check lifecycle is in runner hooks.
- Protocol rule checkers are enabled for regression.
- Temporary debug logs are removed or guarded.
- Project builds cleanly.
- Simulator loads the `.so` through VPI/PLI.
- At least one baseline smoke testcase passes.
