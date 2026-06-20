#include "vip_common/common/common.hpp"

namespace vip::common {

CommonUtils::CommonUtils(TestBase& tb, std::string default_clk_net)
    : tb_(tb), default_clk_net_(std::move(default_clk_net)) {}

CommonUtils::RunUserTask CommonUtils::waitFor(const std::string& net,
                                              const unsigned long long val) const {
    auto rd = tb_.getCoRead();
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

CommonUtils::RunUserTask CommonUtils::write_barrier() const {
    // Intentionally do not write any nets here; this is just a phase barrier.
    // Yield into the scheduler's write phase without relying on tiny time delays.
    co_await tb_.getCoWrite();
    co_return;
}

CommonUtils::RunUserTask CommonUtils::clock_to_write(const int n, const int edge) const {
    co_await clock(n, edge);
    co_await write_barrier();
    co_return;
}

} // namespace vip::common
