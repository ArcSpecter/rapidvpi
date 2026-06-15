# RapidVPI UART Core Template

This directory is a self-contained RapidVPI template project.

It is intentionally provided as a copy-and-modify starting point, not as a
standalone maintained UART IP product. The UART RTL and VIP are included to
demonstrate a realistic RapidVPI verification environment with driver,
monitor, scoreboard, randomized tests, reset tests, and documentation/scripts
layout.

Users are expected to copy this template and replace the UART-specific RTL/VIP
with their own DUT and verification components.

## What this template contains

This `uart_core` folder is a snapshot-style RapidVPI project built around a
simple UART core. It includes:

- `rtl_uart_core`: synthesizable UART RTL used as the example DUT
- `vip_uart_core`: RapidVPI UART verification project for driving,
  monitoring, and checking UART Core IP behavior
- `vip_common`: shared RapidVPI VIP support code used by the UART VIP
- `vip_uart`: UART-specific RapidVPI VIP package providing TX/RX agents, 
UART frame/byte helpers, scoreboard support, randomized stimulus hooks,
reset-aware behavior, and protocol-oriented checking for the `uart_core` template.
- testcase structure showing how to organize reusable cases, helpers, runner
  hooks, and scoreboards
- project documentation and agent/design guides showing the intended RTL and
  VIP development style

The purpose of this folder is to show how a real RapidVPI-based RTL/VIP project
is structured end-to-end. The UART protocol is only the example target; the main
value is the project layout, coroutine verification style, agent/scoreboard
split, and reusable documentation/script structure.

## Intended use

Copy this folder when starting a new RapidVPI-based RTL/VIP project, then replace
the UART-specific RTL, VIP, tests, and documentation with the new project logic.

Typical use:

```text
copy templates/uart_core -> my_new_project
remove/replace UART RTL
remove/replace UART VIP behavior
keep the project structure and RapidVPI verification style
adapt tests, scoreboards, docs, and scripts for the new DUT
```

This template is intentionally self-contained. The code under `ext/` is included
as normal source snapshot content for convenience, not as nested Git submodules.

## Maintenance policy

This template is good enough as a public RapidVPI demonstration and project
starter. It is not intended to be continuously updated as a feature-complete UART
IP product.
