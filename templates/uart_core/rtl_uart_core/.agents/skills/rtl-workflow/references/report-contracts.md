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

# RTL Workflow Report Contracts

All generated workflow artifacts live under `reports/`. Create the directory on demand.

Use exactly these fixed latest-result paths:

```text
reports/report_design_coverage.md
reports/report_cdc.md
reports/report_rework.md
reports/report_sim.md
```

Never create timestamped variants. Reports are non-authoritative evidence/results and never override `docs/design_guide.md` or `docs/rtl_design_guide.md`.

## `reports/report_design_coverage.md`

Created/refreshed by `rtl_design` only when `docs/design_guide.md` uses REQ-N traceability. `rtl_rework` updates affected rows only when the report already exists.

The file contains ONLY this Markdown table shape:

```markdown
| Spec | Status | RTL evidence |
|---|---|---|
| `REQ-1` | DONE | `src/rtl_ctrl.sv @ 42:88` |
| `REQ-2` | BLOCKED | Architectural decision required |
```

Rules:
- one row per REQ-N;
- DONE requires current RTL file/line evidence;
- multiple files remain on one compact row separated by semicolons;
- BLOCKED uses a concise reason;
- absence is normal for legacy projects without REQ-N identifiers.

## `reports/report_sim.md`

Owned by the main Codex thread. Create/overwrite after every configured non-GUI simulation run performed by the workflow.

PASS form:

```markdown
# Simulation Result

RESULT: SIM_PASS

Testcase: `<name-or-unknown>`
Command/target: `<configured command or target>`
Exit status: `<code>`
```

FAIL form:

```markdown
# Simulation Result

RESULT: SIM_FAIL

Testcase: `<name-or-unknown>`
Command/target: `<configured command or target>`
Exit status: `<code>`

## Failure summary

<concise failure>

## Relevant testcase output

<only relevant lines>

## Relevant RTL output

<only relevant [RTL] lines>

## Full evidence

- transcript/log: `<path if available>`
- waveform/log artifacts: `<paths if available>`
```

Do not paste huge transcripts into the report; reference configured output locations. On successful runs, use concise result evidence rather than ingesting the full transcript when unnecessary.

When `.agents/tools/rtl_sim_wait.sh` wraps the simulation, `Command/target` records the underlying configured simulation command/target, not merely the waiter wrapper. The waiter's temporary console log is fallback tooling evidence and may be referenced under `Full evidence` only when it is relevant to launch/build/tool failure. It is not a substitute for the simulator scoreboard/transcript.

`Testcase`/plan identity must come from the configured local RTL command/target or simulator output. If it is not exposed there, use `unknown` or `current configured plan`. Never inspect external verification/VIP source, CMake/cache data, headers, or testcase registration merely to populate this field.

During `SIM_REPAIR_LOOP`, overwrite this file after every simulation iteration. At task completion it represents the latest simulation run.

## `reports/report_rework.md`

Owned by `rtl_rework`. Create/overwrite on every `PLANNED_CHANGE` and `FAILURE_REWORK` invocation.

For planned change, record:
- `RESULT: PLANNED_CHANGE_COMPLETE` or explicit blocker/escalation;
- selected change-request path(s);
- key RTL/project changes;
- whether `docs/design_guide.md` changed and why;
- REQ-N/coverage status when applicable;
- local validation;
- unresolved issues.

For failure rework, begin with:

```text
CLASSIFICATION: <RTL_AUTO_REWORK_APPLIED | RTL_DEFECT_NO_SAFE_AUTOFIX | EXTERNAL_TEST_ISSUE | SPECIFICATION_QUESTION | ARCHITECTURAL_DECISION_REQUIRED | UNRESOLVED_FAILURE | TOOLING_BLOCKER>
```

Then include only the concise evidence needed to understand the classification and six-point-gate result.

During `SIM_REPAIR_LOOP`, overwrite this file on every `FAILURE_REWORK` invocation. At task completion it represents the latest rework decision.

## `reports/report_cdc.md`

Owned by the main Codex thread because `rtl_cdc_audit` is read-only. After every explicit CDC audit, create/overwrite from the worker's structured payload.

Use one of:

```text
CLASSIFICATION: CDC_AUDIT_PASS
CLASSIFICATION: CDC_AUDIT_FAIL
CLASSIFICATION: CDC_AUDIT_UNRESOLVED
CLASSIFICATION: CDC_AUDIT_NOT_APPLICABLE
```

For a full audit include the inventory source, clock/reset domains, compact per-crossing results, REQ-N when present, RTL evidence, undeclared crossings, and exact failure/unresolved reasons.

For `CDC_AUDIT_NOT_APPLICABLE`, keep the report minimal: classification plus evidence that no transfer requiring CDC synchronization exists.
