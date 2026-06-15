#ifndef VIP_UART_COMMON_UART_TYPES_HPP
#define VIP_UART_COMMON_UART_TYPES_HPP

#include <cstdint>
#include <string>

#include "vip_uart/common/uart_params.hpp"

namespace vip::uart {

struct UartFrame {
    std::uint8_t data = 0;
    bool parity_error = false;
    bool framing_error = false;
    bool break_detect = false;
    double start_time_ns = -1.0;
    double end_time_ns = -1.0;

    unsigned data_bits = 8;
    unsigned stop_bits = 1;
    UartParity parity = UartParity::NONE;
};

struct UartTxPortConfig {
    // Logical name used by testcases and scoreboards.
    std::string name;

    // Physical net driven by this UART TX source.
    // In a DUT UART-core RX test this is normally the DUT uart_rx_i net.
    std::string tx_net;

    // Optional physical RTS net sampled before each frame launch.
    // In a DUT UART-core RX test this is normally the DUT uart_rts_o net.
    std::string rts_net;
    bool use_rts = false;
    bool rts_active_low = true;

    [[nodiscard]] static UartTxPortConfig from_base(const std::string& base) {
        UartTxPortConfig cfg;
        cfg.name = base;
        cfg.tx_net = base + "_tx";
        cfg.rts_net = base + "_rts";
        cfg.use_rts = false;
        return cfg;
    }
};

struct UartRxPortConfig {
    // Logical name used by testcases and scoreboards.
    std::string name;

    // Physical net sampled by this UART RX sink.
    // In a DUT UART-core TX test this is normally the DUT uart_tx_o net.
    std::string rx_net;

    // Optional physical CTS net driven to permit/block the peer transmitter.
    // In a DUT UART-core TX test this is normally the DUT uart_cts_i net.
    std::string cts_net;
    bool drive_cts = false;
    bool cts_active_low = true;

    [[nodiscard]] static UartRxPortConfig from_base(const std::string& base) {
        UartRxPortConfig cfg;
        cfg.name = base;
        cfg.rx_net = base + "_rx";
        cfg.cts_net = base + "_cts";
        cfg.drive_cts = false;
        return cfg;
    }
};

} // namespace vip::uart

#endif // VIP_UART_COMMON_UART_TYPES_HPP
