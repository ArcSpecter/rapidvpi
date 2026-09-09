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

# Failure-Driven RTL Rework Policy

This reference defines the evidence standard and classifications for `rtl_rework` in `FAILURE_REWORK` mode.

`docs/design_guide.md` is fixed authority during failure-driven rework. Do not change specification semantics to make observed RTL or a testcase appear correct.

## Evidence bundle

Classify from evidence available on the RTL side:

- configured simulator return status and transcript;
- testcase runtime diagnostic output;
- `[RTL]` runtime output;
- configured waveforms/logs when available;
- stimulus actually observed at the DUT boundary;
- DUT response actually observed;
- relevant RTL implementation;
- exact `docs/design_guide.md` requirement and REQ-N when present.

Do not depend on external verification-project documentation or internal implementation details to establish an RTL defect. Do not list, search, open, read, stat, build, or otherwise inspect a sibling verification/VIP project. Runtime testcase messages already present in the local simulator transcript are allowed evidence; external testcase source/configuration is not.

## Mandatory six-point gate

Automatic RTL repair is allowed only when ALL are proven:

1. The violated behavior is explicitly defined by `docs/design_guide.md`.
2. The testcase stimulus demonstrably matches the intended scenario at the DUT boundary.
3. The observed DUT behavior demonstrably violates the requirement.
4. The failure does not depend on unresolved interpretation.
5. The correction does not require changing the specification.
6. The correction is RTL-only and does not require modifying an external testcase, expected result, or verification artifact.

If any gate is unproven, do not auto-repair.

## Role in `SIM_REPAIR_LOOP`

Each `FAILURE_REWORK` invocation handles one observed failing run. The main Codex thread owns the iterative simulation loop.

When the user has explicitly requested `SIM_REPAIR_LOOP`:

```text
FAIL
  -> FAILURE_REWORK
       -> RTL_AUTO_REWORK_APPLIED: main thread validates + reruns simulation
       -> any other classification: terminal STOP
```

There is no arbitrary retry count. A new simulation failure after an applied patch is fresh evidence and requires a new `FAILURE_REWORK` invocation with a fresh six-point-gate decision. A prior gate pass never pre-authorizes a later RTL edit.

Do not reapply the same patch or keep rerunning unchanged failing state without new evidence. If further bounded progress cannot be proven, return the appropriate terminal classification, normally `UNRESOLVED_FAILURE` when evidence is insufficient or `RTL_DEFECT_NO_SAFE_AUTOFIX` when an RTL defect is likely but another autonomous change is unsafe.

## Classifications

### `RTL_AUTO_REWORK_APPLIED`

Use only when all six gates pass and a bounded RTL correction has already been applied.

Report:
- violated requirement/REQ-N;
- concise gate evidence;
- changed RTL files/symbols;
- local validation performed.

When `SIM_REPAIR_LOOP` is active, the main thread must then run configured validation and rerun the configured simulation. Outside that policy, follow the user's requested scope.

### `RTL_DEFECT_NO_SAFE_AUTOFIX`

Use when evidence strongly indicates an RTL defect but autonomous correction is not sufficiently bounded or safe.

Examples:
- broad cross-cutting change;
- uncertain blast radius;
- substantial redesign;
- insufficient confidence that a local correction preserves other required behavior.

Do not implement the questionable repair.

### `EXTERNAL_TEST_ISSUE`

Use when RTL-side runtime evidence shows that stimulus/checking presented to the DUT does not establish the scenario it claims or needs to exercise.

Examples:
- claimed transaction differs from the DUT-boundary transaction;
- required stimulus never reaches the DUT;
- driven parameter/length/command/timing/control differs from the claimed scenario;
- DUT response is consistent with actual observed stimulus, but that stimulus is not the required scenario.

No RTL modification. Do not diagnose or repair the external verification implementation from this workflow.

Report concise reproduction evidence suitable for user review or a separate verification-side investigation.

### `SPECIFICATION_QUESTION`

Use when `docs/design_guide.md` does not uniquely determine the required behavior, conflicts materially with itself, or leaves the failing corner case unspecified.

No RTL modification. Identify the exact ambiguity/conflict/missing requirement. Do not invent semantics.

### `ARCHITECTURAL_DECISION_REQUIRED`

Use when correction requires choosing or changing architecture rather than implementing an architecture already prescribed by `docs/design_guide.md`.

Examples include changing CDC mechanism, buffering/queueing strategy, ownership, protocol architecture, or clock/reset architecture.

No autonomous redesign.

### `UNRESOLVED_FAILURE`

Use when available evidence is insufficient to classify confidently.

Report what is known, what evidence is missing, and what runtime observability is required.

### `TOOLING_BLOCKER`

Use when required evidence cannot be produced because configured build/lint/simulation/logging is unavailable or unreliable.

Do not fabricate results or substitute unrelated commands.

## Prohibited behavior

- Do not edit external testcase expected results to match DUT output.
- Do not inspect or modify external verification-project source/configuration/build internals from this RTL workflow.
- Do not weaken or rewrite `docs/design_guide.md` to justify current RTL behavior.
- Do not invent DUT semantics.
- Do not change architecture merely because another architecture would make the test pass.
- Do not auto-fix when any gate is unproven.
- Do not continue an autonomous loop after an escalation classification.
