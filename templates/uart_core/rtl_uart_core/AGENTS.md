# AGENTS.md

## Purpose and authority

This repository contains synthesizable RTL and may be either a reusable RTL IP or a system-integration RTL project.

Before modifying RTL, wrappers, constraints, build files, or project structure, treat these as authority in this order:

```text
docs/design_guide.md       project-specific functionality and architecture
docs/rtl_design_guide.md   RTL coding/implementation policy
```

Generated `reports/*` are non-authoritative evidence/results and never override the docs.

This workflow is generic. Project-specific module names, interfaces, parameters, dependency lists, behavior, and architecture belong in the project docs/build files, not in `.agents` or `.codex`.

## First inspection

Before editing:

1. Read `docs/design_guide.md`.
2. Read `docs/rtl_design_guide.md`.
3. Inspect the repository tree and relevant build/source manifests.
4. Determine whether the project is `reusable_ip` or `system_integration` from the design guide; do not infer only from the repository name.
5. If documentation and code materially disagree, do not silently choose the code.

Common files to inspect when present:

```text
src/
ext/
CMakeLists.txt
scripts/cmake/rtl_dependency_helpers.cmake
scripts/cmake/questa_modules.cmake
scripts/questa/**/*.cmake
scripts/vivado/read_sources.tcl
```

During normal implementation or failure-driven rework, `docs/design_guide.md` is authoritative. Update it only for an explicitly authorized planned change that alters enduring design behavior/architecture, or when the user explicitly says the guide is stale.

## Scope discipline

Make only the changes required by the request.

Do not refactor, optimize, modernize, clean up, rename, or alter unrelated RTL, APIs, files, dependencies, or build machinery unless the user explicitly requests broader discretionary work.

Do not:
- invent unspecified DUT behavior;
- modify external verification source or expected results;
- weaken the specification to match current RTL;
- add temporary testbenches unless explicitly requested;
- add simulation-only logic to synthesizable RTL;
- fetch/update git submodules unless explicitly requested.

## External verification / VIP trust boundary

This workflow owns and inspects the RTL repository only. A sibling verification/VIP project is outside this workflow's authority **and outside its inspection domain**.

Within `rtl-workflow`, NEVER:
- list, traverse, search, grep, open, read, stat, or otherwise inspect files inside a sibling `vip_*` or other external verification project;
- inspect external verification source, headers, docs, CMake files, build caches, testcase registration, configuration files, or generated build products to discover what the current simulation will run;
- build, reconfigure, repair, or modify a sibling verification/VIP project;
- infer verification behavior from external implementation internals.

The compiled verification shared object is an **opaque external dependency**. Its path/loading/configuration are owned by the RTL repository's already-configured CMake/simulation flow. Normally the workflow does not need to inspect the `.so` directly either; it runs the configured RTL target and lets the build/simulation system consume the artifact.

Allowed verification-side evidence is only what is already emitted through the RTL-side simulation execution, for example simulator return status, the local simulator transcript, testcase runtime diagnostics appearing in that transcript, configured local waveforms/logs, and actual DUT-boundary stimulus/response. Reading such runtime evidence is not permission to inspect the external verification project.

If the configured RTL simulation cannot locate/load/use its external verification artifact, classify/report the failure from the RTL-side build/simulator evidence (normally `TOOLING_BLOCKER`) and stop. Do not enter the sibling project to repair it.

If the user explicitly asks to inspect or modify a VIP/verification project, that is a separate verification-side task and is not performed under this RTL workflow.

## Workflow model

The main Codex thread is the workflow entry point. It resolves user intent first, reads only the local project state needed for that mode, routes engineering tasks, and runs deterministic tooling directly.

Specialists:

```text
rtl_design
rtl_rework
rtl_cdc_audit
```

Rules:
- spawn a specialist only when its engineering judgment is required;
- run at most one specialist at a time;
- never run concurrent RTL-writing workers;
- do not create a reasoning worker merely to run configured init/compile/lint/simulation commands;
- preserve repository/filesystem state as the handoff between sequential steps.

### Request routing

```text
initial/full RTL implementation
    -> rtl_design

user-approved RTL change instructions
    -> rtl_rework / PLANNED_CHANGE

compile or lint only
    -> main thread runs configured deterministic tooling

simulation/regression without RTL repair intent
    -> main thread / SIM_ONLY

simulation/regression with explicit RTL repair/closure intent
    -> main thread / SIM_REPAIR_LOOP
    -> on FAIL: rtl_rework / FAILURE_REWORK

known failing simulation with explicit RTL repair/rework requested
    -> rtl_rework / FAILURE_REWORK

known failing simulation with diagnosis-only intent
    -> analyze/report only; no RTL modification permission

explicit CDC audit/check/review
    -> rtl_cdc_audit
```

Use user intent, not a hard-coded filename. A change-request document has authority only when the user identifies it as part of the requested change.

## Reports

Use exactly:

```text
reports/report_design_coverage.md
reports/report_cdc.md
reports/report_rework.md
reports/report_sim.md
```

Create `reports/` on demand. Never create timestamped variants. Overwrite the fixed latest-result file for repeated activity.

Detailed report contracts are in:

```text
.agents/skills/rtl-workflow/references/report-contracts.md
```

## Optional REQ-N traceability

When `docs/design_guide.md` contains `REQ-<positive integer>` identifiers:

- every REQ-N is a mandatory obligation;
- IDs are immutable global serial identities and do not encode document position;
- never renumber or reuse an existing ID;
- `rtl_design` must account for every REQ as DONE or BLOCKED;
- after full implementation settles, `rtl_design` creates/refreshes `reports/report_design_coverage.md`;
- an authorized planned change keeps the same ID when refining the same obligation;
- a genuinely new obligation in an already REQ-enabled guide receives the next unused global REQ-N when no ID is supplied;
- `rtl_rework` updates affected coverage evidence only when the report already exists.

If no REQ-N identifiers exist, the project is valid without a coverage report. Do not create one merely because this workflow is installed.

## Planned-change mode

`rtl_rework / PLANNED_CHANGE` uses:

```text
docs/design_guide.md                  current baseline
user-selected change instructions     approved delta for this task
```

The worker may update RTL and other owned project files required by the delta.

If the approved change alters enduring architecture, interfaces, parameters, behavior, register semantics, clock/reset behavior, dependency structure, or other lasting design requirements, update `docs/design_guide.md` so it becomes the complete post-change source of truth.

Preserve unrelated behavior and do not widen the requested change.

## Failure-rework mode

`rtl_rework / FAILURE_REWORK` treats `docs/design_guide.md` as fixed authority and classifies from RTL-side runtime evidence before modifying RTL.

Evidence may include:
- simulator status/transcript;
- testcase runtime diagnostics;
- `[RTL]` diagnostics;
- configured waveform/log evidence;
- actual DUT-boundary stimulus;
- actual DUT response;
- relevant RTL;
- exact design-guide requirement and REQ-N when present.

Do not depend on external verification-project documentation to establish an RTL defect.

### Six-point automatic RTL rework gate

Automatic RTL correction is allowed only when ALL are proven:

1. The violated behavior is explicitly defined by `docs/design_guide.md`.
2. The testcase stimulus demonstrably matches the intended scenario at the DUT boundary.
3. The observed DUT behavior demonstrably violates that requirement.
4. The failure does not depend on unresolved interpretation.
5. The correction does not require changing the specification.
6. The correction is RTL-only and does not require modifying external testcase/expected-result/verification artifacts.

If all six pass, `rtl_rework` may apply a bounded RTL correction and classify:

```text
RTL_AUTO_REWORK_APPLIED
```

Otherwise stop without questionable RTL modification using exactly one primary classification:

```text
RTL_DEFECT_NO_SAFE_AUTOFIX
EXTERNAL_TEST_ISSUE
SPECIFICATION_QUESTION
ARCHITECTURAL_DECISION_REQUIRED
UNRESOLVED_FAILURE
TOOLING_BLOCKER
```

Detailed classification policy is in:

```text
.agents/skills/rtl-workflow/references/failure-rework.md
```

## CDC audit

`rtl_cdc_audit` is explicit-request only. Never launch it automatically after implementation, rework, compile, lint, or simulation.

It first performs a fast applicability precheck from the declared clock/reset inventory, Complete CDC Inventory when present, and structural RTL sanity.

If there is exactly one functional clock domain and no asynchronous crossing/event/configuration/reset interaction requiring synchronization, return:

```text
CDC_AUDIT_NOT_APPLICABLE
```

Do not decide applicability merely by counting ports named `clk`.

When CDC exists, audit declared crossings and search for credible undeclared crossings. Check source/destination clocks/resets, transfer coherency, required versus implemented mechanism, request/ack sequencing, event persistence, reconvergence, reset/in-flight behavior, asynchronous FIFO/Gray behavior where applicable, multi-bit sampling safety, and crossing ownership/direction.

Results:

```text
CDC_AUDIT_PASS
CDC_AUDIT_FAIL
CDC_AUDIT_UNRESOLVED
CDC_AUDIT_NOT_APPLICABLE
```

The CDC worker is read-only. The main thread writes/overwrites `reports/report_cdc.md`. Any CDC repair is a separate requested RTL rework.

## Deterministic tooling and simulation

Use only repository-configured build/lint/simulation targets or an exact command explicitly supplied by the user.

Do not invent simulator commands or bypass the project flow.

The main thread directly performs mechanical steps such as configured init, compile, lint, simulation, and rerun. A simulation run does not justify creating `rtl_design` or `rtl_rework` merely because it ran or failed. Whether a failing simulation may enter RTL rework is determined by the simulation policy resolved from the user's request.

### Mechanical fast path

Resolve the requested tooling/simulation mode **before** broad repository inspection. For mechanical compile/lint/simulation execution, prefer the shortest local path to the already-configured target.

Every simulation execution starts on this mechanical fast path, including `SIM_ONLY` and each run/rerun inside `SIM_REPAIR_LOOP`:

1. Do not perform the full design/editing inspection merely to run an existing simulation. In `SIM_REPAIR_LOOP`, defer design-guide/RTL engineering inspection until a FAIL actually requires `FAILURE_REWORK`.
2. Do not inventory the repository with broad recursive `rg --files`, `find`, directory dumps, or similar discovery when the configured target/build directory is already known or can be identified from a small amount of local build metadata.
3. Never inspect a sibling verification/VIP project for testcase selection, configuration, build state, source, headers, or CMake/cache information. The configured RTL build target is the interface.
4. Do not inspect `docs/design_guide.md` or `docs/rtl_design_guide.md` merely to execute a pure `SIM_ONLY` run unless the user asks for design interpretation or the local RTL flow genuinely requires those docs to resolve the configured command.
5. Run the configured non-GUI simulation target.
6. Determine PASS/FAIL from process completion plus concise local simulator evidence. On PASS, do not ingest or dump the full transcript when a small result check is sufficient. On FAIL, collect only the focused local evidence needed to report the failure or feed `FAILURE_REWORK` when `SIM_REPAIR_LOOP` is active.
7. Write/overwrite `reports/report_sim.md`. Then stop for `SIM_ONLY`, or on FAIL hand the focused RTL-side evidence to `FAILURE_REWORK` when `SIM_REPAIR_LOOP` is active.

For configured non-GUI simulation/regression execution, use the repository-provided quiet waiter:

```text
.agents/tools/rtl_sim_wait.sh -- <configured simulation command and arguments>
```

The waiter performs silent child-process liveness checks about every 30 seconds and has **no generic wall-clock timeout**. It is execution infrastructure, not a simulation correctness oracle.

While the wrapped simulation is still active:
- do not repeatedly inspect/search the simulator transcript merely to report progress;
- do not emit periodic `still running`, testcase-count, timeout-poll, or similar commentary;
- do not perform engineering reasoning merely because a wait interval elapsed;
- do not infer a simulator hang from silence alone;
- do not force-kill the run based on elapsed wall-clock time.

If the tool host itself surfaces an intermediate background-process/`still running` status despite the quiet wrapper, immediately continue waiting on the same process without transcript inspection, commentary, or new engineering analysis. Re-enter reasoning when the process exits, a concrete tool/execution error occurs, the user interrupts, or the user explicitly asks for an interim inspection.

Do not repeatedly run broad `git status`, `git diff`, filesystem inventories, or log dumps merely to prove that a mechanical run did not edit RTL. If a write-safety check is useful, keep it concise.

Resolve one of these policies before the first simulation run:

```text
SIM_ONLY
    user requested simulation/regression/result only
    -> PASS: report and stop
    -> FAIL: report and stop
    -> no rtl_rework and no RTL modification

SIM_REPAIR_LOOP
    simulation task explicitly requests fixing/reworking/correcting real RTL failures or RTL-side closure
    -> rerun after every gate-permitted RTL fix is implied by this policy
    -> PASS: stop
    -> FAIL: rtl_rework / FAILURE_REWORK
         -> RTL_AUTO_REWORK_APPLIED: validate + rerun
         -> terminal classification: stop
```

`report the result` does not imply repair permission. An explicit instruction not to modify RTL forces `SIM_ONLY`.

After every configured non-GUI simulation run performed by the workflow, create/overwrite `reports/report_sim.md` with the exact observed PASS/FAIL result and concise evidence.

When the verification runner completes, normal simulation completion uses the simulation environment's established high-level finish mechanism. The waiter's ~30-second liveness poll does not alter simulation completion and is not a timeout. Do not add a generic maximum wall-clock duration, progress heuristic, or forced process termination to this generic workflow. Testcase-functional timeouts remain owned by the verification environment.

Never claim lint or simulation passed unless the configured command actually completed successfully and its result was inspected.

### Simulation repair loop

`SIM_REPAIR_LOOP` is active only when the simulation task explicitly requests fixing/reworking/correcting real RTL failures, or otherwise explicitly requests RTL-side closure. The repair request itself is sufficient; the user does not need to separately say `rerun`. Rerunning after every gate-permitted RTL fix is part of this policy.

```text
configured simulation
    -> PASS: stop
    -> FAIL: rtl_rework / FAILURE_REWORK
         -> RTL_AUTO_REWORK_APPLIED: main thread validates and reruns simulation
         -> RTL_DEFECT_NO_SAFE_AUTOFIX: stop
         -> EXTERNAL_TEST_ISSUE: stop
         -> SPECIFICATION_QUESTION: stop
         -> ARCHITECTURAL_DECISION_REQUIRED: stop
         -> UNRESOLVED_FAILURE: stop
         -> TOOLING_BLOCKER: stop
```

There is no arbitrary retry limit. After every applied RTL correction, rerun the configured validation/simulation and treat any new FAIL as fresh evidence requiring a fresh six-point-gate decision.

Do not spin indefinitely on unchanged state: never reapply the same patch or repeatedly rerun an unchanged failing state without new evidence. If another bounded repair cannot be proven, stop with the appropriate existing terminal classification, normally `UNRESOLVED_FAILURE` or `RTL_DEFECT_NO_SAFE_AUTOFIX` depending on the evidence.

Do not continue after any terminal classification. Do not modify or inspect external verification-project internals to obtain PASS.

## RTL coding rules

Follow `docs/rtl_design_guide.md` strictly. Key reusable rules are:

- use SystemVerilog 2012;
- put `` `default_nettype none `` at the top of RTL files and do not restore `` `default_nettype wire `` at the end;
- use `input wire` for module inputs and `logic` for outputs/internal signals;
- use `always_ff`, `always_comb`, and continuous `assign`; do not use plain `always` or `always @(*)`;
- use synchronous reset for `always_ff` logic when reset is required unless the authoritative design explicitly requires otherwise;
- code FSMs as single-process synchronous FSMs, using symbolic enum states and a safe `default` branch that returns the state register to the idle state; do not split an FSM into separate sequential next-state and combinational next-state processes;
- do not add SVA assertions unless the user explicitly asks for them;
- keep synthesizable RTL synthesizable; do not add delays, testbench constructs, or `initial` blocks;
- preserve existing naming/style unless the authoritative docs require change;
- prefer readable, timing-friendly RTL over clever compact logic;
- keep comments local and useful; architecture belongs in `docs/design_guide.md`;
- do not duplicate large specification sections inside code comments.

## Source-file policy

Local reusable RTL source belongs under `src/` unless the design guide explicitly defines otherwise.

Keep source manifests synchronized whenever local source files are added, removed, or renamed.

### Reusable IP

If the project is exported for parent RTL builds, `scripts/cmake/questa_modules.cmake` should list only this repository's local `src` files in dependency order:

- packages/helpers first;
- cores before wrappers;
- top wrappers last;
- no stale files;
- no copied `ext/<dependency>/src/*.sv` paths.

Dependencies should be imported through their own export manifests rather than copied into the local list.

### System integration

A top-level system project may own local source lists directly in subsystem Questa init scripts instead of a local `questa_modules.cmake`. Do not create an export manifest unless the system itself is meant to be imported by a parent build.

## CMake and external RTL dependencies

Follow the project's existing configured helper flow. Do not invent a second dependency framework.

When an external RTL dependency is required:

- keep it under the project's established `ext/` ownership model;
- use the dependency's own exported source/import manifest when available;
- preserve dependency-safe compile order;
- rebuild lower-level libraries before dependents and `WORK_LIB` last;
- keep synthesis source imports synchronized when the dependency participates in synthesis;
- do not duplicate dependency source-file lists into multiple places without a demonstrated need.

For system-integration Questa flows, dependency import belongs in initialization/configuration; compile scripts consume generated makefiles, and run scripts elaborate the configured top. Do not move dependency import logic into run scripts.

If the repository uses the existing `rtl_dependency_helpers.cmake`, inspect and follow its conventions rather than replacing it.

## Lint policy

After RTL/source-list changes, run the configured Verilator lint target when a valid configured build directory/source list is available and the task calls for validation.

Fix definite syntax/elaboration/module/port/width RTL errors. Do not turn warning cleanup into an unrelated redesign.

If lint is not meaningfully configured, report that fact rather than claiming PASS.

## Documentation synchronization

When an authorized change affects enduring interfaces, parameters, file names, pipeline semantics, address maps, register behavior, protocols, dependencies, hierarchy, or clock/reset architecture, keep the relevant authoritative docs and project manifests synchronized.

Potentially affected files include:

```text
docs/design_guide.md
docs/rtl_design_guide.md
reports/report_design_coverage.md      when already using REQ traceability
CMakeLists.txt
src/*.sv
scripts/cmake/questa_modules.cmake     when exporting local RTL
scripts/questa/**/*.cmake              when owning system manifests
scripts/vivado/read_sources.tcl        when synthesis sources change
```

## Before finishing

Report only what is relevant to the task:

- mode used (`rtl_design`, `rtl_rework/PLANNED_CHANGE`, `rtl_rework/FAILURE_REWORK`, `rtl_cdc_audit`, or main-thread tooling);
- files changed;
- project classification and authoritative docs used when the task required them;
- selected planned-change instructions when applicable;
- whether `docs/design_guide.md` changed and why;
- REQ-N/coverage status when applicable;
- configured lint/simulation commands actually run and exact result;
- failure classification and six-point-gate result when applicable;
- CDC classification when explicitly requested;
- fixed reports created/updated;
- remaining blockers/risks/warnings.
