---
name: vip-workflow
description: Minimal VIP workflow with a direct coordinator fast path for existing-test compile/package work, and fresh sequential workers only for actual tc_* development/fixing. This workflow only produces .so files; it never runs simulation or inspects RTL.
---

# VIP workflow

You are the main coordinator for a `vip_*` project.

There are only two execution paths:

1. **COMPILE_ONLY** — requested tests already exist; the coordinator selects/builds them directly with no workers.
2. **DEVELOPMENT** — testcase implementation/fixing is requested; use one fresh sequential `vip_testcase` worker per testcase, then optional `vip_integration` for the retained final `.so`.

Nothing else belongs in this workflow.

## Hard boundaries

- Work only in the current `vip_*` repository.
- Never inspect/read/search/modify sibling `rtl_*` repositories or RTL source.
- Never run simulation. Do not invoke Questa, VVP, RTL runners, regressions, or execute the generated `.so`.
- Never add or modify simulation-completion behavior.
- Do not refactor, optimize, modernize, clean up, rename, or alter unrelated code/APIs/files.
- Never change testcase expectations merely to match DUT behavior.

## Step 1: resolve the request once

Determine only the information needed to execute the request:

- requested active DUT mode/profile/generic/defines, if relevant;
- requested/applicable `tc_*` list;
- whether the request is COMPILE_ONLY or DEVELOPMENT;
- testcase selection source, normally `src/test.cpp`;
- existing normal CMake build directory;
- build target producing the VIP `.so`;
- expected `.so` path.

Use project-local authority such as `docs/verification_plan.md`, `docs/user_guide.md`, existing `test.cpp`, `CMakeLists.txt`, and the existing CMake build cache. Do not inspect RTL to resolve anything.

If the user names the DUT mode explicitly, honor it. For a full regression request, the testcase set is all `tc_*` cases documented/applicable for that active mode. For an explicit list/subset, use exactly that list.

Classify semantically:

- **COMPILE_ONLY:** tests already exist and the user asks only to compile, compile-check, build, enable/select, package, prepare a `.so`, or prepare a regression.
- **DEVELOPMENT:** the user asks to implement, add, develop, fix, repair, rework, or change testcase/shared-VIP behavior.

Do not spawn a worker merely to perform testcase selection or a CMake build.

## Step 2A: COMPILE_ONLY fast path — coordinator only

For COMPILE_ONLY, the coordinator performs all selection/build operations directly in the current repository.

**Do not spawn `vip_testcase`. Do not spawn `vip_integration`.**

If individual testcase compile checking is requested:

1. For the first requested/applicable `tc_*`, edit the existing selection mechanism so only that testcase is enabled for the active mode.
2. Build the normal project `.so` using the existing build directory/target.
3. Verify the build succeeded and the expected `.so` exists.
4. Repeat directly for each remaining testcase, one at a time.

Intermediate `.so` files are disposable and may overwrite the same normal artifact.

If a retained final `.so` is requested after the checks, set the final selection directly:

- one testcase -> enable exactly that one;
- explicit subset -> enable exactly that subset;
- full regression -> enable all `tc_*` applicable to the active mode.

Then build the normal project `.so` once more and leave it at the normal output path.

Do not create per-test build trees, clones, worktrees, report files, helper services, or saved intermediate artifacts.

If a compile fails and fixing source/testcase/shared-support code would be required, stop and report the failing testcase/build error unless the user's request also authorizes DEVELOPMENT. Do not silently escalate a compile-only request into engineering changes.

## Step 2B: DEVELOPMENT path — fresh worker per testcase

Use this path only when actual testcase/shared-VIP engineering is requested.

For every testcase that must be implemented/fixed:

1. Start exactly one fresh `vip_testcase` worker.
2. Pass PROJECT_ROOT, ACTIVE_MODE, TC_NAME, SELECTION_FILE, BUILD_DIR, BUILD_TARGET, ARTIFACT_PATH.
3. The worker works in the same project repository, not a clone/worktree/copy.
4. The worker enables only its assigned `tc_*` for the active mode and disables the other `tc_*` selections.
5. The worker implements/fixes only that testcase and genuinely necessary shared support.
6. The worker builds the normal project `.so`.
7. Require `TC_READY` before starting the next testcase.
8. If it returns `TC_BLOCKED`, stop and report the blocker.

Run workers strictly sequentially. Do not start multiple testcase workers at once.

Do not create per-test build trees or save intermediate `.so` files. Each successful build may overwrite the same normal output artifact.

Keep coordination quiet: start one worker, wait for its final result, then start the next. Do not repeatedly emit progress commentary.

## Step 3: final `.so` after DEVELOPMENT

If DEVELOPMENT completed and the user wants a retained final `.so`, start one fresh `vip_integration` worker.

Pass FINAL_TC_LIST exactly as requested:

- one testcase -> exactly that one;
- explicit subset -> exactly that subset;
- full regression -> all `tc_*` applicable to the active mode.

The integration worker changes only testcase selection as needed and builds the final `.so` in the same normal build directory.

If the user did not request a retained final selection/build, stop after the testcase workers.

## Completion

Success is only about compilation and artifact production:

- COMPILE_ONLY: the coordinator performed the requested individual selection/build checks and produced the requested final `.so`, with no workers; or
- DEVELOPMENT: each requested testcase was handled by its fresh worker and any requested final selection was assembled afterward.

Do not run the `.so`. Simulation belongs to a different workflow/domain.
