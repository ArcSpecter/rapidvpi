# AGENTS.md

## Purpose

This repository is a RapidVPI-based VIP/cosim project.

When creating a new reusable VIP package or agent family such as `vip_uart`, `vip_spi`, `vip_i2c`, or any other `vip_xxx`, follow the standalone reusable VIP creation guide first:

```text
./docs/vip_design_guide.md
```

That guide defines the default `vip_xxx` repository structure, agent/scoreboard split, configuration-hook style, additive-extension policy, and CMake/documentation expectations. Do not require copying another existing `vip_xxx` repository as reference when creating a new VIP; existing VIPs may be inspected only as optional examples when available.


Before making structural changes, creating new testcases, adding agents, changing scoreboard behavior, or modifying RapidVPI coroutine flow, follow the shared RapidVPI VIP design guide:

```text
ext/vip_common/docs/vip_cosim_design_guide_compact.md
```

If this repository is `vip_common` itself, use:

```text
docs/vip_cosim_design_guide_compact.md
```

Treat that guide as canonical for project layout, testcase organization, reusable VIP policy, RapidVPI RO/WO phase discipline, valid-ready drive discipline, wide-vector handling, scoreboards, agents, logging, and README rules.

For the IP/DUT-specific operating manual, visible interface behavior, address map, register map, command/status formats when present, opcode/flag definitions when present, response semantics, operating modes, and latest behavior expected by tests, use:

```text
docs/user_guide.md
```

Treat `docs/user_guide.md` as the canonical project-local source for the DUT being tested. Do not infer DUT behavior from stale testcase code when `docs/user_guide.md` defines the behavior.

---

## Mandatory Rules

### Simulation Policy

Do not run RTL simulations unless explicitly instructed with a specific simulation command.

For verification after code changes, only compile/build the cosim project and confirm the shared library is created successfully, for example the RapidVPI `.so` target. The user runs RTL simulations manually and will provide simulator results/logs when needed.

When compiling, use the configured release build directory, normally `cmake-build-release`, and build the VIP shared-library target. Example shape:

```text
cmake --build /path/to/vip_project/cmake-build-release --target <vip_target_name> -j <jobs>
```

### Project Structure

- Keep `src/test.cpp` thin. It is only for project orchestration, agent setup, scoreboard hookup, runner hooks, and testcase registration.
- Keep testcase bodies under `src/cases`.
- Keep shared testcase helpers in `src/cases/tc_utils.hpp` and `src/cases/tc_utils.cpp`.
- Keep testcase registration wrappers in `tc_xxx.hpp`.
- Keep testcase coroutine bodies in `tc_xxx.cpp`.
- Keep signal names, widths, local protocol constants, register constants, command/status constants, and DUT-specific constants in `src/pindefs.hpp` or protocol parameter/config objects.
- Keep RapidVPI factory and net registration in `src/init.cpp`.
- Keep DUT behavior documentation in `docs/user_guide.md`. Update it when visible DUT behavior, register fields, command/status fields, opcodes, flags, status formats, operating modes, address maps, or completion rules change.

### IP/DUT User Guide

- Before changing tests or scoreboards that depend on DUT behavior, inspect `docs/user_guide.md`.
- Use `docs/user_guide.md` as the source of truth for project-local DUT behavior, including:
  - interface-level behavior
  - address maps and register maps
  - command/status layout and bit fields, when present
  - opcode, flag, and status meanings, when present
  - valid-ready handshake expectations
  - packet, transaction, or register-access operating modes
  - response and completion semantics
  - ordering, routing, splitting, aggregation, and response-folding policy, when applicable
  - project-specific constraints that are not part of generic protocol behavior
- If code and `docs/user_guide.md` disagree, do not silently choose the code. Flag the mismatch and either update the code to match the guide or update the guide if the guide is stale.
- Do not duplicate large chunks of `docs/user_guide.md` into testcase comments. Reference it and keep tests focused on intent.

### Reuse Policy

- Reuse `ext/vip_xxx` packages before creating local agents or scoreboards.
- Extend `ext/vip_xxx` when behavior is protocol-generic and reusable.
- Keep local agents or local scoreboards only for project-specific behavior that has no obvious reuse value.
- Do not duplicate AXI, AXI-Lite, AXIS, MDIO, Ethernet, or other protocol mechanics locally if a reusable VIP package already provides them.
- Keep DUT-specific interpretation project-local unless it is clearly reusable across multiple projects.

### RapidVPI Phase Discipline

- Treat `getCoRead()`, `getCoChange()`, and `test.utils.clock()` as RO/read-observation awaits.
- After any RO await, do not assume a following `getCoWrite(0)` affects the already-sampled RTL clock edge.
- Same timestamp does not mean same simulation region. A waveform may show final settled values while RTL sampled earlier values at the active edge.
- Use `test.utils.clock_to_write()` and `test.utils.write_barrier()` when crossing from observation to driving.
- Use `clock_to_write(1, 0)` to drive in the low phase for next rising-edge sampling.
- Use `clock_to_write(1, 1)` only when driving after a rising edge for the next sampling edge. It is too late for the just-observed rising edge.

### Valid-Ready Discipline

For DUT-sampled valid-ready channels:

- Drive payload before asserting valid when ready may already be high.
- Keep payload stable while valid is high.
- Keep valid high until a real sampled `valid && ready` clock edge occurs.
- Clear or change valid/payload only after the accepting edge.
- Do not compress immediately-ready command/control transfers into one unsafe post-RO write.

Preferred valid-ready drive pattern:

```cpp
co_await test.utils.clock_to_write(1, 0);

{
    auto w = test.getCoWrite(0);
    axi::write_bitvec(w, data_net, data_word, DATA_W);
    w.write(valid_net, 0);
    co_await w;
}

co_await test.utils.write_barrier();

{
    auto w = test.getCoWrite(0);
    axi::write_bitvec(w, data_net, data_word, DATA_W);
    w.write(valid_net, 1);
    co_await w;
}
```

### Wide-Vector Policy

- Never use numeric writes for buses wider than 64 bits.
- Use string/hex or bit-vector helpers for wide writes.
- Prefer helpers such as `axi::write_bitvec()` and `axi::read_bitvec()` when available.
- Do not use `getNum()` for wide vectors.
- Preserve fixed width and leading zeros for packed command/status/register words.
- When a DUT uses packed command/status/register fields, pack and decode them exactly as documented in `docs/user_guide.md`. Do not silently reverse MSB/LSB order.

### Scoreboards and Agents

- Use `vip_common::Scoreboard` as the project-level scoreboard aggregator.
- Use protocol scoreboards from `ext/vip_xxx` for protocol behavior.
- Add local scoreboards only for DUT-specific semantics.
- Agents own protocol mechanics. Testcases own test intent.
- Testcases should call agent APIs, not manually toggle full protocols when reusable agents exist.
- Keep reusable VIP scoreboards backward-compatible by default. New stricter behavior should be opt-in unless all existing users require it.
- Keep DUT-specific scoreboard policy aligned with `docs/user_guide.md`.

### Documentation

- Every README must have a clickable table of contents.
- Update the nearest README when adding a testcase, agent, scoreboard, helper, or meaningful behavior.
- `src/cases/README.md` must describe each testcase and its intent.
- `ext/vip_xxx/README.md` must describe reusable agent/scoreboard APIs and limitations.
- `docs/user_guide.md` must stay current with the latest DUT behavior expected by tests.

### Build and Verification

- Keep CMake organized around the main shared library target.
- `src/cases/CMakeLists.txt` should add testcase sources to the main target.
- Do not create unrelated executables for testcase files.
- After changes, build the VIP shared library when a configured `cmake-build-release` directory is available.
- Use the local target name from `CMakeLists.txt`; do not guess a target from another VIP project.
- Do not run RTL simulation unless explicitly instructed.
- Report exactly what was changed and what was not verified.

---

## Before Editing Checklist

Before editing files, inspect the files that exist in the current repository from this list:

```text
README.md
docs/user_guide.md
src/test.hpp
src/test.cpp
src/init.cpp
src/pindefs.hpp
src/cases/README.md
src/cases/tc_utils.hpp
src/cases/tc_utils.cpp
src/cases/tc_*.hpp
src/cases/tc_*.cpp
ext/vip_common/docs/vip_cosim_design_guide_compact.md
```

Also inspect relevant protocol VIP packages under `ext/`, for example:

```text
ext/vip_common
ext/vip_axi
ext/vip_axil
ext/vip_axis
ext/vip_mdio
ext/vip_eth
```

Do not make broad structural changes until you understand the current project layout, the DUT-specific behavior documented in `docs/user_guide.md`, and the available reusable VIP APIs.

---

## Coding Style

- Prefer clear, explicit helper names.
- Keep testcase `.cpp` files readable and compact.
- Push complexity into reusable agents, scoreboards, or `tc_utils`.
- Avoid clever coroutine patterns that obscure RO/WO ordering.
- Avoid bus-equality waits that may stall.
- Use bit masks for readiness/condition checks where applicable.
- Keep debug logging useful but not permanently noisy.
- Temporary bring-up logs must be easy to remove or guard.
- Keep DUT-specific packing, decoding, routing, and response helpers obvious and traceable back to `docs/user_guide.md`.

---

## Output Expectations

When completing a task:

- Summarize changed files.
- Explain whether changes are project-local or reusable VIP changes.
- State whether build was run.
- State whether simulation was not run, unless explicitly requested and actually completed.
- State any remaining risks or assumptions.
- If files were not verified by simulation, say so directly.
- If the change depends on DUT behavior, state whether `docs/user_guide.md` was consulted or updated.
