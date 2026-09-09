# UART Core User Guide

## Table of Contents

- [1. Overview](#1-overview)
- [2. Public Entity](#2-public-entity)
- [3. Parameters](#3-parameters)
  - [3.1 Baud and Sampling Parameters](#31-baud-and-sampling-parameters)
  - [3.2 FIFO Parameters](#32-fifo-parameters)
  - [3.3 RTS/CTS Parameters](#33-rtscts-parameters)
  - [3.4 Debug Parameter](#34-debug-parameter)
- [4. Clock and Reset](#4-clock-and-reset)
- [5. UART Pins](#5-uart-pins)
- [6. Runtime Configuration Inputs](#6-runtime-configuration-inputs)
  - [6.1 Core Enables](#61-core-enables)
  - [6.2 Baud Increment](#62-baud-increment)
  - [6.3 Parity, Stop Bits, and Data Bits](#63-parity-stop-bits-and-data-bits)
  - [6.4 Hardware Flow-Control Enable](#64-hardware-flow-control-enable)
- [7. FIFO Control Inputs](#7-fifo-control-inputs)
- [8. TX Byte-Stream Interface](#8-tx-byte-stream-interface)
- [9. RX Byte-Stream Interface](#9-rx-byte-stream-interface)
- [10. RX Per-Byte Status Fields](#10-rx-per-byte-status-fields)
- [11. FIFO Status Outputs](#11-fifo-status-outputs)
- [12. UART Activity and Flow-Control Status](#12-uart-activity-and-flow-control-status)
- [13. Event Pulse Outputs](#13-event-pulse-outputs)
- [14. RTS/CTS Usage Model](#14-rtscts-usage-model)
- [15. Integration Notes](#15-integration-notes)
- [16. Simulation Wrapper and VIP Binding](#16-simulation-wrapper-and-vip-binding)

---

## 1. Overview

`uart_core` is the reusable UART physical core used by higher-level UART-family IPs.

It converts between raw UART serial pins and a simple byte-stream interface:

- TX path: upper layer provides bytes through `tx_byte_*`; `uart_core` serializes them onto `uart_tx_o`.
- RX path: `uart_core` samples `uart_rx_i`, reconstructs received bytes, stores them in the RX FIFO, and presents them through `rx_byte_*`.

`uart_core` is intentionally bus-neutral. It does not contain AXI, AXI-Lite, AXI-Stream, APB, Wishbone, TileLink, or command-protocol logic. It also does not own configuration registers. Configuration is supplied through live `cfg_*` inputs by the wrapper or register layer above it.

This makes the same core usable by both major UART IP categories:

| Usage | Example higher-level IP | How it uses `uart_core` |
| --- | --- | --- |
| Bus-controlled UART endpoint | `axil_uart`, `axi_uart`, `axis_uart` | Bus/register/stream logic pushes and pops bytes through the byte-stream interface |
| UART-commanded bus bridge | `uart_axil_bridge`, `uart_axi_bridge` | UART protocol decoder consumes RX bytes and response encoder produces TX bytes |

---

## 2. Public Entity

```systemverilog
module uart_core #(
  parameter int unsigned BAUD_ACC_W         = 32,   // cfg_baud_inc = round(baud * OVERSAMPLE * 2^BAUD_ACC_W / clk_hz)
  parameter int unsigned OVERSAMPLE         = 16,   // RX oversampling ratio

  parameter int unsigned RX_FIFO_DEPTH      = 16,   // RX FIFO entries, each entry is byte + RX metadata
  parameter int unsigned TX_FIFO_DEPTH      = 16,   // TX FIFO entries, each entry is one byte

  parameter bit          HAS_RTS_CTS        = 1'b0, // Synthesis-time RTS/CTS feature enable
  parameter bit          RTS_ACTIVE_LOW     = 1'b1, // Physical RTS pin is asserted low
  parameter bit          CTS_ACTIVE_LOW     = 1'b1, // Physical CTS pin is asserted low

  parameter int unsigned RTS_DEASSERT_LEVEL = RX_FIFO_DEPTH - 2, // Deassert RTS when RX FIFO reaches this level
  parameter int unsigned RTS_ASSERT_LEVEL   = RX_FIFO_DEPTH / 2, // Reassert RTS after RX FIFO drains to this level

  parameter bit          RTL_DBG            = 1'b1  // Enables guarded RTL debug printouts in simulation
) (
  input  wire                                      clk,
  input  wire                                      rst_n,

  // Raw UART serial pins.
  input  wire                                      uart_rx_i,
  output wire                                      uart_tx_o,

  // Optional hardware flow-control pins.
  // These ports stay present even when HAS_RTS_CTS=0.
  input  wire                                      uart_cts_i,
  output wire                                      uart_rts_o,

  // Live runtime configuration.
  // uart_core consumes these directly and does not own config registers.
  input  wire                                      cfg_enable,
  input  wire                                      cfg_rx_enable,
  input  wire                                      cfg_tx_enable,
  input  wire [BAUD_ACC_W-1:0]                     cfg_baud_inc,
  input  wire [1:0]                                cfg_parity_mode,
  input  wire [1:0]                                cfg_stop_bits,
  input  wire [1:0]                                cfg_data_bits,
  input  wire                                      cfg_hw_flow_enable,

  // FIFO clear pulses from the wrapper/register layer.
  input  wire                                      ctrl_rx_fifo_clear,
  input  wire                                      ctrl_tx_fifo_clear,

  // TX byte stream: upper layer -> uart_core.
  // Handshake accepts bytes into the TX FIFO, not necessarily directly into the TX shifter.
  input  wire                                      tx_byte_valid,
  output wire                                      tx_byte_ready,
  input  wire [7:0]                                tx_byte_data,

  // RX byte stream: uart_core -> upper layer.
  // Handshake pops one RX FIFO record: data byte plus per-byte status fields below.
  output wire                                      rx_byte_valid,
  input  wire                                      rx_byte_ready,
  output wire [7:0]                                rx_byte_data,

  // RX per-byte status.
  // These fields describe the same RX FIFO record currently presented with rx_byte_valid.
  output wire                                      rx_byte_frame_error,
  output wire                                      rx_byte_parity_error,
  output wire                                      rx_byte_break_detect,

  // FIFO status.
  output wire [$clog2(RX_FIFO_DEPTH + 1)-1:0]      rx_fifo_level,
  output wire [$clog2(TX_FIFO_DEPTH + 1)-1:0]      tx_fifo_level,
  output wire                                      rx_fifo_empty,
  output wire                                      rx_fifo_full,
  output wire                                      tx_fifo_empty,
  output wire                                      tx_fifo_full,

  // UART activity and flow-control status.
  output wire                                      rx_busy,
  output wire                                      tx_busy,
  output wire                                      cts_active,
  output wire                                      rts_active,
  output wire                                      cts_blocked,

  // Immediate one-cycle core event pulses.
  // These are detection-time events, not FIFO-output metadata.
  output wire                                      event_rx_overrun,
  output wire                                      event_rx_frame_error,
  output wire                                      event_rx_parity_error,
  output wire                                      event_rx_break_detect,
  output wire                                      event_tx_done
);

endmodule
```

---

## 3. Parameters

### 3.1 Baud and Sampling Parameters

| Parameter | Default | Description |
| --- | ---: | --- |
| `BAUD_ACC_W` | `32` | Width of the live fractional baud increment input `cfg_baud_inc`. |
| `OVERSAMPLE` | `16` | RX oversampling ratio. `16` means the RX sampling logic uses sixteen sample ticks per UART bit time. |

`OVERSAMPLE=16` is the normal default for robust UART RX sampling. Higher-level wrappers normally do not need to change it unless they intentionally want a different UART sampling architecture.

`cfg_baud_inc` is supplied live by the wrapper layer. `uart_core` does not own a reset-default baud register.

The baud increment is calculated by the wrapper/software/VIP layer:

```text
cfg_baud_inc = round(baud_rate * OVERSAMPLE * 2^BAUD_ACC_W / clk_hz)
```

The average generated oversample tick rate is:

```text
oversample_tick_rate = clk_hz * cfg_baud_inc / 2^BAUD_ACC_W
```

The average UART baud is:

```text
baud_actual = oversample_tick_rate / OVERSAMPLE
```

`cfg_baud_inc=0` disables baud ticks. A value near all-ones generates an oversample tick on almost every `clk` cycle. Exact tick-every-clock behavior is not required for normal UART operation.

### 3.2 FIFO Parameters

| Parameter | Default | Description |
| --- | ---: | --- |
| `RX_FIFO_DEPTH` | `16` | Number of RX FIFO entries. Each RX entry contains one received byte plus per-byte RX metadata. |
| `TX_FIFO_DEPTH` | `16` | Number of TX FIFO entries. Each TX entry contains one byte. |

RX FIFO entries are wider than one byte internally because the per-byte status fields must remain attached to the received byte:

- `rx_byte_frame_error`
- `rx_byte_parity_error`
- `rx_byte_break_detect`

TX FIFO entries only need to store the byte payload because TX-side status is reported separately through activity and event outputs.

The FIFO level output widths are derived from these depths:

```systemverilog
$clog2(RX_FIFO_DEPTH + 1)
$clog2(TX_FIFO_DEPTH + 1)
```

### 3.3 RTS/CTS Parameters

| Parameter | Default | Description |
| --- | ---: | --- |
| `HAS_RTS_CTS` | `1'b0` | Synthesis-time enable for RTS/CTS hardware flow-control logic. |
| `RTS_ACTIVE_LOW` | `1'b1` | Defines the physical polarity of `uart_rts_o`. |
| `CTS_ACTIVE_LOW` | `1'b1` | Defines the physical polarity of `uart_cts_i`. |
| `RTS_DEASSERT_LEVEL` | `RX_FIFO_DEPTH - 2` | RX FIFO level where RTS deasserts, telling the peer to stop sending. |
| `RTS_ASSERT_LEVEL` | `RX_FIFO_DEPTH / 2` | RX FIFO level where RTS asserts again after the FIFO drains. |

`HAS_RTS_CTS` does not remove ports. The `uart_cts_i` and `uart_rts_o` ports are always present. When `HAS_RTS_CTS=0`, CTS is ignored, TX is never blocked by CTS, and RTS is driven inactive.

`RTS_DEASSERT_LEVEL` and `RTS_ASSERT_LEVEL` provide hysteresis. The usual relationship is:

```text
RTS_ASSERT_LEVEL < RTS_DEASSERT_LEVEL
```

For meaningful RTS hysteresis, `RX_FIFO_DEPTH` should be large enough that these two thresholds are separated.

### 3.4 Debug Parameter

| Parameter | Default | Description |
| --- | ---: | --- |
| `RTL_DBG` | `1'b1` | Enables standardized, synthesis-guarded `[RTL]` debug printouts and is passed through unchanged by the simulation wrapper. |

---

## 4. Clock and Reset

| Port | Direction | Description |
| --- | --- | --- |
| `clk` | Input | Main synchronous clock for the UART core. All internal logic runs in this domain. |
| `rst_n` | Input | Active-low reset. |

The UART RX serial input and CTS input are external asynchronous signals. `uart_core` handles the required synchronization internally before using them in the `clk` domain.

This table describes direct synthesis integration of `uart_core`. In the
verification hierarchy described in Section 16, `clk` is generated internally
by `dut_wrapper` and is no longer a public wrapper input. `rst_n` remains a
VIP-driven wrapper input.

---

## 5. UART Pins

| Port | Direction | Description |
| --- | --- | --- |
| `uart_rx_i` | Input | Raw UART RX serial input from the external peer. |
| `uart_tx_o` | Output | Raw UART TX serial output to the external peer. |
| `uart_cts_i` | Input | Optional clear-to-send input from the peer. Always present as a port. |
| `uart_rts_o` | Output | Optional request-to-send output to the peer. Always present as a port. |

`uart_rx_i` and `uart_tx_o` are the mandatory UART serial pins.

`uart_cts_i` and `uart_rts_o` are optional hardware flow-control pins. They remain in the entity even when RTS/CTS support is disabled, so the module interface stays stable across configurations.

---

## 6. Runtime Configuration Inputs

`uart_core` consumes live configuration inputs. It does not own configuration registers or reset-default configuration parameters.

A Type A wrapper such as `axil_uart` may drive these from an AXI-Lite register bank. A Type B bridge such as `uart_axil_bridge` may drive them from top-level parameters, static wires, or its own small configuration layer.

### 6.1 Core Enables

| Port | Description |
| --- | --- |
| `cfg_enable` | Global enable for the core. |
| `cfg_rx_enable` | Enables RX operation. |
| `cfg_tx_enable` | Enables TX operation. |

`cfg_enable` is the top-level operational gate. `cfg_rx_enable` and `cfg_tx_enable` allow RX and TX to be independently enabled by the wrapper layer.

### 6.2 Baud Increment

| Port | Description |
| --- | --- |
| `cfg_baud_inc` | Live fractional baud increment used by the baud/tick generator. Width is controlled by `BAUD_ACC_W`. |

`uart_core` uses `cfg_baud_inc` as a fractional/NCO baud control word, not as an integer divider. The wrapper supplies this value directly to `uart_core`.

Use this formula to calculate it:

```text
cfg_baud_inc = round(baud_rate * OVERSAMPLE * 2^BAUD_ACC_W / clk_hz)
```

For example, with `clk_hz=100_000_000`, `baud_rate=921_600`, `OVERSAMPLE=16`, and `BAUD_ACC_W=32`, the wrapper/VIP calculates `cfg_baud_inc` from that formula and drives the result into the port.

### 6.3 Parity, Stop Bits, and Data Bits

| Port | Description |
| --- | --- |
| `cfg_parity_mode` | Selects parity behavior. Encoding comes from `uart_pkg.sv`. |
| `cfg_stop_bits` | Selects stop-bit behavior. Encoding comes from `uart_pkg.sv`. |
| `cfg_data_bits` | Selects UART data-bit width. Encoding comes from `uart_pkg.sv`. |

These are live inputs. The wrapper should hold them stable during normal operation. If the wrapper changes these fields while the UART is actively receiving or transmitting, the wrapper is responsible for defining that system-level behavior.

The byte-stream data path remains 8 bits wide regardless of the selected UART data-bit mode. For data modes below 8 bits, the implementation treats only the selected number of low bits as meaningful.

### 6.4 Hardware Flow-Control Enable

| Port | Description |
| --- | --- |
| `cfg_hw_flow_enable` | Runtime enable for RTS/CTS behavior when `HAS_RTS_CTS=1`. |

`HAS_RTS_CTS` is the synthesis-time feature knob. `cfg_hw_flow_enable` is the runtime control input.

When `HAS_RTS_CTS=0`, `cfg_hw_flow_enable` has no effect.

---

## 7. FIFO Control Inputs

| Port | Description |
| --- | --- |
| `ctrl_rx_fifo_clear` | Pulse from wrapper layer to clear RX FIFO contents. |
| `ctrl_tx_fifo_clear` | Pulse from wrapper layer to clear TX FIFO contents. |

These controls are intended for wrapper/register-layer use. For example, a bus-controlled UART peripheral may expose software-controlled FIFO clear bits and convert those writes into one-cycle pulses to `uart_core`.

Clearing the RX FIFO discards stored RX byte records and their associated per-byte status metadata. Clearing the TX FIFO discards queued TX bytes that have not yet been launched into the TX shifter.

---

## 8. TX Byte-Stream Interface

| Port | Direction | Description |
| --- | --- | --- |
| `tx_byte_valid` | Input | Upper layer presents a byte for transmission. |
| `tx_byte_ready` | Output | `uart_core` can accept a byte into the TX FIFO. |
| `tx_byte_data` | Input | Byte to enqueue for UART transmission. |

The TX byte-stream interface uses ready/valid handshake semantics.

A TX byte is accepted when:

```systemverilog
tx_byte_valid && tx_byte_ready
```

This handshake accepts a byte into the TX FIFO. It does not mean the byte immediately starts serial transmission on `uart_tx_o`. The TX shifter launches bytes from the TX FIFO according to UART timing, `cfg_tx_enable`, `cfg_enable`, and optional CTS flow-control state.

---

## 9. RX Byte-Stream Interface

| Port | Direction | Description |
| --- | --- | --- |
| `rx_byte_valid` | Output | `uart_core` is presenting one RX FIFO record. |
| `rx_byte_ready` | Input | Upper layer accepts/pops the presented RX FIFO record. |
| `rx_byte_data` | Output | Received byte payload from the current RX FIFO record. |

The RX byte-stream interface uses ready/valid handshake semantics.

An RX record is consumed when:

```systemverilog
rx_byte_valid && rx_byte_ready
```

Each RX FIFO record contains:

- received byte data
- frame-error status for that byte
- parity-error status for that byte
- break-detect status for that byte

---

## 10. RX Per-Byte Status Fields

| Port | Description |
| --- | --- |
| `rx_byte_frame_error` | Frame error status attached to the RX FIFO record currently presented with `rx_byte_valid`. |
| `rx_byte_parity_error` | Parity error status attached to the RX FIFO record currently presented with `rx_byte_valid`. |
| `rx_byte_break_detect` | Break-detect status attached to the RX FIFO record currently presented with `rx_byte_valid`. |

These outputs are FIFO-output metadata. They describe the same RX record as `rx_byte_data`.

They are not immediate detection-time pulses. They remain associated with the stored byte until that byte record is popped by the upper layer.

Example meaning:

```text
rx_byte_valid        = 1
rx_byte_data         = 8'h41
rx_byte_parity_error = 1
```

This means the currently presented RX FIFO record contains byte `8'h41`, and that byte was received with a parity error.

---

## 11. FIFO Status Outputs

| Port | Description |
| --- | --- |
| `rx_fifo_level` | Number of entries currently stored in the RX FIFO. |
| `tx_fifo_level` | Number of entries currently stored in the TX FIFO. |
| `rx_fifo_empty` | RX FIFO contains no stored records. |
| `rx_fifo_full` | RX FIFO cannot accept another received byte record. |
| `tx_fifo_empty` | TX FIFO contains no queued bytes. |
| `tx_fifo_full` | TX FIFO cannot accept another TX byte from the upper layer. |

The level outputs count FIFO entries, not bits or serial characters currently in the RX/TX shifters.

`tx_fifo_empty` does not necessarily mean the UART TX pin is idle, because a byte may already be active in the TX shifter. Full TX idle is indicated by the combination of TX FIFO empty and TX engine not busy:

```systemverilog
tx_fifo_empty && !tx_busy
```

---

## 12. UART Activity and Flow-Control Status

| Port | Description |
| --- | --- |
| `rx_busy` | RX engine is currently receiving a UART character. |
| `tx_busy` | TX engine is currently serializing a UART character. |
| `cts_active` | Normalized CTS state; peer currently permits this core to transmit. |
| `rts_active` | Normalized RTS state; this core currently permits the peer to transmit. |
| `cts_blocked` | TX has a byte ready to launch, but CTS is preventing the next character launch. |

`cts_active` is a physical permission state after synchronization and polarity normalization.

`cts_blocked` is a stall/status condition. It only asserts when the TX path actually wants to launch a new UART character but CTS is inactive.

So these are different:

```text
cts_active = 0
```

means the peer is not permitting transmission.

```text
cts_blocked = 1
```

means the core wanted to transmit, but was blocked by CTS.

If CTS is inactive while the TX FIFO is empty, `cts_blocked` should remain `0` because no TX byte is waiting to launch.

---

## 13. Event Pulse Outputs

| Port | Description |
| --- | --- |
| `event_rx_overrun` | One-cycle pulse when a received byte record cannot be stored because RX FIFO is full. |
| `event_rx_frame_error` | One-cycle pulse when the RX engine detects a frame error. |
| `event_rx_parity_error` | One-cycle pulse when the RX engine detects a parity error. |
| `event_rx_break_detect` | One-cycle pulse when the RX engine detects a break condition. |
| `event_tx_done` | One-cycle pulse when one UART character has fully completed transmission, including stop bit. |

These are immediate core event pulses. They occur at detection/completion time.

They are different from RX per-byte status fields:

| Signal type | Meaning |
| --- | --- |
| `event_rx_parity_error` | A parity error was detected now. |
| `rx_byte_parity_error` | The currently presented RX FIFO record had a parity error when it was received. |

`event_rx_overrun` has no matching per-byte RX field because an overrun means the new received byte record could not be stored.

---

## 14. RTS/CTS Usage Model

RTS/CTS support has two levels of control:

| Control | Type | Meaning |
| --- | --- | --- |
| `HAS_RTS_CTS` | Parameter | Includes or removes RTS/CTS logic at synthesis time. Ports remain stable. |
| `cfg_hw_flow_enable` | Runtime input | Enables/disables flow-control behavior when the hardware feature exists. |

When `HAS_RTS_CTS=0`:

- `uart_cts_i` is ignored.
- TX is never blocked by CTS.
- `uart_rts_o` is driven inactive according to `RTS_ACTIVE_LOW`.
- `cts_active` is benign, normally treated as active/allowed.
- `rts_active` is benign, normally treated as inactive.
- `cts_blocked` remains `0`.

When `HAS_RTS_CTS=1` and `cfg_hw_flow_enable=1`:

- CTS is synchronized and polarity-normalized.
- CTS only gates the launch of the next UART character.
- CTS does not interrupt a character already being serialized.
- RTS is generated from RX FIFO level using `RTS_DEASSERT_LEVEL` and `RTS_ASSERT_LEVEL`.

RTS behavior:

```text
RX FIFO reaches RTS_DEASSERT_LEVEL:
  deassert RTS, telling peer to stop sending

RX FIFO drains to RTS_ASSERT_LEVEL:
  assert RTS again, allowing peer to send
```

The physical polarity of `uart_rts_o` is controlled by `RTS_ACTIVE_LOW`.

The physical polarity of `uart_cts_i` is controlled by `CTS_ACTIVE_LOW`.

---

## 15. Integration Notes

`uart_core` is a byte-stream UART PHY core. Higher layers are responsible for system-specific behavior such as:

- configuration registers
- reset-default configuration values
- interrupt masking/latching/clearing
- AXI/AXI-Lite/APB/Wishbone/TileLink interfaces
- UART command framing protocols
- software-visible register maps

For a bus-controlled UART endpoint, the wrapper typically maps register writes and reads into the `tx_byte_*` and `rx_byte_*` interfaces.

For a UART-commanded bridge, the wrapper typically feeds `rx_byte_*` into a frame decoder and feeds response bytes back through `tx_byte_*`.

The `uart_core` interface is stable across RTS/CTS configurations. Do not rely on conditional ports or preprocessor changes for flow-control variants.

---

## 16. Simulation Wrapper and VIP Binding

Verification must elaborate `dut_wrapper`, not the bare `uart_core` module. The
root `dut_wrapper.sv` is simulation-only and excluded from synthesis and reusable
RTL export manifests. It instantiates the real synthesizable top exactly once:

```text
dut_wrapper
├── sim_native_clock_gen u_sim_clk
└── uart_core u_dut
```

The VIP DUT name is `dut_wrapper`. Functional DUT-internal diagnostics descend
through `dut_wrapper.u_dut`; wrapper ports and native-clock infrastructure are
directly below `dut_wrapper`.

### Wrapper parameters and functional ports

`dut_wrapper` mirrors and explicitly passes these `uart_core` parameters without
changing their types or defaults:

| Parameter | Type | Default |
| --- | --- | --- |
| `BAUD_ACC_W` | `int unsigned` | `32` |
| `OVERSAMPLE` | `int unsigned` | `16` |
| `RX_FIFO_DEPTH` | `int unsigned` | `16` |
| `TX_FIFO_DEPTH` | `int unsigned` | `16` |
| `HAS_RTS_CTS` | `bit` | `1'b0` |
| `RTS_ACTIVE_LOW` | `bit` | `1'b1` |
| `CTS_ACTIVE_LOW` | `bit` | `1'b1` |
| `RTS_DEASSERT_LEVEL` | `int unsigned` | `RX_FIFO_DEPTH - 2` |
| `RTS_ASSERT_LEVEL` | `int unsigned` | `RX_FIFO_DEPTH / 2` |
| `RTL_DBG` | `bit` | `1'b1` |

The wrapper preserves every functional `uart_core` port except `clk`, which is
generated internally. All ports are unsigned packed scalar/vector wires with no
unpacked dimensions.

| Wrapper-visible ports | Direction | Width |
| --- | --- | ---: |
| `rst_n`, `uart_rx_i`, `uart_cts_i` | Input | 1 each |
| `uart_tx_o`, `uart_rts_o` | Output | 1 each |
| `cfg_enable`, `cfg_rx_enable`, `cfg_tx_enable`, `cfg_hw_flow_enable` | Input | 1 each |
| `cfg_baud_inc` | Input | `BAUD_ACC_W` |
| `cfg_parity_mode`, `cfg_stop_bits`, `cfg_data_bits` | Input | 2 each |
| `ctrl_rx_fifo_clear`, `ctrl_tx_fifo_clear` | Input | 1 each |
| `tx_byte_valid`, `tx_byte_data` | Input | 1, 8 |
| `tx_byte_ready` | Output | 1 |
| `rx_byte_ready` | Input | 1 |
| `rx_byte_valid`, `rx_byte_data` | Output | 1, 8 |
| `rx_byte_frame_error`, `rx_byte_parity_error`, `rx_byte_break_detect` | Output | 1 each |
| `rx_fifo_level` | Output | `$clog2(RX_FIFO_DEPTH + 1)` |
| `tx_fifo_level` | Output | `$clog2(TX_FIFO_DEPTH + 1)` |
| `rx_fifo_empty`, `rx_fifo_full`, `tx_fifo_empty`, `tx_fifo_full` | Output | 1 each |
| `rx_busy`, `tx_busy`, `cts_active`, `rts_active`, `cts_blocked` | Output | 1 each |
| `event_rx_overrun`, `event_rx_frame_error`, `event_rx_parity_error`, `event_rx_break_detect`, `event_tx_done` | Output | 1 each |

`rst_n` is the active-low, wrapper-visible reset and remains VIP driven. The
wrapper neither generates nor modifies reset. DUT sequential logic uses reset
synchronously to the internally generated `clk`, so the VIP must run the clock
while applying and releasing reset according to the established test sequence.

There is no compile-time interface-selection macro. Questa and Verilator expose
the same wrapper port set. `HAS_RTS_CTS` selects flow-control logic by parameter
(default `1'b0`); the RTS/CTS pins remain present in either setting.

### Native `clk` contract

The real-DUT input clock `clk` is generated inside `dut_wrapper`; it is not a
public wrapper input and must never be driven edge-by-edge by the VIP. The VIP
registers the following VPI-visible signals and constructs one
`vip::common::Clock` using `NativeClockCfg`:

| Role | Exact VPI-visible name | Width | Initial/default behavior |
| --- | --- | ---: | --- |
| Generated waveform | `clk` | 1 | Parked low |
| Run/stop control | `sim_clk_enable` | 1 | `0` (disabled) |
| Full-period control | `sim_clk_period_ticks` | 64 | `10000` ticks (10 ns / 100 MHz nominal) |
| Stopped acknowledgement | `sim_clk_stopped` | 1 | `1` when stopped and low |

One period tick is 1 ps. The VIP writes `sim_clk_period_ticks` before changing
`sim_clk_enable` from 0 to 1. That enable write produces the first rising edge in
the same simulation time slot, which preserves deterministic `start_at()`
behavior. A stop request takes effect at the next half-cycle boundary; the clock
parks low before `sim_clk_stopped` asserts. Periods below two ticks are clamped
to two ticks.

There are no DUT-generated or protocol-generated clock ports in `uart_core`, so
there are no observation-only clock outputs and no additional native-clock
control triplets. `sim_keepalive` is wrapper timing-queue infrastructure only.
The VIP must never register, drive, await, or treat it as testcase state.

### Simulator and plugin prerequisites

The RTL-side compile targets are:

```bash
cmake --build cmake-build-release --target sim_questa_init
cmake --build cmake-build-release --target sim_questa_compile
cmake --build cmake-build-release --target sim_verilator_compile
```

Questa loads the separately built sibling plugin from:

```text
../vip_uart_core/cmake-build-release/libvip_uart_core.so
```

Verilator requires the Verilator-flavor sibling plugin produced by target
`verilator_uart_core` at:

```text
../vip_uart_core/cmake-build-verilator/libvip_uart_core.so
```

The two plugin flavors are not interchangeable. The RTL repository does not
configure or build the sibling VIP. The Verilator executable is generated at
`cmake-build-release/verilator/obj_dir/sim_verilator` and loads the latter plugin
with `+verilator+vpi+<absolute-plugin-path>`.
