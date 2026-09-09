# RTL Rework Result

RESULT: PLANNED_CHANGE_COMPLETE

Mode: `rtl_rework / PLANNED_CHANGE`

Selected change request: `./rtl_refactor.md`

## Project mapping

- Project classification: `reusable_ip`
- RTL project: `rtl_uart_core` version `0.0.1`
- Real synthesizable top: `uart_core`
- Simulation top: `dut_wrapper`
- DUT instance: `dut_wrapper.u_dut`
- Paired VIP project: `vip_uart_core`
- Paired Verilator VIP target: `verilator_uart_core`
- RTL dependencies: none; eight local synthesizable sources remain in dependency order
- Active compile-time mode: no interface-selection define; both simulators use the unconditional port set and parameter defaults
- Wrapper-owned clocks: `clk`, default full period `64'd10000` in 1 ps ticks
- DUT-owned/output clocks: none

## Implemented changes

- Added root `dut_wrapper.sv` with the canonical `sim_native_clock_gen`, one native `clk` control group, the standard Verilator keepalive, exact parameter/functional-port pass-through, and one `uart_core u_dut` instance.
- Kept `RTL_SOURCES` synthesizable-only; Questa compiles the wrapper separately after the local RTL sources and Verilator places it last in the flat simulation source list.
- Changed both simulator tops to `dut_wrapper` and added the canonical `sim_verilator_compile` / `sim_verilator_run` targets and separate Verilator VIP plugin path.
- Normalized the Questa batch and GUI run scripts to `run -all`, made `-onfinish stop` the final active `vsim_options` entry, and retained 1 ps precision.
- Rebased `waveforms/wave.tcl` from `/uart_core` to `/dut_wrapper`, retained the existing groups/formatting/selections, and added the native `clk` controls. No selected DUT-internal path required `/dut_wrapper/u_dut`.
- Updated `docs/design_guide.md` because the wrapper hierarchy, clock ownership, and dual-simulator build architecture are enduring project facts.
- Patched the staged root `user_guide.md` in place with the exact wrapper parameter/port contract, VIP-driven active-low reset contract, internally generated `clk`, 1/64/1 native control widths, `vip::common::Clock` / `NativeClockCfg` requirement, keepalive exclusion, hierarchy, and simulator-plugin prerequisites.
- Corrected the enduring RTL-local configure/build instructions to use `./build`; `cmake-build-release` and `cmake-build-verilator` remain reserved for the separately built sibling VIP plugin paths.
- Made no changes to synthesizable `src/*.sv`, the reusable export manifest, submodule topology, testcase intent, or external verification artifacts.

## REQ-N and coverage status

`docs/design_guide.md` contains no `REQ-N` identifiers and no
`reports/report_design_coverage.md` existed, so no coverage report was created.

## Local validation

Static conformance audit passed:

- exactly one `sim_native_clock_gen`, one `dut_wrapper`, one native generator instance, one 1/64/1 control group, and one `u_dut` instance;
- wrapper parameters and functional ports match `uart_core`, with only `clk` removed from the public wrapper inputs;
- `RTL_TOP_MODULE` and `VERILATOR_TOP_MODULE` are `dut_wrapper`;
- canonical Verilator VPI/timing/language/output options and plugin load syntax are present;
- Questa and Verilator have no project defines/includes/parameter overrides and therefore match configuration;
- batch active lines are exactly `log -r /*` and `run -all`; GUI ends with `run -all`; no active finite-duration run remains;
- final active `vsim_options` entry is `-onfinish stop`;
- wrapper is absent from `RTL_SOURCES`, `src/`, and `scripts/cmake/questa_modules.cmake`;
- Verilator has zero dependency sources, then the same eight local RTL sources, then `dut_wrapper.sv`;
- no stale `/uart_core` waveform root, implementation placeholder, trailing whitespace, or tracked diff whitespace error was found;
- staged `user_guide.md` names `dut_wrapper`, `u_dut`, all native clock controls, `sim_keepalive`, and the required native-clock VIP API.

An earlier main-thread mechanical qualification pass used the now-superseded
RTL-local `cmake-build-release` directory and produced these exact results:

- `cmake -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release`: PASS.
- `cmake --build cmake-build-release --target sim_questa_init`: PASS; compiled the eight product sources in dependency order followed by `dut_wrapper.sv`, with 0 errors and 0 warnings.
- `cmake --build cmake-build-release --target sim_questa_compile`: PASS; the `vmake` incremental build completed, with only make's circular `work/_lib.qdb` dependency-dropped notices.
- `cmake --build cmake-build-release --target sim_verilator_compile`: PASS using Verilator 5.051 devel rev v5.050-222-gf6f6f8404; two non-fatal `ZERODLY` warnings identify the canonical variable half-cycle delays at `dut_wrapper.sv` lines 67 and 74.

No run or simulation target was executed. In particular,
`sim_questa_run`, `sim_questa_run_gui`, and `sim_verilator_run` were not run, as
explicitly prohibited.

The user subsequently clarified that RTL-local configure/build commands must use
`./build`, while `cmake-build-release` and `cmake-build-verilator` are reserved
for sibling VIP plugin builds. Corrected main-thread mechanical qualification
then completed with these exact results:

- `cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release`: PASS.
- `cmake --build ./build --target sim_questa_init`: PASS; compiled the eight product sources in dependency order followed by `dut_wrapper.sv`, with 0 errors and 0 warnings.
- `cmake --build ./build --target sim_questa_compile`: PASS; the `vmake` incremental build completed, with only make's circular `work/_lib.qdb` dependency-dropped notices.
- `cmake --build ./build --target sim_verilator_compile`: PASS using Verilator 5.051 devel rev v5.050-222-gf6f6f8404; the same two non-fatal `ZERODLY` warnings identify the canonical variable half-cycle delays at `dut_wrapper.sv` lines 67 and 74.

No run or simulation target was executed during corrected validation.

## Unresolved issues

No static implementation or compiler blocker remains. The two Verilator
`ZERODLY` warnings are non-fatal and point only to the canonical native-clock
generator's variable half-cycle delays. The nominal 10 ns `clk` default is
derived from the staged guide's documented 100 MHz integration example; the VIP
must program `sim_clk_period_ticks` before enabling the clock.

The staged root `user_guide.md` was absent during this build-directory correction
pass, so its RTL-local build-command references could not be patched. It was not
recreated from memory or from the sibling VIP repository.

The sibling VIP repository was not listed, traversed, inspected, built, or
modified. Only the user-supplied root `user_guide.md` handoff copy was patched.
