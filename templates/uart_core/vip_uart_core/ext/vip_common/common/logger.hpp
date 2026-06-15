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

// vip_common/common/logger.hpp
#ifndef VIP_COMMON_SIM_LOGGER_HPP
#define VIP_COMMON_SIM_LOGGER_HPP

#include <cstdint>
#include <ostream>
#include <sstream>
#include <string>

#include <vpi_user.h>

namespace vip::common {

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

inline std::uint64_t sim_time_ns() {
    s_vpi_time t;
    t.type = vpiSimTime;
    t.high = 0;
    t.low = 0;
    vpi_get_time(nullptr, &t);
    return (static_cast<std::uint64_t>(t.high) << 32) | static_cast<std::uint64_t>(t.low);
}

inline void log_line(const std::string& src, const std::string& level, const std::string& msg) {
    SimLogger log;
    log << "# [" << src << "][" << level << "][" << sim_time_ns() << "] " << msg << std::endl;
}

} // namespace vip::common

#endif // VIP_COMMON_SIM_LOGGER_HPP
