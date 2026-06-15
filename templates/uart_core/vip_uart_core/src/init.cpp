#include "test.hpp"
#include "core.hpp"

#include <memory>

extern "C" void userRegisterFactory() {
    core::registerTestFactory([]() {
        return std::make_unique<test::Test>();
    });
}

namespace test {

void Test::initDutName() {
    setDutName(dutName);
}

void Test::initNets() {
    initDutName();

    addNet(clk, 1);
    addNet(rst_n, 1);

    addNet(uart_rx_i, 1);
    addNet(uart_tx_o, 1);
    addNet(uart_cts_i, 1);
    addNet(uart_rts_o, 1);

    addNet(cfg_enable, 1);
    addNet(cfg_rx_enable, 1);
    addNet(cfg_tx_enable, 1);
    addNet(cfg_baud_inc, BAUD_ACC_W);
    addNet(cfg_parity_mode, CFG_MODE_W);
    addNet(cfg_stop_bits, CFG_MODE_W);
    addNet(cfg_data_bits, CFG_MODE_W);
    addNet(cfg_hw_flow_enable, 1);

    addNet(ctrl_rx_fifo_clear, 1);
    addNet(ctrl_tx_fifo_clear, 1);

    addNet(tx_byte_valid, 1);
    addNet(tx_byte_ready, 1);
    addNet(tx_byte_data, BYTE_W);

    addNet(rx_byte_valid, 1);
    addNet(rx_byte_ready, 1);
    addNet(rx_byte_data, BYTE_W);
    addNet(rx_byte_frame_error, 1);
    addNet(rx_byte_parity_error, 1);
    addNet(rx_byte_break_detect, 1);

    addNet(rx_fifo_level, FIFO_LEVEL_W);
    addNet(tx_fifo_level, FIFO_LEVEL_W);
    addNet(rx_fifo_empty, 1);
    addNet(rx_fifo_full, 1);
    addNet(tx_fifo_empty, 1);
    addNet(tx_fifo_full, 1);

    addNet(rx_busy, 1);
    addNet(tx_busy, 1);
    addNet(cts_active, 1);
    addNet(rts_active, 1);
    addNet(cts_blocked, 1);

    addNet(event_rx_overrun, 1);
    addNet(event_rx_frame_error, 1);
    addNet(event_rx_parity_error, 1);
    addNet(event_rx_break_detect, 1);
    addNet(event_tx_done, 1);
}

} // namespace test
