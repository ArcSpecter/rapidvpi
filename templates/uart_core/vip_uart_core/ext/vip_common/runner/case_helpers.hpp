// vip_common/runner/case_helpers.hpp
//
// Small helpers to register testcases with vip::common::Runner without
// repeating the CaseDesc boilerplate.

#ifndef VIP_COMMON_CASE_HELPERS_HPP
#define VIP_COMMON_CASE_HELPERS_HPP

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

#include "vip_common/runner/runner.hpp"

namespace vip::common {

// Add a case to the runner.
//
// - name: case name (unique)
// - tags: optional tags (used for filtering/selection)
// - enabled_by_default: whether it runs when no explicit plan is set
// - fn: coroutine factory that returns a RunUserTask
template <class Fn>
inline void add_case(Runner& runner,
                     std::string_view name,
                     std::initializer_list<std::string_view> tags,
                     bool enabled_by_default,
                     Fn&& fn) {
    Runner::CaseDesc c;
    c.name = std::string(name);
    c.enabled_by_default = enabled_by_default;

    c.tags.reserve(tags.size());
    for (const auto& t : tags) {
        c.tags.emplace_back(t);
    }

    c.fn = std::forward<Fn>(fn);
    runner.add_case(c);
}

} // namespace vip::common

#endif // VIP_COMMON_CASE_HELPERS_HPP
