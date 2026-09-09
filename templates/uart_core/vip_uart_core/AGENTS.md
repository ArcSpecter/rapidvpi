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

# VIP project agent rules

This repository uses the local `vip-workflow` skill for VIP testcase development and final `.so` preparation.

## Non-negotiable boundary

- This flow exists to edit/compile VIP code and produce a VIP `.so` only.
- NEVER run RTL simulation from this workflow.
- NEVER inspect/read/search/modify sibling `rtl_*` repositories or RTL source.
- NEVER change expected testcase behavior merely to make a DUT pass.
- Make only changes required by the user's request. Do not refactor, optimize, modernize, clean up, rename, or alter unrelated code/APIs/files.


## Clock-control rule

When the RTL simulation top exposes native clock controls through `dut_wrapper.sv`, project verification code shall use `vip_common::Clock` for those externally owned clocks.

Do not manually generate or toggle those clocks from testcase or protocol-agent code.

Use:

- the RTL `user_guide.md` to determine which clocks and `sim_*` control/status nets are exposed;
- `vip_design_guide.md` for the standard `vip_common::Clock` construction and operation rules.

DUT-generated clocks remain observation-only.

## Testcase selection rule

When the user asks to disable, isolate, or select testcases, control execution through the runner testcase plan only.

Typical temporary disable:

```cpp
// smoke_plan.emplace_back("tc_random_stress");
```

The normal VIP build shall continue to contain the complete testcase implementation and registration set.

Do not remove or comment out:

- `tc_*.cpp` files from CMake;
- testcase headers/includes;
- `register_tc_*()` calls;

merely to disable or isolate a testcase.

Only exclude testcase implementations from compilation or registration when the user explicitly requests a specialized build that does so. Otherwise, testcase selection means changing only the runner execution plan.

## Architecture

Classify the request semantically into exactly one of two paths.

### COMPILE_ONLY

Use when the requested `tc_*` implementations already exist and the user asks only to compile, compile-check, enable/select testcases, prepare a `.so`, prepare a testcase subset, or prepare a full regression `.so`.

- The main coordinator performs testcase selection and normal project builds directly.
- Do NOT start `vip_testcase` workers.
- Do NOT start a `vip_integration` worker.
- If individual testcase compile checks are requested, select each requested testcase through the runner execution plan and build it sequentially in the same normal build tree.
- If only a final `.so` is requested, select exactly the requested final testcase set in the runner execution plan and build it directly; do not perform unnecessary individual compile checks.
- Final selection can be one testcase, an explicit subset, or all testcases applicable to the requested DUT mode.
- Intermediate `.so` files are disposable and may overwrite the same normal build artifact.
- If compilation shows that testcase/shared-VIP source must be changed, stop and report the failure unless the user also authorized DEVELOPMENT work.

### DEVELOPMENT

Use only when the user asks to implement, add, develop, fix, repair, rework, or change testcase/shared-VIP behavior.

- Use one fresh `vip_testcase` worker per requested/applicable `tc_*` that requires engineering work.
- Workers run strictly sequentially in the same project repository; repository state persists, worker context does not.
- Each testcase worker selects only its own `tc_*` in the runner execution plan and comments/removes the other active-mode plan entries only; it must not remove testcase sources from CMake or remove `register_tc_*()` registrations. It implements/fixes only the assigned testcase and genuinely required shared support, and must compile the normal project `.so` successfully.
- After actual testcase-development work, use one fresh `vip_integration` worker only when a retained final `.so` selection/build is requested.
- Final selection can be one testcase, an explicit subset, or all testcases applicable to the requested DUT mode.
