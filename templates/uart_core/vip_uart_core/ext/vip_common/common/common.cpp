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

#include "vip_common/common/common.hpp"

namespace vip::common {

CommonUtils::CommonUtils(TestBase& tb, std::string default_clk_net)
    : tb_(tb), default_clk_net_(std::move(default_clk_net)) {}

CommonUtils::RunUserTask CommonUtils::waitFor(const std::string& net,
                                              const unsigned long long val) const {
    auto rd = tb_.getCoRead(0);
    rd.read(net);
    co_await rd;

    if (rd.getNum(net) != val) {
        auto awchange = tb_.getCoChange(net, val);
        co_await awchange;
    }
    co_return;
}

CommonUtils::RunUserTask CommonUtils::clock(const int n, const int edge) const {
    for (int i = 0; i < n; ++i) {
        co_await tb_.getCoChange(default_clk_net_, edge);
    }
    co_return;
}

CommonUtils::RunUserTask CommonUtils::delay_ns(const double delay) const {
    co_await tb_.getCoWrite(delay);
    co_return;
}

CommonUtils::RunUserTask CommonUtils::write_barrier() const {
    // Intentionally do not write any nets here; this is just a phase barrier.
    // Yield into the scheduler's write phase without relying on tiny time delays.
    co_await tb_.getCoWrite(0);
    co_return;
}

CommonUtils::RunUserTask CommonUtils::clock_to_write(const int n, const int edge) const {
    co_await clock(n, edge);
    co_await write_barrier();
    co_return;
}

} // namespace vip::common
