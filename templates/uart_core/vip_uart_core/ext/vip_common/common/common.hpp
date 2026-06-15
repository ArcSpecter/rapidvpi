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
