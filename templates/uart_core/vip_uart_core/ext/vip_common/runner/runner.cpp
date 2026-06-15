// vip_common/runner/runner.cpp
#include "vip_common/runner/runner.hpp"

#include <algorithm>
#include <deque>
#include <unordered_set>

#include <vpi_user.h>

namespace vip::common {

std::uint64_t Runner::sim_time_ns_() {
    s_vpi_time t;
    t.type = vpiSimTime;
    t.high = 0;
    t.low = 0;
    vpi_get_time(nullptr, &t);
    return (static_cast<std::uint64_t>(t.high) << 32) | static_cast<std::uint64_t>(t.low);
}

void Runner::log_line_(const std::string& level, const std::string& msg) const {
    log_ << "# [runner][" << level << "][" << sim_time_ns_() << "] " << msg << std::endl;
}

Runner::Runner(TestBase& tb, AfterAllHook after_all)
    : tb_(tb)
    , after_all_(std::move(after_all))
    , log_() {}

void Runner::register_tasks() {
    tb_.registerTest("case_runner", [this]() { return this->case_runner().handle; });
}

void Runner::add_case(const CaseDesc& c) {
    // Enforce unique names.
    if (name_to_index_.find(c.name) != name_to_index_.end()) {
        log_line_("FAIL", "duplicate case name registered: " + c.name);
        return;
    }

    name_to_index_[c.name] = cases_.size();
    cases_.push_back(c);
}

void Runner::set_plan(const std::vector<std::string>& ordered_case_names) {
    plan_ = ordered_case_names;
}

void Runner::clear_plan() {
    plan_.clear();
}

void Runner::set_plan_tagged(std::initializer_list<std::string> tags,
                             const bool include_disabled) {
    std::vector<std::string> plan;
    plan.reserve(cases_.size());

    for (const auto& c : cases_) {
        if (!include_disabled && !c.enabled_by_default) {
            continue;
        }

        bool hit = false;
        for (const auto& t : c.tags) {
            for (const auto& want : tags) {
                if (t == want) {
                    hit = true;
                    break;
                }
            }
            if (hit) break;
        }

        if (hit) plan.push_back(c.name);
    }

    set_plan(plan);
}

void Runner::add_after(const std::string& case_name, const std::string& after_name) {
    prereq_[case_name].push_back(after_name);
}

bool Runner::case_exists_(const std::string& name) const {
    return name_to_index_.find(name) != name_to_index_.end();
}

Runner::RunTask Runner::case_runner() {
    if (cases_.empty()) {
        log_line_("WARN", "no cases registered.");
        if (after_all_) {
            after_all_();
        }
        co_return;
    }

    const auto order = compute_execution_order();
    if (order.empty()) {
        log_line_("WARN", "no cases selected to run.");
        if (after_all_) {
            after_all_();
        }
        co_return;
    }

    log_line_("INFO", "running " + std::to_string(order.size()) + " case(s)");

    for (std::size_t k = 0; k < order.size(); ++k) {
        const CaseDesc& c = cases_[order[k]];

        log_line_("INFO", "case " + std::to_string(k + 1) + "/" + std::to_string(order.size()) + ": " + c.name);

        if (!c.fn) {
            log_line_("FAIL", "case has no function: " + c.name);
            continue;
        }

        if (before_case_) {
            before_case_(c);
        }

        co_await c.fn();

        if (after_case_) {
            after_case_(c);
        }
    }

    // Project-owned run-level cleanup.
    if (after_all_) {
        after_all_();
    }

    log_line_("INFO", "runner done.");
    co_return;
}

std::vector<std::size_t> Runner::compute_execution_order() const {
    // Step 1: Determine initial selection set
    // - If plan_ is non-empty: run those names (in that human order)
    // - Else: run all enabled_by_default in registration order
    std::vector<std::string> initial_names;

    if (!plan_.empty()) {
        initial_names = plan_;
    } else {
        for (const auto& c : cases_) {
            if (c.enabled_by_default) {
                initial_names.push_back(c.name);
            }
        }
    }

    // Filter unknown names (but keep going)
    std::vector<std::string> filtered;
    filtered.reserve(initial_names.size());

    for (const auto& n : initial_names) {
        if (!case_exists_(n)) {
            log_line_("FAIL", "selected case not registered: " + n);
            continue;
        }
        filtered.push_back(n);
    }

    if (filtered.empty()) {
        return {};
    }

    // Step 2: Build closure over prerequisites:
    // If A depends on B, and A is selected, include B too (if registered).
    std::unordered_set<std::string> selected_set;
    std::deque<std::string> q;

    for (const auto& n : filtered) {
        selected_set.insert(n);
        q.push_back(n);
    }

    while (!q.empty()) {
        const std::string cur = q.front();
        q.pop_front();

        auto itp = prereq_.find(cur);
        if (itp == prereq_.end()) {
            continue;
        }

        for (const auto& pre : itp->second) {
            if (!case_exists_(pre)) {
                log_line_("FAIL", "dependency refers to unknown case: " + cur + " after " + pre);
                continue;
            }
            if (selected_set.insert(pre).second) {
                q.push_back(pre);
            }
        }
    }

    // Step 3: Assign stable ranks for tie-breaking among indegree-0 nodes.
    // - Planned cases get rank by plan order
    // - Others get rank after plan by registration order
    std::unordered_map<std::string, std::size_t> rank;
    rank.reserve(selected_set.size());

    std::size_t base = 0;
    if (!plan_.empty()) {
        for (std::size_t i = 0; i < plan_.size(); ++i) {
            const auto& n = plan_[i];
            if (selected_set.find(n) != selected_set.end() && rank.find(n) == rank.end()) {
                rank[n] = i;
                base = std::max(base, i + 1);
            }
        }
    }

    std::size_t r = base;
    for (const auto& c : cases_) {
        if (selected_set.find(c.name) != selected_set.end() && rank.find(c.name) == rank.end()) {
            rank[c.name] = r++;
        }
    }

    // Step 4: Build graph among selected cases: edges pre -> cur
    std::unordered_map<std::string, std::vector<std::string>> adj;
    std::unordered_map<std::string, std::size_t> indeg;

    adj.reserve(selected_set.size());
    indeg.reserve(selected_set.size());

    for (const auto& n : selected_set) {
        indeg[n] = 0;
        adj[n] = {};
    }

    for (const auto& n : selected_set) {
        auto itp = prereq_.find(n);
        if (itp == prereq_.end()) {
            continue;
        }

        for (const auto& pre : itp->second) {
            if (selected_set.find(pre) == selected_set.end()) {
                continue;
            }

            adj[pre].push_back(n);
            indeg[n] += 1;
        }
    }

    // Step 5: Kahn topological sort with stable selection by min rank
    std::vector<std::string> ready;
    ready.reserve(selected_set.size());

    for (const auto& kv : indeg) {
        if (kv.second == 0) {
            ready.push_back(kv.first);
        }
    }

    auto pick_min_rank = [&](std::vector<std::string>& v) -> std::string {
        std::size_t best_i = 0;
        std::size_t best_r = static_cast<std::size_t>(-1);

        for (std::size_t i = 0; i < v.size(); ++i) {
            const auto it = rank.find(v[i]);
            const std::size_t rr = (it == rank.end()) ? static_cast<std::size_t>(-1) : it->second;
            if (rr < best_r) {
                best_r = rr;
                best_i = i;
            }
        }

        std::string out = v[best_i];
        v.erase(v.begin() + static_cast<long>(best_i));
        return out;
    };

    std::vector<std::string> ordered_names;
    ordered_names.reserve(selected_set.size());

    while (!ready.empty()) {
        const std::string n = pick_min_rank(ready);
        ordered_names.push_back(n);

        for (const auto& nxt : adj[n]) {
            auto it = indeg.find(nxt);
            if (it == indeg.end()) {
                continue;
            }

            if (it->second > 0) {
                it->second -= 1;
            }
            if (it->second == 0) {
                ready.push_back(nxt);
            }
        }
    }

    if (ordered_names.size() != selected_set.size()) {
        log_line_("FAIL", "dependency cycle detected; running remaining cases by rank order.");

        std::unordered_set<std::string> done(ordered_names.begin(), ordered_names.end());
        std::vector<std::string> leftover;
        leftover.reserve(selected_set.size());

        for (const auto& n : selected_set) {
            if (done.find(n) == done.end()) {
                leftover.push_back(n);
            }
        }

        std::sort(leftover.begin(), leftover.end(),
                  [&](const std::string& a, const std::string& b) {
                      return rank[a] < rank[b];
                  });

        for (const auto& n : leftover) {
            ordered_names.push_back(n);
        }
    }

    // Convert to indices
    std::vector<std::size_t> out;
    out.reserve(ordered_names.size());

    for (const auto& n : ordered_names) {
        auto it = name_to_index_.find(n);
        if (it == name_to_index_.end()) {
            continue;
        }
        out.push_back(it->second);
    }

    return out;
}

} // namespace vip::common
