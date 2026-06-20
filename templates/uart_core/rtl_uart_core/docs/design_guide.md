# UART Core Design Guide

## Table of Contents

- [1. General Information](#1-general-information)
- [2. File Map](#2-file-map)
- [3. Public `uart_core.sv` Contract](#3-public-uart_coresv-contract)
- [4. Top-Level Signal Semantics](#4-top-level-signal-semantics)
  - [4.1 Configuration Signals](#41-configuration-signals)
  - [4.2 TX Byte Stream](#42-tx-byte-stream)
  - [4.3 RX Byte Stream and Per-Byte Status](#43-rx-byte-stream-and-per-byte-status)
  - [4.4 Event Pulses](#44-event-pulses)
  - [4.5 FIFO Status](#45-fifo-status)
  - [4.6 RTS/CTS Status](#46-rtscts-status)
- [5. Design-Wide Architecture](#5-design-wide-architecture)
- [6. `uart_pkg.sv`](#6-uart_pkgsv)
- [7. `uart_rx_sync.sv`](#7-uart_rx_syncsv)
- [8. `uart_baud_gen.sv`](#8-uart_baud_gensv)
- [9. `uart_rx.sv`](#9-uart_rxsv)
- [10. `uart_tx.sv`](#10-uart_txsv)
- [11. `uart_byte_fifo.sv`](#11-uart_byte_fifosv)
- [12. `uart_hw_flow_ctrl.sv`](#12-uart_hw_flow_ctrlsv)
- [13. `uart_core.sv` Glue Logic](#13-uart_coresv-glue-logic)
- [14. Reset, Clear, and Enable Behavior](#14-reset-clear-and-enable-behavior)
- [15. Implementation Rules for Codex](#15-implementation-rules-for-codex)
- [16. Bring-Up and Verification Checklist](#16-bring-up-and-verification-checklist)

---

## 1. General Information

`uart_core` is the reusable physical UART core used by both Type A and Type B UART-related IPs.

It owns only the physical UART byte-pipe layer:

```text
raw UART pins <-> synchronization / baud timing / RX decode / TX encode / FIFOs <-> byte-stream interface
```

It must not contain AXI, AXI-Lite, AXI-Stream, register-bank, command-frame, CRC, opcode, or bridge-specific logic.

The core consumes live configuration inputs. It does not own configuration registers. Reset defaults for baud rate, parity, stop bits, data bits, FIFO thresholds, interrupt masks, and software-visible register values belong in the wrapper or register-control layer above this core.

Examples:

```text
Type A axil_uart:
  AXI-Lite slave registers -> cfg_* / tx_byte_* / rx_byte_* -> uart_core -> raw UART pins

Type B uart_axil_bridge:
  raw UART pins -> uart_core -> byte stream -> framed UART protocol -> AXI-Lite master
```

`uart_core` must expose a stable port list. Optional RTS/CTS support is controlled by parameters and generate logic, not by `ifdef` interface changes.

---

## 2. File Map

| IP group    | File                   | Purpose                                                                                                                                    |
| ----------- | ---------------------- | ------------------------------------------------------------------------------------------------------------------------------------------ |
| `uart_core` | `uart_pkg.sv`          | Common UART parameters/constants: parity modes, stop-bit modes, data-bit modes, flow-control modes, default reset values                   |
|             | `uart_rx_sync.sv`      | Synchronizes asynchronous `uart_rx_i` into the local clock domain before RX decoding; optional simple glitch filter can live here later    |
|             | `uart_baud_gen.sv`     | Generates UART baud / oversampling ticks                                                                                                   |
|             | `uart_rx.sv`           | Physical UART RX serial decoder, outputs received bytes and RX error flags                                                                 |
|             | `uart_tx.sv`           | Physical UART TX serial encoder, consumes bytes and serializes them onto `uart_tx_o`                                                       |
|             | `uart_byte_fifo.sv`    | Small byte FIFO for RX/TX buffering                                                                                                        |
|             | `uart_hw_flow_ctrl.sv` | Optional RTS/CTS hardware flow-control helper: synchronizes CTS, normalizes polarity, gates TX start, and generates RTS from RX FIFO level |
|             | `uart_core.sv`         | Wraps RX sync, baud generator, RX, TX, FIFOs, and optional RTS/CTS flow control into a clean byte-stream interface                         |

---

## 3. Public `uart_core.sv` Contract

`uart_core.sv` is the top-level file of this reusable core. Use `input wire` / `output wire` style for module IO.

The entity below is the intended public contract.

```systemverilog
module uart_core #(
  parameter int unsigned BAUD_ACC_W         = 32,   // cfg_baud_inc = round(baud_rate * OVERSAMPLE * 2^BAUD_ACC_W / clk_hz)
  parameter int unsigned OVERSAMPLE         = 16,   // RX oversampling ratio

  parameter int unsigned RX_FIFO_DEPTH      = 16,   // RX FIFO entries, each entry is byte + RX metadata
  parameter int unsigned TX_FIFO_DEPTH      = 16,   // TX FIFO entries, each entry is one byte

  parameter bit          HAS_RTS_CTS        = 1'b0, // Synthesis-time RTS/CTS feature enable
  parameter bit          RTS_ACTIVE_LOW     = 1'b1, // Physical RTS pin is asserted low
  parameter bit          CTS_ACTIVE_LOW     = 1'b1, // Physical CTS pin is asserted low

  parameter int unsigned RTS_DEASSERT_LEVEL = RX_FIFO_DEPTH - 2, // Deassert RTS when RX FIFO reaches this level
  parameter int unsigned RTS_ASSERT_LEVEL   = RX_FIFO_DEPTH / 2, // Reassert RTS after RX FIFO drains to this level

  // ================================================================
  // DEBUG CONTROL
  // ================================================================
  parameter bit RTL_DBG = 1'b1 // Enables guarded RTL debug printouts in simulation
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

Do not add public generics for derived widths such as `RX_LEVEL_W` or `TX_LEVEL_W`. Derive them internally with localparams when needed.

Do not add public reset-default generics such as `BAUD_INC_RESET`, `PARITY_MODE_RESET`, `STOP_BITS_RESET`, or `DATA_BITS_RESET`. Reset defaults belong in wrappers/register layers above this core.

Do not add a public `CTS_SYNC_STAGES` generic. The CTS synchronizer depth is internal to `uart_hw_flow_ctrl.sv`; use a fixed conservative localparam there.

The `RTL_DBG` generic is the simulation debug control. `RTL_DBG=1` enables standardized `[RTL]` debug printouts at meaningful core events such as configuration changes, RX/TX character boundaries, FIFO clear/transfer/backpressure, RX overrun, and RTS/CTS state changes. These prints must be wrapped with the standard `DPRINT` macro, removed under `SYNTHESIS`, and use raw simulator ticks in the `[%0t]` field with `$time`. `uart_core.sv` must propagate `RTL_DBG` to all runtime helper modules.

---

## 4. Top-Level Signal Semantics

### 4.1 Configuration Signals

`cfg_*` signals are live configuration inputs. `uart_core` samples/uses them directly.

| Signal | Meaning |
| ------ | ------- |
| `cfg_enable` | Global core enable. When low, RX/TX engines should not start new work. |
| `cfg_rx_enable` | Allows RX decoder to detect and receive new characters. |
| `cfg_tx_enable` | Allows TX path to launch new characters. |
| `cfg_baud_inc` | Fractional baud increment used by `uart_baud_gen` to generate average oversample timing. |
| `cfg_parity_mode` | Parity selection from `uart_pkg.sv`. |
| `cfg_stop_bits` | Stop-bit selection from `uart_pkg.sv`. |
| `cfg_data_bits` | Data-bit selection from `uart_pkg.sv`. The external byte stream remains 8-bit. |
| `cfg_hw_flow_enable` | Runtime enable for RTS/CTS behavior when `HAS_RTS_CTS=1`. |

`cfg_enable`, `cfg_rx_enable`, and `cfg_tx_enable` should prevent new RX/TX work. Avoid abruptly corrupting a character already in progress unless the design guide explicitly says reset/clear is occurring.

For implementation simplicity, configuration should be treated as stable while RX/TX traffic is active. Wrappers should program configuration while the core is idle or disabled.

### 4.2 TX Byte Stream

The TX byte stream is the reusable interface used by Type A and Type B upper layers to send bytes through the UART transmitter.

```systemverilog
input  wire       tx_byte_valid;
output wire       tx_byte_ready;
input  wire [7:0] tx_byte_data;
```

`tx_byte_valid && tx_byte_ready` means one byte is accepted into the TX FIFO.

`tx_byte_ready` should normally be high when the TX FIFO is not full and the TX FIFO clear pulse is not active. It should not depend on CTS. CTS blocks character launch from the FIFO into the TX serializer; it should not prevent the upper layer from filling available TX FIFO space.

If `cfg_data_bits` selects fewer than 8 data bits, the TX serializer sends only the configured number of LSB data bits. Upper bits are ignored by the serializer.

### 4.3 RX Byte Stream and Per-Byte Status

The RX byte stream is the reusable interface used by upper layers to consume received UART bytes.

```systemverilog
output wire       rx_byte_valid;
input  wire       rx_byte_ready;
output wire [7:0] rx_byte_data;

output wire       rx_byte_frame_error;
output wire       rx_byte_parity_error;
output wire       rx_byte_break_detect;
```

`rx_byte_valid && rx_byte_ready` pops one RX FIFO record.

The RX FIFO record is not just the byte. It must store byte metadata too:

```text
{break_detect, parity_error, frame_error, data[7:0]}
```

The `rx_byte_*` status fields describe the exact byte currently presented on `rx_byte_data` while `rx_byte_valid=1`.

Example:

```text
UART receives 8'h41 with parity error.
Later, when that record reaches the RX output:
  rx_byte_valid        = 1
  rx_byte_data         = 8'h41
  rx_byte_parity_error = 1
```

These fields are data-path metadata. They are not one-cycle event pulses.

If `cfg_data_bits` selects fewer than 8 data bits, the RX data output should zero-fill unused upper bits.

### 4.4 Event Pulses

Event outputs are immediate one-cycle pulses generated when the core detects something.

```systemverilog
output wire event_rx_overrun;
output wire event_rx_frame_error;
output wire event_rx_parity_error;
output wire event_rx_break_detect;
output wire event_tx_done;
```

These signals are not FIFO-output metadata.

Difference between per-byte status and events:

```text
rx_byte_parity_error:
  The byte currently presented at rx_byte_data had a parity error.

event_rx_parity_error:
  The RX decoder detected a parity error now.
```

`event_rx_frame_error`, `event_rx_parity_error`, and `event_rx_break_detect` should pulse at detection time, usually when the RX decoder completes the affected character.

`event_rx_overrun` pulses when the RX decoder has a completed character record but the RX FIFO cannot accept it because it is full. There is no `rx_byte_overrun` field because the dropped byte has no reliable FIFO record to attach metadata to.

`event_tx_done` pulses when one UART character has fully completed serialization, including the configured stop bit period. It does not mean the TX FIFO is empty. A full TX idle condition is:

```systemverilog
tx_fifo_empty && !tx_busy
```

### 4.5 FIFO Status

FIFO status outputs report the current visible FIFO state.

| Signal | Meaning |
| ------ | ------- |
| `rx_fifo_level` | Number of RX records available to the upper layer. |
| `tx_fifo_level` | Number of TX bytes queued in the TX FIFO. |
| `rx_fifo_empty` | RX FIFO has no records. |
| `rx_fifo_full` | RX FIFO cannot accept another record. |
| `tx_fifo_empty` | TX FIFO has no queued bytes. |
| `tx_fifo_full` | TX FIFO cannot accept another byte from the upper layer. |

`RX_FIFO_DEPTH` and `TX_FIFO_DEPTH` must be at least 2. For RTS/CTS usage, `RX_FIFO_DEPTH` should be at least 4 so the RTS hysteresis thresholds are meaningful.

### 4.6 RTS/CTS Status

RTS/CTS behavior is optional and controlled by `HAS_RTS_CTS` plus `cfg_hw_flow_enable`.

`HAS_RTS_CTS` is a synthesis-time feature parameter. `cfg_hw_flow_enable` is a runtime enable.

When `HAS_RTS_CTS=0`:

```text
CTS is ignored.
TX is never blocked by CTS.
RTS is driven inactive.
cts_active  = 1
rts_active  = 0
cts_blocked = 0
```

When `HAS_RTS_CTS=1` and `cfg_hw_flow_enable=1`:

```text
CTS gates launch of the next UART character.
RTS is generated from RX FIFO level.
```

CTS must not pause a character already being serialized. It only blocks launching the next character.

`cts_active` means the peer currently permits this core to transmit after polarity normalization.

`rts_active` means this core currently permits the peer to transmit after polarity normalization.

`cts_blocked` means TX has a byte ready to launch, but CTS is currently preventing that launch.

Better definition:

```systemverilog
cts_blocked = cfg_hw_flow_enable
           && HAS_RTS_CTS
           && tx_launch_wanted
           && !cts_active;
```

If CTS is inactive but the TX FIFO is empty, `cts_blocked` must remain low because no pending byte is actually blocked.

---

## 5. Design-Wide Architecture

The core is built from these pieces:

```text
uart_rx_i
  -> uart_rx_sync
  -> uart_rx
  -> RX FIFO: {break, parity_error, frame_error, data[7:0]}
  -> rx_byte_* output interface

upper tx_byte_* input interface
  -> TX FIFO: {data[7:0]}
  -> uart_tx
  -> uart_tx_o

uart_baud_gen
  -> oversample_tick for RX timing
  -> baud_tick or bit timing for TX timing

uart_hw_flow_ctrl
  -> synchronizes CTS
  -> generates RTS
  -> gates TX character launch
```

The top-level core should be clean glue. Keep detailed RX/TX behavior inside `uart_rx.sv` and `uart_tx.sv`. Keep RTS/CTS policy inside `uart_hw_flow_ctrl.sv`. Keep FIFO behavior inside `uart_byte_fifo.sv`.

---

## 6. `uart_pkg.sv`

`uart_pkg.sv` should define shared UART constants and encodings. It should not implement logic.

Recommended contents:

```systemverilog
package uart_pkg;

  localparam logic [1:0] UART_PARITY_NONE = 2'd0;
  localparam logic [1:0] UART_PARITY_EVEN = 2'd1;
  localparam logic [1:0] UART_PARITY_ODD  = 2'd2;
  localparam logic [1:0] UART_PARITY_RSVD = 2'd3;

  localparam logic [1:0] UART_STOP_1      = 2'd0;
  localparam logic [1:0] UART_STOP_2      = 2'd1;
  localparam logic [1:0] UART_STOP_RSVD0  = 2'd2;
  localparam logic [1:0] UART_STOP_RSVD1  = 2'd3;

  localparam logic [1:0] UART_DATA_5      = 2'd0;
  localparam logic [1:0] UART_DATA_6      = 2'd1;
  localparam logic [1:0] UART_DATA_7      = 2'd2;
  localparam logic [1:0] UART_DATA_8      = 2'd3;

endpackage
```

Do not put AXI, AXI-Lite, AXI-Stream, command protocol, bridge protocol, CRC, opcode, or register-map definitions here.

Wrappers may reuse these constants for reset defaults, but `uart_core` itself does not own those defaults.

---

## 7. `uart_rx_sync.sv`

Purpose: synchronize the asynchronous external `uart_rx_i` signal into `clk` before the RX decoder uses it.

Inputs/outputs should be simple:

```text
clk
rst_n
uart_rx_i
rx_sync_o
```

Implementation requirements:

- Use synchronous reset.
- Use a small multi-stage synchronizer, normally 3 stages.
- Idle UART RX is high, so reset the synchronizer chain to `1'b1`.
- Do not decode UART frames here.
- Do not generate baud timing here.
- Do not generate RX error flags here.

Optional future glitch filter may live here, but do not add it unless explicitly required. The first implementation should be a clean synchronizer.

---

## 8. `uart_baud_gen.sv`

Purpose: generate timing pulses used by RX and TX logic.

`cfg_baud_inc` defines the fractional baud increment used by an NCO / phase accumulator.

For a target baud rate:

```text
cfg_baud_inc = round(baud_rate * OVERSAMPLE * 2^BAUD_ACC_W / clk_hz)
```

The actual average oversample tick rate is:

```text
baud_os_tick_rate = clk_hz * cfg_baud_inc / 2^BAUD_ACC_W
```

The actual average UART baud rate is:

```text
baud_actual = baud_os_tick_rate / OVERSAMPLE
```

`uart_baud_gen` should provide at least:

```text
oversample_tick
baud_tick
```

Where:

```text
oversample_tick:
  one-cycle pulse from the NCO carry-out at average rate baud * OVERSAMPLE.

baud_tick:
  one-cycle pulse generated every OVERSAMPLE accepted oversample ticks for TX bit timing.
```

Implementation requirements:

- Use synchronous reset.
- When `cfg_enable=0`, the phase accumulator, oversample counter, and pulses should return to a known idle state.
- `oversample_tick` and `baud_tick` are one-cycle pulses.
- Implement an unsigned phase accumulator; the carry-out produces `oversample_tick`.
- `cfg_baud_inc=0` disables ticks. It must not be treated as maximum rate.
- `cfg_baud_inc` all-ones produces an oversample tick on almost every clock. Exact tick-every-clock operation is not required for normal UART operation.
- Do not contain UART RX/TX FSMs.
- Do not contain config registers.

RX may need to align its own sample phase after detecting a start edge. This can be done inside `uart_rx.sv` using `oversample_tick` and internal counters. Do not force RX start-bit phase alignment into `uart_baud_gen`.

---

## 9. `uart_rx.sv`

Purpose: decode the synchronized serial RX signal into byte records and error status.

The RX decoder consumes:

```text
clk
rst_n
cfg_enable
cfg_rx_enable
cfg_parity_mode
cfg_stop_bits
cfg_data_bits
oversample_tick
rx_sync_i
```

The RX decoder produces:

```text
rx_char_valid
rx_char_data[7:0]
rx_char_frame_error
rx_char_parity_error
rx_char_break_detect
rx_busy
event_rx_frame_error
event_rx_parity_error
event_rx_break_detect
```

`rx_char_valid` is the completed character record pulse from the RX decoder into the RX FIFO push path.

### RX FSM

Use a single `always_ff` process for the RX FSM. Do not split FSM state transition into separate `always_comb` next-state logic.

Recommended states:

```text
S_IDLE
S_START
S_DATA
S_PARITY
S_STOP
S_DONE
```

Include a default state branch:

```systemverilog
default: begin
  s_current <= S_IDLE;
end
```

### RX start detection

In `S_IDLE`, wait for `rx_sync_i` to go low while `cfg_enable && cfg_rx_enable` are both high.

After detecting the falling edge/start condition, wait half a bit period using oversample counting. With `OVERSAMPLE=16`, the middle of the start bit is approximately 8 oversample ticks after detection.

If the mid-start sample is not low, treat it as a false start and return to `S_IDLE` without producing a byte.

### RX data sampling

After a valid start bit, sample each data bit near the bit center every `OVERSAMPLE` ticks.

Data bit count comes from `cfg_data_bits`:

| `cfg_data_bits` | Data bits |
| --------------- | --------- |
| `UART_DATA_5` | 5 |
| `UART_DATA_6` | 6 |
| `UART_DATA_7` | 7 |
| `UART_DATA_8` | 8 |

Store received data LSB-first. Zero-fill unused upper bits in the final 8-bit output.

### RX parity

If `cfg_parity_mode=UART_PARITY_NONE`, skip parity sampling.

If parity is enabled, sample one parity bit after the data bits.

Parity check:

```text
EVEN parity: data bits plus parity bit should have even number of 1s.
ODD parity:  data bits plus parity bit should have odd number of 1s.
```

If parity fails:

```text
rx_char_parity_error = 1 for this completed character record
event_rx_parity_error pulses once at detection/completion time
```

Reserved parity mode should behave safely. Recommended behavior: treat reserved mode as no parity.

### RX stop bits and frame error

Stop bit samples must be high.

For one stop bit, check one stop-bit sample.

For two stop bits, wait/check the second stop-bit interval too. A frame error should be reported if any required stop-bit sample is low.

If frame error is detected:

```text
rx_char_frame_error = 1 for this completed character record
event_rx_frame_error pulses once at detection/completion time
```

Reserved stop-bit modes should behave safely. Recommended behavior: treat reserved modes as one stop bit.

### RX break detect

A break condition means RX stayed low for a full character-like interval instead of returning high for stop.

Recommended first implementation:

```text
break_detect = frame_error && all sampled data bits are 0 && stop sample is 0
```

If break is detected:

```text
rx_char_break_detect = 1 for this completed character record
event_rx_break_detect pulses once at detection/completion time
```

The byte data for a break record may be `8'h00`.

### RX FIFO push interaction

`uart_rx.sv` should not know if the RX FIFO is full. It should simply emit a completed character record pulse.

`uart_core.sv` glue logic decides whether that record is pushed into RX FIFO or dropped with `event_rx_overrun`.

---

## 10. `uart_tx.sv`

Purpose: serialize one byte into UART TX format.

The TX serializer consumes:

```text
clk
rst_n
cfg_enable
cfg_tx_enable
cfg_parity_mode
cfg_stop_bits
cfg_data_bits
baud_tick
tx_start
tx_data[7:0]
```

The TX serializer produces:

```text
tx_ready_for_start
tx_busy
uart_tx_o
event_tx_done
```

`tx_start` should be asserted by `uart_core.sv` only when:

```text
TX FIFO is not empty
TX serializer is ready
cfg_enable && cfg_tx_enable
CTS/flow-control allows launch
```

CTS must not be handled inside `uart_tx.sv`. CTS belongs in `uart_hw_flow_ctrl.sv` and top glue launch logic.

### TX FSM

Use a single `always_ff` process for the TX FSM. Do not split FSM state transition into separate `always_comb` next-state logic.

Recommended states:

```text
S_IDLE
S_START
S_DATA
S_PARITY
S_STOP
```

Include a default state branch:

```systemverilog
default: begin
  s_current <= S_IDLE;
end
```

### TX serialization

UART TX idle level is high.

Frame format:

```text
start bit: 0
data bits: LSB-first, count from cfg_data_bits
parity: optional
stop bits: high, count from cfg_stop_bits
```

If `cfg_data_bits` selects fewer than 8 bits, serialize only the configured number of LSBs.

Reserved parity mode should behave safely. Recommended behavior: no parity.

Reserved stop-bit modes should behave safely. Recommended behavior: one stop bit.

`event_tx_done` pulses when the final configured stop bit period has completed.

---

## 11. `uart_byte_fifo.sv`

Purpose: small synchronous FIFO used for both TX bytes and RX byte records.

Make it generic enough for both uses:

```text
DATA_W
DEPTH
```

TX FIFO configuration:

```text
DATA_W = 8
DEPTH  = TX_FIFO_DEPTH
```

RX FIFO configuration:

```text
DATA_W = 11
DEPTH  = RX_FIFO_DEPTH
payload = {break_detect, parity_error, frame_error, data[7:0]}
```

FIFO interface should provide:

```text
push_valid
push_ready
push_data
pop_valid
pop_ready
pop_data
level
empty
full
clear
```

Implementation requirements:

- Single clock only.
- Synchronous reset.
- Synchronous clear input.
- Support push and pop in the same cycle.
- Level output must remain correct under simultaneous push/pop.
- `push_ready` should be high when the FIFO can accept data.
- `pop_valid` should be high when the FIFO has data.
- Do not implement UART-specific logic inside the FIFO.

Recommended depth expectations:

```text
DEPTH >= 2
```

If pointer widths need care for small depths, use clean localparams and avoid ugly width-cast hacks.

---

## 12. `uart_hw_flow_ctrl.sv`

Purpose: isolate all optional RTS/CTS behavior from the rest of the core.

This module handles:

```text
CTS synchronization
CTS polarity normalization
RTS generation from RX FIFO level
RTS polarity normalization
TX launch permission
cts_active / rts_active / cts_blocked status
```

Do not use `ifdef` to remove RTS/CTS logic or ports. `HAS_RTS_CTS` is used by `uart_core.sv` generate logic.

### CTS synchronization

`uart_cts_i` is asynchronous to `clk`.

Inside `uart_hw_flow_ctrl.sv`, use a fixed conservative synchronizer depth:

```systemverilog
localparam int unsigned CTS_SYNC_STAGES = 3;
```

Do not expose `CTS_SYNC_STAGES` as a public `uart_core` generic.

Reset CTS synchronizer to inactive-safe or transmit-safe depending on selected policy. Recommended for practical operation: make normalized `cts_active` reset to `1'b1` so TX is not permanently blocked before the external pin has settled. If a stricter safety policy is desired later, document it before changing.

### CTS behavior

Normalize CTS polarity:

```text
CTS_ACTIVE_LOW=1: uart_cts_i low means peer permits transmit.
CTS_ACTIVE_LOW=0: uart_cts_i high means peer permits transmit.
```

CTS gates only the launch of a new character. It must never stop a character already in progress.

`tx_start_allowed` should be true when:

```text
!cfg_hw_flow_enable || cts_active
```

when the feature exists. If hardware flow is disabled at runtime, CTS is ignored.

### RTS behavior

RTS tells the peer whether this core can accept more incoming RX data.

Use RX FIFO level with hysteresis:

```text
If rts_active=1 and rx_fifo_level >= RTS_DEASSERT_LEVEL:
  deassert RTS.

If rts_active=0 and rx_fifo_level <= RTS_ASSERT_LEVEL:
  assert RTS.
```

Normalize RTS polarity:

```text
RTS_ACTIVE_LOW=1: active RTS drives uart_rts_o low.
RTS_ACTIVE_LOW=0: active RTS drives uart_rts_o high.
```

When `cfg_hw_flow_enable=0`, drive RTS active or inactive according to selected policy. Recommended behavior: drive RTS active if the core is enabled and RX is enabled, because this means the core is not asking the peer to stop due to hardware flow control. If upper wrappers want RTS inactive when flow control is disabled, they can set `HAS_RTS_CTS=0` or ignore the pin. Keep the behavior documented and deterministic.

When `HAS_RTS_CTS=0`, top glue must tie:

```text
cts_active  = 1
rts_active  = 0
cts_blocked = 0
uart_rts_o  = inactive polarity
```

### cts_blocked

`cts_blocked` is diagnostic/status information.

It means:

```text
TX has a byte ready to launch, but CTS is currently preventing launch.
```

It is not simply `!cts_active`.

---

## 13. `uart_core.sv` Glue Logic

`uart_core.sv` instantiates and wires all submodules.

Recommended internal localparams:

```systemverilog
localparam int unsigned RX_LEVEL_W     = $clog2(RX_FIFO_DEPTH + 1);
localparam int unsigned TX_LEVEL_W     = $clog2(TX_FIFO_DEPTH + 1);
localparam int unsigned RX_FIFO_DATA_W = 11;
localparam int unsigned TX_FIFO_DATA_W = 8;
```

Do not expose these as public generics.

### Main wiring

Required submodule flow:

```text
uart_rx_i
  -> uart_rx_sync
  -> uart_rx
  -> RX FIFO
  -> rx_byte_* outputs
```

```text
tx_byte_* inputs
  -> TX FIFO
  -> uart_tx
  -> uart_tx_o
```

```text
uart_baud_gen
  -> oversample_tick to uart_rx
  -> baud_tick to uart_tx
```

```text
uart_hw_flow_ctrl or no-flow generate branch
  -> tx_start_allowed
  -> uart_rts_o
  -> cts_active / rts_active / cts_blocked
```

### RX FIFO push and overrun

`uart_rx.sv` emits a completed character record pulse.

In top glue:

```text
If rx_char_valid && rx_fifo_push_ready:
  push {break, parity_error, frame_error, data} into RX FIFO.

If rx_char_valid && !rx_fifo_push_ready:
  drop the character and pulse event_rx_overrun.
```

Frame/parity/break event pulses come from the RX decoder at detection time. Overrun event comes from top glue because only top glue sees FIFO availability.

### TX FIFO pop and TX launch

Top glue should launch TX only when all are true:

```text
cfg_enable
cfg_tx_enable
!tx_fifo_empty
uart_tx ready for new character
tx_start_allowed from flow-control logic
```

When launch occurs:

```text
pop one byte from TX FIFO
present it to uart_tx
generate tx_start for uart_tx
```

Do not let CTS affect `tx_byte_ready`. CTS blocks launch from FIFO to serializer, not acceptance into available FIFO space.

### RTS/CTS generate structure

Use parameter/generate. Do not use `ifdef`.

```systemverilog
generate
  if (HAS_RTS_CTS) begin : g_hw_flow_ctrl
    // instantiate uart_hw_flow_ctrl
  end else begin : g_no_hw_flow_ctrl
    // tie off flow-control behavior
  end
endgenerate
```

When disabled:

```text
CTS ignored
TX launch allowed
RTS driven inactive
cts_active=1
rts_active=0
cts_blocked=0
```

---

## 14. Reset, Clear, and Enable Behavior

All sequential logic must use synchronous reset.

General reset behavior:

```text
uart_tx_o = 1, UART idle high
uart_rts_o = inactive if HAS_RTS_CTS=0
RX/TX FIFOs empty
rx_busy = 0
tx_busy = 0
event_* = 0
```

`ctrl_rx_fifo_clear`:

```text
clears queued RX FIFO records
must not reset the RX serializer/decoder FSM unless explicitly designed that way
must not generate fake RX events
```

`ctrl_tx_fifo_clear`:

```text
clears queued TX FIFO bytes
must not corrupt a character already being serialized
```

`cfg_enable=0`:

```text
prevents new RX/TX work
baud pulses may stop
TX output should remain idle high when transmitter is idle
```

`cfg_rx_enable=0`:

```text
RX should not start decoding new characters
existing RX FIFO records remain available unless ctrl_rx_fifo_clear is asserted
```

`cfg_tx_enable=0`:

```text
TX should not launch new characters
existing TX FIFO bytes may remain queued unless ctrl_tx_fifo_clear is asserted
```

Configuration changes should be done while the core is idle or disabled. The core does not need to guarantee clean behavior if parity/data/stop/baud configuration changes mid-character.

---

## 15. Implementation Rules for Codex

Follow these rules while implementing the RTL:

1. Use SystemVerilog 2012.
2. Use `input wire` / `output wire` style in module port declarations.
3. Use synchronous resets for `always_ff` logic.
4. FSMs must be coded as single `always_ff` processes.
5. Do not use two-process FSM style with separate `always_comb` next-state logic.
6. Every FSM must include a default state branch assigning `s_current <= S_IDLE;`.
7. Do not add SVA assertions.
8. Do not use `ifdef` to change the `uart_core` interface.
9. Use `HAS_RTS_CTS` with generate logic so synthesis prunes disabled flow-control logic.
10. Do not add AXI, AXI-Lite, AXI-Stream, bridge protocol, opcode, frame, or CRC logic inside this core.
11. Do not add config registers inside `uart_core`.
12. Keep RX per-byte metadata separate from immediate event pulses.
13. Keep CTS gating on TX character launch only, never mid-character.
14. Derive internal widths with localparams, not public generics.
15. Keep code readable; avoid ugly width-cast/slice hacks when clean localparams or typed intermediates solve the issue.

---

## 16. Bring-Up and Verification Checklist

Codex should implement the design so these behaviors can be verified later.

### Compile-level checks

- All listed files compile together.
- `uart_core.sv` instantiates every required helper file.
- No missing package imports.
- No unused public ports due to broken generate logic.
- No accidental AXI/AXIL/AXIS dependencies.

### RX checks

- Idle RX line produces no bytes.
- Valid 8N1 byte produces `rx_byte_valid` with correct data.
- 5/6/7/8 data-bit modes zero-fill unused upper bits.
- Even parity and odd parity modes detect parity errors.
- Stop-bit violation sets `rx_byte_frame_error` and pulses `event_rx_frame_error`.
- Break-like condition sets `rx_byte_break_detect` and pulses `event_rx_break_detect`.
- RX FIFO full causes `event_rx_overrun` and drops the completed character.

### TX checks

- TX idle output is high.
- Valid TX byte serializes start, data, optional parity, and stop bits.
- 5/6/7/8 data-bit modes serialize the correct number of LSBs.
- `event_tx_done` pulses once per completed UART character.
- `tx_fifo_empty && !tx_busy` represents full TX idle.

### FIFO checks

- TX accepts bytes when FIFO space exists.
- RX presents records with data and metadata aligned.
- Simultaneous FIFO push/pop preserves correct level.
- FIFO clear removes queued entries cleanly.

### RTS/CTS checks

- With `HAS_RTS_CTS=0`, CTS does not block TX and RTS is inactive.
- With `HAS_RTS_CTS=1` and `cfg_hw_flow_enable=0`, CTS does not block TX.
- With `HAS_RTS_CTS=1` and `cfg_hw_flow_enable=1`, inactive CTS blocks only new character launch.
- `cts_blocked` asserts only when a byte is ready to launch and CTS blocks it.
- RTS deasserts at `RTS_DEASSERT_LEVEL` and reasserts at `RTS_ASSERT_LEVEL`.
