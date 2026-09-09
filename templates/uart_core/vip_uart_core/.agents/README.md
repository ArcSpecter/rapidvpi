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

# VIP AI Workflow v16

Minimal reusable VIP testcase-development and `.so` build flow.

v16 has a fast mechanical path for already-existing tests and keeps fresh workers only where fresh engineering context actually provides value.

- **COMPILE_ONLY:** coordinator directly selects existing `tc_*` cases and builds the `.so`; zero workers.
- **DEVELOPMENT:** one fresh `vip_testcase` worker per testcase; optional `vip_integration` for the requested retained final `.so`.

There are no coordination services, helper scripts, workflow report files, or simulator actions.

## Main flow

```mermaid
%%{init: {"theme":"base","themeVariables":{"background":"#2b3038","primaryTextColor":"#e5eefc","lineColor":"#9fb3c8","edgeLabelBackground":"#2b3038","fontFamily":"monospace","fontSize":"15px"}}}%%
flowchart TD
    U["User request"] --> R{"Existing tests only,<br/>or development?"}

    R -->|Compile / package only| C["Coordinator resolves applicable<br/>tc_* set"]
    C --> K{"Individual compile<br/>checks requested?"}
    K -->|Yes| N{"More tc_*?"}
    N -->|Yes| S["Enable one tc_* only<br/>disable others"]
    S --> B["Coordinator builds normal .so"]
    B --> N
    N -->|No| F{"Retained final .so<br/>requested?"}
    K -->|No| F
    F -->|Yes| Q{"Final selection"}
    Q -->|One| O["Enable one tc_*"]
    Q -->|Subset| P["Enable requested subset"]
    Q -->|Full regression| A["Enable all applicable tc_*<br/>for active DUT mode"]
    O --> X["Coordinator builds final .so"]
    P --> X
    A --> X
    X --> Z["FINAL_SO_READY"]
    F -->|No| D["Compile work complete"]

    R -->|Implement / fix tc_*| L["Coordinator resolves requested<br/>tc_* set"]
    L --> M{"More tc_*?"}
    M -->|Yes| T["Fresh vip_testcase worker<br/>for one assigned tc_*"]
    T --> W["Enable assigned tc_* only<br/>implement/fix + compile .so"]
    W --> M
    M -->|No| G{"Retained final .so<br/>requested?"}
    G -->|Yes| H{"Straightforward final<br/>selection/build?"}
    H -->|Yes| J["Coordinator selects final tc_* set<br/>and builds final .so"]
    H -->|No| I["Optional vip_integration<br/>final selection + build"]
    J --> Z
    I --> Z
    G -->|No| E["Development complete"]

    classDef start fill:#1d4ed8,stroke:#93c5fd,color:#eff6ff,stroke-width:2px;
    classDef decision fill:#7c3aed,stroke:#c4b5fd,color:#ffffff,stroke-width:2px;
    classDef fast fill:#0369a1,stroke:#7dd3fc,color:#f0f9ff,stroke-width:2px;
    classDef testcase fill:#147d74,stroke:#5eead4,color:#ecfeff,stroke-width:2px;
    classDef build fill:#b45309,stroke:#fdba74,color:#fff7ed,stroke-width:2px;
    classDef integration fill:#c2185b,stroke:#f9a8d4,color:#fff1f2,stroke-width:2px;
    classDef complete fill:#166534,stroke:#86efac,color:#f0fdf4,stroke-width:2px;
    classDef neutral fill:#374151,stroke:#aab4c3,color:#f8fafc,stroke-width:2px;

    class U start;
    class R,K,N,F,Q,M,G,H decision;
    class C,S,O,P,A,J fast;
    class B,X build;
    class L,T,W testcase;
    class I integration;
    class Z complete;
    class D,E neutral;
```

## COMPILE_ONLY fast path

Use this when testcase implementations already exist and the request is mechanical, for example:

- make sure each applicable `tc_*` compiles;
- build a `.so` with one named testcase;
- build a `.so` with a requested subset;
- enable all tests for a DUT mode and build the regression `.so`.

The coordinator does the work directly. It does **not** start `vip_testcase` or `vip_integration`.

For individual checking it repeatedly performs only:

1. enable one `tc_*` for the active mode and disable the others;
2. run the normal project build;
3. verify the `.so` exists;
4. move to the next testcase.

For the retained final artifact it enables exactly the requested final set and builds once more.

If compilation reveals an engineering defect requiring source changes, stop and report it unless the user also requested fixing/development.

## DEVELOPMENT worker path

Use fresh workers only when testcase behavior actually must be implemented or changed.

For `tc_foo`:

1. fresh `vip_testcase` context;
2. same project repository and same normal build directory;
3. enable/uncomment `tc_foo` and disable/comment the other active-mode `tc_*` selections;
4. implement/fix only `tc_foo` and genuinely necessary shared VIP support;
5. compile the normal VIP `.so`;
6. return `TC_READY` and exit.

Workers are strictly sequential. The intermediate `.so` is disposable and the next worker may overwrite it.

If a retained final `.so` is requested after development, one fresh `vip_integration` worker enables exactly the desired final testcase set and builds it.

## Explicit non-goals

This workflow does **not** run simulation and does **not** execute the `.so`. It does not inspect RTL. Its endpoint is a successfully compiled shared library in the project's normal output location.
