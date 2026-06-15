// vip_common/agents/por/por.cpp
#include "vip_common/agents/por/por.hpp"

namespace vip::common {

Por::Por(TestBase& tb, std::string rst_net, const bool active_low)
    : tb_(tb)
    , utils_(tb)
    , log_()
    , rst_net_(std::move(rst_net))
    , active_low_(active_low) {
    assert_val_ = active_low_ ? 0 : 1;
    deassert_val_ = active_low_ ? 1 : 0;
}

Por::RunUserTask Por::assert_reset(double hold_ns) {
    {
        auto w = tb_.getCoWrite(0);
        w.write(rst_net_, assert_val_);
        co_await w;
    }

    if (hold_ns > 0.0) {
        co_await utils_.delay_ns(hold_ns);
    }

    co_return;
}

Por::RunUserTask Por::deassert_reset(double settle_ns) {
    {
        auto w = tb_.getCoWrite(0);
        w.write(rst_net_, deassert_val_);
        co_await w;
    }

    if (settle_ns > 0.0) {
        co_await utils_.delay_ns(settle_ns);
    }

    co_return;
}

Por::RunUserTask Por::pulse_reset(double hold_ns, double settle_ns) {
    // Assert
    {
        auto w = tb_.getCoWrite(0);
        w.write(rst_net_, assert_val_);
        co_await w;
    }

    if (hold_ns > 0.0) {
        co_await utils_.delay_ns(hold_ns);
    }

    // Deassert
    {
        auto w = tb_.getCoWrite(0);
        w.write(rst_net_, deassert_val_);
        co_await w;
    }

    if (settle_ns > 0.0) {
        co_await utils_.delay_ns(settle_ns);
    }

    co_return;
}

} // namespace vip::common
