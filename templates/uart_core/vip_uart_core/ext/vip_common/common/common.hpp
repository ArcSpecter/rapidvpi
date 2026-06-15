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

// vip_common/common/common.hpp
#ifndef VIP_COMMON_COMMON_HPP
#define VIP_COMMON_COMMON_HPP

#include <cstdint>
#include <string>

#include <rapidvpi/testbase/testbase.hpp>

namespace vip::common {

// RapidVPI's TestBase lives in `namespace test`.
// Keep vip_common neutral by aliasing it here.
using TestBase = ::test::TestBase;



// Common RapidVPI helpers.
//
// This is intentionally lightweight and does not assume anything about
// the user's project layout or pin-def headers.
class CommonUtils {
public:
    using RunUserTask = TestBase::RunUserTask;

    explicit CommonUtils(TestBase& tb, std::string default_clk_net = "clk");

    // Wait until net == val. If already equal, returns immediately.
    RunUserTask waitFor(const std::string& net, unsigned long long val) const;

    // Wait for `n` edges of the configured default clock.
    // edge=1 waits for clock==1 transitions, edge=0 waits for clock==0.
    RunUserTask clock(int n = 1, int edge = 1) const;

    // Delay (write-phase yield) by a number of nanoseconds.
    RunUserTask delay_ns(double delay) const;

    // Phase-safe helpers
    RunUserTask write_barrier() const;
    RunUserTask clock_to_write(int n = 1, int edge = 1) const;

    const std::string& default_clk_net() const { return default_clk_net_; }
    void set_default_clk_net(const std::string& net) { default_clk_net_ = net; }

private:
    TestBase& tb_;
    std::string default_clk_net_;
};

} // namespace vip::common

#endif // VIP_COMMON_COMMON_HPP
