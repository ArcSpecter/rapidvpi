---
# MIT License
#
# Copyright (c) 2024 Rovshan Rustamov
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

name: rtl-workflow
description: Reusable main-thread RTL engineering workflow for implementation, planned changes, deterministic simulation, failure-driven rework, and explicit CDC audit.
---

# RTL workflow

Use this workflow in synthesizable `rtl_*` repositories.

Project-specific truth is always:

```text
docs/design_guide.md
docs/rtl_design_guide.md
```

Generated `reports/*` are evidence only.

## Trust boundary

This workflow owns and inspects the RTL repository only. A sibling verification/VIP project is outside both its write authority and its inspection domain.

Within this workflow, NEVER list, traverse, search, grep, open, read, stat, build, configure, or modify files inside a sibling `vip_*` or other external verification project. Do not inspect its source, headers, docs, CMake files, build cache, testcase registration, configuration, or generated files to determine what the current RTL simulation will run.

External verification is consumed only as an **opaque compiled dependency** through the RTL repository's already-configured build/simulation flow. Normally do not inspect the `.so` directly either; let the configured RTL target resolve/load it. If loading/configuration fails, use the local build/simulator evidence and stop with the appropriate blocker rather than entering the external project.

Runtime testcase messages already emitted into the local simulator transcript, configured local waveforms/logs, and actual DUT-boundary stimulus/response are valid RTL-side evidence. They do not authorize inspection of the external verification implementation.

If the user explicitly asks to inspect or modify a VIP/verification project, that is a separate verification-side task and is not performed under this RTL workflow.

Make only changes required by the user request. Do not refactor, optimize, modernize, clean up, rename, or alter unrelated code/APIs/files unless explicitly requested.

## Execution model

The main Codex thread is the persistent workflow entry point.

It:
- resolves the requested operating mode before broad inspection;
- reads authoritative docs and relevant local repository structure only when that mode requires engineering interpretation or edits;
- runs configured deterministic init/compile/lint/simulation commands directly;
- spawns a specialist only when engineering reasoning is needed;
- uses at most one specialist at a time;
- uses repository/filesystem state and fixed reports as the handoff between sequential steps.

Specialists:

```text
rtl_design      initial/full implementation
rtl_rework      PLANNED_CHANGE or FAILURE_REWORK
rtl_cdc_audit   explicit CDC audit only; read-only
```

There is deliberately no simulation worker. Running configured build/simulation commands is mechanical execution.

## Reports

Use only:

```text
reports/report_design_coverage.md
reports/report_cdc.md
reports/report_rework.md
reports/report_sim.md
```

See `references/report-contracts.md`.

## Optional REQ-N traceability

If `docs/design_guide.md` contains `REQ-<positive integer>` identifiers:
- every REQ-N is mandatory;
- IDs are immutable global serial identities;
- `rtl_design` accounts for every REQ as DONE or BLOCKED;
- `rtl_design` creates/refreshes `reports/report_design_coverage.md` after full implementation;
- planned changes preserve existing IDs and allocate the next unused ID only for genuinely new obligations when needed;
- `rtl_rework` updates affected coverage rows only if the report already exists.

If the guide has no REQ-N identifiers, do not require or create the coverage report.

## Route the request

### Initial/full design

Use `rtl_design` when the user asks to implement the baseline/full design, implement from `docs/design_guide.md`, or perform a full design-compliance implementation pass.

After the worker returns, the main thread performs configured project-level validation only to the extent requested by the user.

### Planned change

Use `rtl_rework` in `PLANNED_CHANGE` when the user explicitly identifies one or more documents/instructions as an approved RTL change.

Pass the exact selected path(s). The current design guide is baseline; selected instructions are the approved delta. Enduring design changes must be reconciled back into `docs/design_guide.md`.

### Verify / simulate / regress

Do not spawn a specialist merely to run tooling.

Before the first simulation, resolve the policy from user intent:

```text
SIM_ONLY
    simulation/regression/result requested without RTL repair intent

SIM_REPAIR_LOOP
    simulation task explicitly requests fixing/reworking/correcting real RTL failures,
    or otherwise explicitly requests RTL-side closure
    -> rerun after every gate-permitted RTL fix is implied; the user need not separately say `rerun`
```

The main thread:
1. resolves `SIM_ONLY` versus `SIM_REPAIR_LOOP` before broad discovery;
2. uses only the configured **local RTL repository** targets/commands;
3. runs the deterministic non-GUI simulation flow;
4. inspects actual completion/result evidence from the local process/transcript/logs;
5. writes/overwrites `reports/report_sim.md`;
6. on PASS, stops successfully;
7. on FAIL under `SIM_ONLY`, reports the failure and stops without invoking `rtl_rework`;
8. on FAIL under `SIM_REPAIR_LOOP`, invokes `rtl_rework` in `FAILURE_REWORK`.

For every simulation execution, take a strict mechanical fast path before any failure-driven engineering work:
- do not read design docs merely to run an already-configured simulation; under `SIM_REPAIR_LOOP`, defer design-guide/RTL inspection until an actual FAIL requires `FAILURE_REWORK`;
- do not recursively inventory the RTL repository when the configured target/build directory is already known or can be resolved from a small amount of local build metadata;
- do not inspect any sibling verification/VIP project, its source tree, headers, docs, build cache, CMake files, testcase registration, or generated files;
- do not read external testcase source to discover the active testcase/plan;
- run the configured target first, then inspect only concise local result evidence;
- on PASS, avoid ingesting/dumping the full transcript when a focused result check is sufficient;
- on FAIL, collect only focused local evidence needed for the report; when `SIM_REPAIR_LOOP` is active, then invoke `FAILURE_REWORK`, which may read the authoritative RTL docs and relevant RTL needed for diagnosis;
- if testcase/plan identity is not available from the local configured command or simulator output, report `unknown` or `current configured plan` rather than crossing the trust boundary;
- avoid repeated broad `git status`, `git diff`, filesystem inventories, or log dumps merely to prove a mechanical run did not edit RTL.

For configured non-GUI simulation/regression commands, use the included deterministic quiet waiter:

```text
.agents/tools/rtl_sim_wait.sh -- <configured simulation command and arguments>
```

The helper waits on the wrapped process, performs silent liveness checks about every 30 seconds, redirects wrapped console output to a temporary log, and applies **no generic wall-clock timeout**. While the process is active, do not repeatedly read/search the simulator transcript for progress, do not emit periodic `still running` commentary, and do not perform engineering reasoning solely because another wait interval elapsed.

If the tool host surfaces an intermediate `still running`/background-process status anyway, immediately continue waiting on the same process without transcript inspection or commentary. Return to reasoning after process completion or a concrete tool/execution error, or when the user explicitly requests interim inspection. Transcript silence alone is not hang evidence and does not authorize forced termination.

A request such as `use the current VIP configuration` means only: use the external artifact/configuration already wired into the local RTL build/simulation target. It never authorizes inspection of the VIP project.

`report the result` does not imply repair permission. An explicit instruction not to modify RTL forces `SIM_ONLY`.

Simulation completion is owned by the established simulation environment. When the verification runner has completed, normal simulation termination uses the already-implemented high-level simulation-finish mechanism. The quiet waiter's ~30-second checks are host-process liveness polls only; they do not impose a maximum duration. Do not add a generic wall-clock timeout or process killing to this reusable flow.

### Known failing simulation

When the user provides or points to an existing failing run and explicitly asks to fix/rework the RTL, collect the available RTL-side runtime evidence and invoke `rtl_rework` in `FAILURE_REWORK`.

A diagnosis-only request does not grant RTL modification permission. Do not treat `diagnose` by itself as permission to enter the repair path.

### CDC audit

Use `rtl_cdc_audit` only when the user explicitly asks for CDC audit/check/review.

Do not launch CDC audit automatically after design, rework, compile, lint, or simulation.

The worker returns one of:

```text
CDC_AUDIT_PASS
CDC_AUDIT_FAIL
CDC_AUDIT_UNRESOLVED
CDC_AUDIT_NOT_APPLICABLE
```

Because it is read-only, the main thread writes/overwrites `reports/report_cdc.md` from its structured payload.

CDC repair, if later requested, is a separate RTL rework task.

## Failure-rework gate

Before `rtl_rework` may modify RTL for a failing simulation, ALL six conditions must be proven:

1. requirement explicit in `docs/design_guide.md`;
2. actual DUT-boundary stimulus matches the intended scenario;
3. observed DUT behavior violates the requirement;
4. no unresolved interpretation;
5. correction does not require specification change;
6. correction is RTL-only and does not require external verification changes.

If all pass, the worker may return `RTL_AUTO_REWORK_APPLIED` after making the bounded RTL correction.

Otherwise it must stop with one of:

```text
RTL_DEFECT_NO_SAFE_AUTOFIX
EXTERNAL_TEST_ISSUE
SPECIFICATION_QUESTION
ARCHITECTURAL_DECISION_REQUIRED
UNRESOLVED_FAILURE
TOOLING_BLOCKER
```

See `references/failure-rework.md`.

## Simulation repair loop

When `SIM_REPAIR_LOOP` is active:

```text
run configured simulation
    -> PASS: stop
    -> FAIL: invoke rtl_rework / FAILURE_REWORK
         -> RTL_AUTO_REWORK_APPLIED: rerun configured validation/simulation
         -> RTL_DEFECT_NO_SAFE_AUTOFIX: stop
         -> EXTERNAL_TEST_ISSUE: stop
         -> SPECIFICATION_QUESTION: stop
         -> ARCHITECTURAL_DECISION_REQUIRED: stop
         -> UNRESOLVED_FAILURE: stop
         -> TOOLING_BLOCKER: stop
```

There is no fixed iteration/retry limit. Every new FAIL is a fresh failure-rework decision and all six gates must be proven again before another RTL edit.

Never reapply the same patch or repeatedly rerun unchanged failing state without new evidence. If no further bounded repair can be proven, stop with the appropriate existing terminal classification, normally `UNRESOLVED_FAILURE` or `RTL_DEFECT_NO_SAFE_AUTOFIX` depending on the evidence.

Do not continue after a terminal classification. Do not inspect or mutate external verification-project internals to obtain PASS.

## Main-thread completion report

At task completion report only relevant facts:
- operating mode;
- files changed;
- authoritative docs/change instructions used;
- REQ traceability status when applicable;
- configured lint/simulation commands actually run and exact results;
- failure classification and rework-gate result when applicable;
- CDC classification when explicitly requested;
- reports created/updated;
- unresolved blockers/risks.
