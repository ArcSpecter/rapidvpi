// vip_common/runner/runner.hpp
//
// Generic testcase runner/orchestrator.
//
// This lives in vip_common so it can be reused across projects.
// It must not depend on any project-specific Test-derived type.

#ifndef VIP_COMMON_RUNNER_HPP
#define VIP_COMMON_RUNNER_HPP

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "vip_common/common/common.hpp"
#include "vip_common/common/logger.hpp"

namespace vip::common {

class Runner {
public:
    using RunTask = TestBase::RunTask;        // top-level tasks registered into RapidVPI
    using RunUserTask = TestBase::RunUserTask; // awaitable tasks used inside other coroutines

    struct CaseDesc {
        std::string name;
        bool enabled_by_default = true;

        // Optional metadata; runner itself doesn't use tags directly, but they
        // can be useful for later filtering or regression selection.
        std::vector<std::string> tags;

        // Case coroutine body (awaitable).
        std::function<RunUserTask(void)> fn;
    };

    using CaseHook = std::function<void(const CaseDesc&)>;
    using AfterAllHook = std::function<void()>;

    explicit Runner(TestBase& tb, AfterAllHook after_all = {});

    // Registers the runner's top-level coroutine into RapidVPI.
    // The task name is "case_runner".
    void register_tasks();

    // Adds a testcase to the registry.
    // Names must be unique.
    void add_case(const CaseDesc& c);

    // Defines an explicit ordered plan (what to run, in what order).
    // Dependencies can still reorder cases if necessary.
    void set_plan(const std::vector<std::string>& ordered_case_names);

    // Builds an explicit plan from tags, in registration order.
    void set_plan_tagged(std::initializer_list<std::string> tags,
                         bool include_disabled = false);

    // Clears the explicit plan. Runner will then run all enabled_by_default cases.
    void clear_plan();

    // Adds a dependency: case_name must run AFTER after_name.
    // Example: add_after("tc_7", "tc_9") -> tc_9 executes before tc_7.
    void add_after(const std::string& case_name, const std::string& after_name);

    // Optional hooks
    void set_before_case_hook(CaseHook h) { before_case_ = std::move(h); }
    void set_after_case_hook(CaseHook h) { after_case_ = std::move(h); }

    // Called once after all cases complete (or immediately if no cases selected).
    // Intended for project-owned cleanup such as trace finalization.
    void set_after_all_hook(AfterAllHook h) { after_all_ = std::move(h); }

    // Main runner coroutine (registered as a top-level task).
    RunTask case_runner();

private:
    TestBase& tb_;

    std::vector<CaseDesc> cases_;
    std::unordered_map<std::string, std::size_t> name_to_index_;

    // Optional explicit plan (ordered).
    std::vector<std::string> plan_;

    // Dependencies: case -> list of prerequisites (must run before case).
    std::unordered_map<std::string, std::vector<std::string>> prereq_;

    CaseHook before_case_;
    CaseHook after_case_;
    AfterAllHook after_all_;

    // Logger (VPI backed)
    mutable SimLogger log_;

    // Computes selected case set (plan or enabled_by_default) and returns an
    // execution order satisfying dependencies (stable tie-break).
    std::vector<std::size_t> compute_execution_order() const;

    // Helpers
    bool case_exists_(const std::string& name) const;

    static sim_tick_t sim_time_ticks_();
    void log_line_(const std::string& level, const std::string& msg) const;
};

} // namespace vip::common

#endif // VIP_COMMON_RUNNER_HPP
