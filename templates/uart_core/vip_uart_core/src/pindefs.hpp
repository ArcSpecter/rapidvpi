#ifndef VIP_UART_CORE_PINDEFS_HPP
#define VIP_UART_CORE_PINDEFS_HPP

#include <cstdint>
#include <string>

#include "vip_uart/common/uart_params.hpp"
#include "vip_uart/common/uart_types.hpp"

namespace test {

static constexpr double CLK_PERIOD_NS = 10.0;
static constexpr std::uint64_t CLK_HZ = 100'000'000ull;

static constexpr unsigned BAUD_ACC_W = 32u;
static constexpr unsigned BYTE_W = 8u;
static constexpr unsigned CFG_MODE_W = 2u;
static constexpr unsigned RX_FIFO_DEPTH = 16u;
static constexpr unsigned TX_FIFO_DEPTH = 16u;
static constexpr unsigned FIFO_LEVEL_W = 5u;
static constexpr unsigned OVERSAMPLE = 16u;

static constexpr std::uint32_t BASIC_BAUD_RATE = 921'600u;
static constexpr std::uint32_t SLOW_BAUD_RATE = 460'800u;
static constexpr std::uint32_t BAUD_INC_DISABLED = 0u;
static constexpr std::uint64_t BAUD_INC_SCALE = std::uint64_t{1} << BAUD_ACC_W;
static constexpr std::uint64_t BASIC_BAUD_INC_NUM =
    static_cast<std::uint64_t>(BASIC_BAUD_RATE) * OVERSAMPLE * BAUD_INC_SCALE;
static constexpr std::uint32_t BASIC_BAUD_INC =
    static_cast<std::uint32_t>((BASIC_BAUD_INC_NUM + (CLK_HZ / 2u)) / CLK_HZ);
static constexpr unsigned BASIC_UART_BIT_CLKS =
    static_cast<unsigned>((CLK_HZ + (BASIC_BAUD_RATE / 2u)) / BASIC_BAUD_RATE);
static constexpr unsigned BASIC_UART_SAMPLE_CLK_INDEX = BASIC_UART_BIT_CLKS / 2u;
static constexpr unsigned HANDSHAKE_TIMEOUT_CYCLES = 1024u;
static constexpr unsigned TX_IDLE_TIMEOUT_CYCLES = BASIC_UART_BIT_CLKS * 16u;

static constexpr unsigned UART_PARITY_NONE = 0u;
static constexpr unsigned UART_PARITY_EVEN = 1u;
static constexpr unsigned UART_PARITY_ODD = 2u;
static constexpr unsigned UART_PARITY_RSVD = 3u;

static constexpr unsigned UART_STOP_1 = 0u;
static constexpr unsigned UART_STOP_2 = 1u;
static constexpr unsigned UART_STOP_RSVD0 = 2u;
static constexpr unsigned UART_STOP_RSVD1 = 3u;

static constexpr unsigned UART_DATA_5 = 0u;
static constexpr unsigned UART_DATA_6 = 1u;
static constexpr unsigned UART_DATA_7 = 2u;
static constexpr unsigned UART_DATA_8 = 3u;

inline std::string dut_name = "uart_core";

inline std::string clk = "clk";
inline std::string rst_n = "rst_n";

inline std::string uart_rx_i = "uart_rx_i";
inline std::string uart_tx_o = "uart_tx_o";
inline std::string uart_cts_i = "uart_cts_i";
inline std::string uart_rts_o = "uart_rts_o";

inline std::string cfg_enable = "cfg_enable";
inline std::string cfg_rx_enable = "cfg_rx_enable";
inline std::string cfg_tx_enable = "cfg_tx_enable";
inline std::string cfg_baud_inc = "cfg_baud_inc";
inline std::string cfg_parity_mode = "cfg_parity_mode";
inline std::string cfg_stop_bits = "cfg_stop_bits";
inline std::string cfg_data_bits = "cfg_data_bits";
inline std::string cfg_hw_flow_enable = "cfg_hw_flow_enable";

inline std::string ctrl_rx_fifo_clear = "ctrl_rx_fifo_clear";
inline std::string ctrl_tx_fifo_clear = "ctrl_tx_fifo_clear";

inline std::string tx_byte_valid = "tx_byte_valid";
inline std::string tx_byte_ready = "tx_byte_ready";
inline std::string tx_byte_data = "tx_byte_data";

inline std::string rx_byte_valid = "rx_byte_valid";
inline std::string rx_byte_ready = "rx_byte_ready";
inline std::string rx_byte_data = "rx_byte_data";
inline std::string rx_byte_frame_error = "rx_byte_frame_error";
inline std::string rx_byte_parity_error = "rx_byte_parity_error";
inline std::string rx_byte_break_detect = "rx_byte_break_detect";

inline std::string rx_fifo_level = "rx_fifo_level";
inline std::string tx_fifo_level = "tx_fifo_level";
inline std::string rx_fifo_empty = "rx_fifo_empty";
inline std::string rx_fifo_full = "rx_fifo_full";
inline std::string tx_fifo_empty = "tx_fifo_empty";
inline std::string tx_fifo_full = "tx_fifo_full";

inline std::string rx_busy = "rx_busy";
inline std::string tx_busy = "tx_busy";
inline std::string cts_active = "cts_active";
inline std::string rts_active = "rts_active";
inline std::string cts_blocked = "cts_blocked";

inline std::string event_rx_overrun = "event_rx_overrun";
inline std::string event_rx_frame_error = "event_rx_frame_error";
inline std::string event_rx_parity_error = "event_rx_parity_error";
inline std::string event_rx_break_detect = "event_rx_break_detect";
inline std::string event_tx_done = "event_tx_done";

inline std::string uart_rx_port_name = "uart_rx";
inline std::string uart_tx_port_name = "uart_tx";

[[nodiscard]] inline vip::uart::UartParams make_uart_params() {
    vip::uart::UartParams p{};
    p.data_bits = 8u;
    p.stop_bits = 1u;
    p.parity = vip::uart::UartParity::NONE;
    p.bit_clks = BASIC_UART_BIT_CLKS;
    p.sample_clk_index = BASIC_UART_SAMPLE_CLK_INDEX;
    p.idle_high = true;
    p.lsb_first = true;
    p.cts_active_low = true;
    p.rts_active_low = true;
    return p;
}

[[nodiscard]] inline vip::uart::UartTxPortConfig make_uart_rx_serial_port() {
    vip::uart::UartTxPortConfig p{};
    p.name = uart_rx_port_name;
    p.tx_net = uart_rx_i;
    p.rts_net = uart_rts_o;
    p.use_rts = false;
    p.rts_active_low = true;
    return p;
}

[[nodiscard]] inline vip::uart::UartRxPortConfig make_uart_tx_serial_port() {
    vip::uart::UartRxPortConfig p{};
    p.name = uart_tx_port_name;
    p.rx_net = uart_tx_o;
    p.cts_net = uart_cts_i;
    p.drive_cts = true;
    p.cts_active_low = true;
    return p;
}

} // namespace test

#endif // VIP_UART_CORE_PINDEFS_HPP
