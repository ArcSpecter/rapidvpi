// MIT License

// Copyright (c) 2026 Rovshan Rustamov

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
