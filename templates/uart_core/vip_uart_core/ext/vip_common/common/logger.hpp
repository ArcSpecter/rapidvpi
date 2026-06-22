/*
 * MIT License
 *
 * Copyright (c) 2026 Rovshan Rustamov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// vip_common/common/logger.hpp
#ifndef VIP_COMMON_SIM_LOGGER_HPP
#define VIP_COMMON_SIM_LOGGER_HPP

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>

#include <rapidvpi/testbase/testbase.hpp>
#include <vpi_user.h>

namespace vip::common {

using TestBase = ::test::TestBase;
using sim_tick_t = ::test::sim_tick_t;

inline constexpr sim_tick_t INVALID_TICK = std::numeric_limits<sim_tick_t>::max();

// Minimal VPI-backed stream logger.
//
// Usage:
//   vip::common::SimLogger log;
//   log << "hello" << std::endl;
//
// This class is intentionally stateless; operators are const so it can be used
// from const contexts (agents typically log from const helpers).
class SimLogger {
public:
    template <typename T>
    SimLogger& operator<<(const T& value) const {
        std::ostringstream ss;
        ss << value;
        const std::string s = ss.str();
        vpi_printf(const_cast<PLI_BYTE8*>(reinterpret_cast<const PLI_BYTE8*>("%s")),
                   const_cast<PLI_BYTE8*>(reinterpret_cast<const PLI_BYTE8*>(s.c_str())));
        return const_cast<SimLogger&>(*this);
    }

    SimLogger& operator<<(std::ostream& (*manip)(std::ostream&)) const {
        (void)manip;
        vpi_printf(const_cast<PLI_BYTE8*>(reinterpret_cast<const PLI_BYTE8*>("%s")),
                   const_cast<PLI_BYTE8*>(reinterpret_cast<const PLI_BYTE8*>("\n")));
        return const_cast<SimLogger&>(*this);
    }
};

[[nodiscard]] inline sim_tick_t sim_time_ticks() noexcept {
    s_vpi_time t{};
    t.type = vpiSimTime;
    vpi_get_time(nullptr, &t);

    const auto high = static_cast<std::uint32_t>(t.high);
    const auto low = static_cast<std::uint32_t>(t.low);
    return (static_cast<sim_tick_t>(high) << 32u) | static_cast<sim_tick_t>(low);
}

[[nodiscard]] inline bool valid_tick(const sim_tick_t tick) noexcept {
    return tick != INVALID_TICK;
}

[[nodiscard]] inline long double ticks_to_seconds(const TestBase& tb,
                                                  const sim_tick_t ticks) noexcept {
    return static_cast<long double>(ticks) * tb.vpiTickPeriodSeconds();
}

[[nodiscard]] inline long double ticks_to_ns_ld(const TestBase& tb,
                                                const sim_tick_t ticks) noexcept {
    return ticks_to_seconds(tb, ticks) / 1.0e-9L;
}

[[nodiscard]] inline double ticks_to_ns(const TestBase& tb,
                                        const sim_tick_t ticks) noexcept {
    return static_cast<double>(ticks_to_ns_ld(tb, ticks));
}

[[nodiscard]] inline sim_tick_t delta_ticks(const sim_tick_t start,
                                            const sim_tick_t end) noexcept {
    if (!valid_tick(start) || !valid_tick(end) || end < start) {
        return 0u;
    }
    return end - start;
}

[[nodiscard]] inline double delta_ticks_to_ns(const TestBase& tb,
                                              const sim_tick_t start,
                                              const sim_tick_t end) noexcept {
    return ticks_to_ns(tb, delta_ticks(start, end));
}

[[nodiscard]] inline std::string format_ns(const double ns_value,
                                           const unsigned precision = 3u) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(static_cast<int>(precision)) << ns_value;
    return oss.str();
}

[[nodiscard]] inline std::string tick_unit_label(const TestBase& tb) {
    switch (tb.vpiTimePrecisionExp10()) {
        case -15: return "1fs";
        case -12: return "1ps";
        case -9: return "1ns";
        case -6: return "1us";
        case -3: return "1ms";
        case 0: return "1s";
        default: return "1e" + std::to_string(tb.vpiTimePrecisionExp10()) + "s";
    }
}

template <test::TimeUnit U>
[[nodiscard]] inline sim_tick_t duration_to_ticks(const TestBase& tb,
                                                  const test::delay_arg_t<U> delay) {
    if constexpr (U == test::TimeUnit::ticks) {
        return static_cast<sim_tick_t>(delay);
    } else {
        if (delay <= 0.0) {
            return 0u;
        }

        const long double raw_ticks =
            static_cast<long double>(delay) * test::TimeUnitSeconds<U>::value /
            tb.vpiTickPeriodSeconds();

        constexpr long double rel_eps = 1.0e-12L;
        const long double nearest = std::floor(raw_ticks + 0.5L);
        const long double diff = raw_ticks > nearest ? raw_ticks - nearest : nearest - raw_ticks;
        const long double scale = raw_ticks > 1.0L ? raw_ticks : 1.0L;
        const long double rounded = (diff <= rel_eps * scale) ? nearest : std::ceil(raw_ticks);

        return static_cast<sim_tick_t>(rounded);
    }
}

inline void log_line(const std::string& src, const std::string& level, const std::string& msg) {
    SimLogger log;
    log << "# [" << src << "][" << level << "][tick=" << sim_time_ticks()
        << "] " << msg << std::endl;
}

} // namespace vip::common

#endif // VIP_COMMON_SIM_LOGGER_HPP
