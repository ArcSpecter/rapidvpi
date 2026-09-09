# RTL/VIP Interop Protocol v1 — RTL Side

This reference defines the RTL side of the optional coordinated RTL/VIP workflow.

It is runtime synchronization infrastructure only. It does not transfer engineering authority between the RTL and VIP trust domains.

## Activation

Interop is inactive by default.

Enter coordinated mode only when the user explicitly asks to start or continue coordinated/interoperability operation with the sibling VIP workflow.

Standalone RTL behavior remains unchanged when coordinated mode is not active.

## Workspace model

Codex is started from the RTL project root:

```text
<workspace>/
├── rtl_<project>/          <- current RTL Codex working directory
├── vip_<project>/          <- separate VIP trust domain
└── interop/                <- temporary runtime synchronization area
```

From the RTL project root:

```text
INTEROP_ROOT = ../interop
RTL_OUTBOX   = ../interop/rtl_to_vip
VIP_INBOX    = ../interop/vip_to_rtl
```

The RTL workflow may create `../interop/` when coordinated mode starts and must create its own outbound directory on demand:

```text
../interop/rtl_to_vip/
```

The RTL workflow MUST NOT create, modify, rename, delete, or replace files inside:

```text
../interop/vip_to_rtl/
```

Absence of `../interop/vip_to_rtl/` or its `status.json` is normal and means the VIP side has not published state yet.

Only the main RTL Codex thread performs interop filesystem operations. Specialist workers do not write interop state.

## RTL-owned runtime layout

The RTL side creates/reuses:

```text
../interop/
└── rtl_to_vip/
    ├── status.json
    └── issues/
        └── issue_000007/
            ├── issue.json
            └── transcript
```

`status.json` is mutable current coordination state.

Every published `issue_NNNNNN/` directory is an immutable snapshot of one `EXTERNAL_TEST_ISSUE` handoff.

The interop tree is temporary runtime state and is not project documentation, design authority, or a version-controlled artifact.

## Path convention

Paths stored in protocol JSON are relative to the workspace root: the directory containing `rtl_*`, `vip_*`, and `interop/`.

Do not publish machine-specific absolute paths.

## VIP build identity

RTL consumes only a valid VIP publication from:

```text
../interop/vip_to_rtl/status.json
```

A usable VIP build publication has:

```json
{
  "protocol_version": 1,
  "producer": "VIP",
  "state": "VIP_READY",
  "build_id": 19,
  "artifact": "vip_example/build/vip_example.so",
  "artifact_hash": "sha256:<hex>"
}
```

A corrective VIP build responding to an RTL issue also includes:

```json
{
  "responds_to_issue_id": 7
}
```

Rules:

- `build_id` is owned by VIP; RTL never creates or increments it.
- `artifact` is workspace-root-relative.
- before simulation, RTL MUST calculate SHA-256 over the published `.so` and require exact equality with `artifact_hash`;
- RTL never modifies the VIP artifact;
- a pre-existing `VIP_READY` is safe to consume if the artifact exists and its hash still matches the publication;
- if the artifact is absent or the hash does not match, do not simulate that publication.

## Waiting/polling

The RTL and VIP Codex sessions never directly invoke or message each other. Filesystem publication is the only coordination mechanism in protocol v1.

When waiting for VIP, periodically re-read:

```text
../interop/vip_to_rtl/status.json
```

Detect progress from protocol fields, not timestamps or file modification time. Missing peer state means not yet published. Invalid/incomplete JSON must not be repaired by RTL; treat it as not-yet-valid and retry while coordinated mode remains active.

No background supervisor is required for v1.

## RTL issue identity

`issue_id` is owned by RTL.

For a new issue, use:

```text
max(complete existing issue_NNNNNN directories) + 1
```

Start at `1` when no complete issue directory exists.

Zero-pad directory names to six decimal digits:

```text
issue_000001
issue_000002
...
```

Ignore incomplete temporary publication directories when determining the next ID.

## `EXTERNAL_TEST_ISSUE` package

`EXTERNAL_TEST_ISSUE` is the only RTL failure classification that asks the VIP workflow to reconsider its own testcase/VIP implementation.

When `rtl_rework` returns `EXTERNAL_TEST_ISSUE` in coordinated mode, the main RTL thread creates:

```text
../interop/rtl_to_vip/issues/issue_NNNNNN/
├── issue.json
└── transcript
```

The transcript is mandatory.

Use the exact Questa transcript from the failing run, normally:

```text
./build/questa/transcript
```

Copy it byte-for-byte. Do not parse, filter, rewrite, truncate, or extract only selected VIP/RTL lines for the interop copy.

If the exact failing transcript is unavailable, do not publish an incomplete external-test issue. Stop with a tooling/evidence blocker instead.

Do NOT copy into the issue package:

```text
reports/report_rework.md
docs/design_guide.md
RTL source
waveform databases such as .wlf
RTL internal reasoning or analysis
```

Those remain inside the RTL trust domain.

### `issue.json`

Use:

```json
{
  "protocol_version": 1,
  "issue_id": 7,
  "classification": "EXTERNAL_TEST_ISSUE",
  "vip_build_id": 18,
  "vip_artifact_hash": "sha256:<hex>",
  "failing_testcases": ["tc_example"],
  "transcript": "interop/rtl_to_vip/issues/issue_000007/transcript"
}
```

`failing_testcases` may be an empty array when the exact testcase identity cannot be established from the run.

The issue JSON is coordination metadata only. It must not contain RTL specification excerpts, RTL repair instructions, or conclusions about VIP internals.

## RTL `status.json`

### External-test issue

After the complete immutable issue directory exists, publish:

```json
{
  "protocol_version": 1,
  "producer": "RTL",
  "state": "EXTERNAL_TEST_ISSUE",
  "issue_id": 7,
  "vip_build_id": 18,
  "vip_artifact_hash": "sha256:<hex>",
  "issue": "interop/rtl_to_vip/issues/issue_000007/issue.json"
}
```

This is the signal that VIP should independently evaluate the run against its own authoritative `docs/user_guide.md` and `docs/verification_plan.md`.

### Verification pass

When the coordinated simulation reaches a real complete verification PASS, publish:

```json
{
  "protocol_version": 1,
  "producer": "RTL",
  "state": "VERIFICATION_PASS",
  "vip_build_id": 19,
  "vip_artifact_hash": "sha256:<hex>"
}
```

Do not publish `VERIFICATION_PASS` merely because the simulator process exited with status zero. Require evidence that the selected VIP testcase plan reached its final runner/scoreboard completion and that the run passed.

### RTL stopped

If coordinated operation terminates for an RTL-side escalation other than `EXTERNAL_TEST_ISSUE`, the RTL side may publish a terminal notification so the VIP side does not wait indefinitely:

```json
{
  "protocol_version": 1,
  "producer": "RTL",
  "state": "RTL_STOPPED",
  "reason": "SPECIFICATION_QUESTION",
  "vip_build_id": 19
}
```

`RTL_STOPPED` is notification only. It does not ask VIP to change anything.

Valid `reason` values are the existing RTL stop classifications, plus `CONTRACT_REVIEW_REQUIRED` when that state was received from VIP.

## Expected VIP response to an RTL issue

After RTL publishes issue `N`, wait for one of two peer outcomes.

### VIP repairs itself

VIP publishes a new valid `VIP_READY` with:

```text
build_id > build_id used by the failing RTL run
responds_to_issue_id == N
```

RTL MUST validate the new artifact hash, then rerun the complete configured simulation from scratch.

Do not assume the prior RTL diagnosis remains true after the VIP rebuild. Judge the new run independently. It may PASS, expose a qualifying RTL defect, or produce another classification.

### VIP does not justify a testcase/VIP change

VIP may publish:

```json
{
  "protocol_version": 1,
  "producer": "VIP",
  "state": "CONTRACT_REVIEW_REQUIRED",
  "issue_id": 7
}
```

This means VIP independently evaluated RTL issue `7` against its own authoritative documentation and did not justify changing its testcase/VIP implementation.

On this state:

- stop autonomous RTL/VIP coordination;
- do not modify RTL merely to satisfy the testcase;
- do not ask the RTL worker to reinterpret VIP internals;
- report that human engineering review is required.

The RTL side may then publish `RTL_STOPPED` with `reason = "CONTRACT_REVIEW_REQUIRED"` and the same `issue_id` for terminal-state clarity.

## Atomic publication

Never expose partially-written JSON or partially-copied issue evidence as current peer-visible state.

For an issue:

1. create a temporary directory under `rtl_to_vip/issues/` for the next issue ID;
2. copy the exact failing transcript into the temporary directory;
3. write the complete `issue.json` into the temporary directory;
4. atomically rename the temporary directory to `issue_NNNNNN/`;
5. write a complete temporary `status.json` in `rtl_to_vip/`;
6. atomically rename/replace it as `rtl_to_vip/status.json` LAST.

For any status-only update, write a complete temporary file in `rtl_to_vip/` and atomically rename/replace it as `status.json`.

Temporary files/directories belong only to `rtl_to_vip/` and may be cleaned by the RTL side after an interrupted publication. Never clean peer-owned temporary state.

## Coordinated loop

The RTL side of the loop is:

```text
user explicitly starts coordinated mode
        ↓
create/reuse ../interop/rtl_to_vip/issues
        ↓
wait for valid VIP_READY if no usable build is published yet
        ↓
validate VIP artifact SHA-256
        ↓
run configured RTL simulation
        ↓
PASS
  └── publish VERIFICATION_PASS → done

FAIL
  ↓
rtl_rework / FAILURE_REWORK
  ↓
RTL_AUTO_REWORK_APPLIED
  └── compile/lint/sim again

EXTERNAL_TEST_ISSUE
  ↓
freeze issue.json + exact transcript
  ↓
publish EXTERNAL_TEST_ISSUE
  ↓
wait for either:
    new VIP_READY responding to this issue
        → validate hash → rerun simulation
    CONTRACT_REVIEW_REQUIRED for this issue
        → both workflows stop for human review

any other RTL escalation
  └── optionally publish RTL_STOPPED → stop
```

## Standalone behavior

Interop must not make the RTL workflow dependent on VIP during normal use.

When coordinated mode is not active:

```text
EXTERNAL_TEST_ISSUE
    → keep normal RTL report behavior
    → stop
```

Do not create or update `../interop/` merely because an ordinary standalone RTL task runs.

## Sandbox bootstrap

Project `.codex/config.toml` grants the neutral sibling runtime root as an additional workspace-write root. Because relative paths in project config resolve from `.codex/`, the RTL project uses:

```toml
[sandbox_workspace_write]
writable_roots = ["../../interop"]
```

Workflow authority remains narrower than filesystem capability: RTL writes only `../../interop/rtl_to_vip` and reads `../../interop/vip_to_rtl`. The sibling `vip_*` repository is not writable through this setting.

If a host-managed sandbox refuses creation of the configured root before it exists, request approval only for the exact one-time bootstrap `mkdir -p ../interop/rtl_to_vip/issues` operation. Do not ask the user to pre-create interop manually and do not grant the whole workspace parent as writable.
