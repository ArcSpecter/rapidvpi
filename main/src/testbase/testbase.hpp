// MIT License
//
// Copyright (c) 2024 Rovshan Rustamov
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#ifndef DUT_TOP_TESTBASE_HPP
#define DUT_TOP_TESTBASE_HPP

#include <string>
#include <cmath>
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <memory>
#include <exception>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdio>  // for printf / std::printf

// VPI library
#include <vpi_user.h>

#include "testmanager.hpp"
#include "scheduler.hpp"

namespace test {
  typedef struct s_write_value {
    std::string strValue;
    unsigned long long ullValue;
    PLI_INT32 flag; // Flag for forced write or regular no delay
  } t_write_value;

  typedef struct s_netmap_value {
    unsigned int length; // net length in bits
    vpiHandle vpi_handle;
  } t_netmap_value;

  typedef struct s_read_value {
    std::string strValue;
    std::vector<unsigned int> uintValues;
  } t_read_value;

  using sim_tick_t = std::uint64_t;

  enum class TimeUnit {
    ticks,
    ps,
    ns,
    us,
    ms
  };

  inline constexpr auto ticks = TimeUnit::ticks;
  inline constexpr auto ps = TimeUnit::ps;
  inline constexpr auto ns = TimeUnit::ns;
  inline constexpr auto us = TimeUnit::us;
  inline constexpr auto ms = TimeUnit::ms;

  template <TimeUnit U>
  struct TimeUnitSeconds;

  template <>
  struct TimeUnitSeconds<TimeUnit::ms> {
    static constexpr long double value = 1.0e-3L;
  };

  template <>
  struct TimeUnitSeconds<TimeUnit::us> {
    static constexpr long double value = 1.0e-6L;
  };

  template <>
  struct TimeUnitSeconds<TimeUnit::ns> {
    static constexpr long double value = 1.0e-9L;
  };

  template <>
  struct TimeUnitSeconds<TimeUnit::ps> {
    static constexpr long double value = 1.0e-12L;
  };

  template <TimeUnit U>
  struct TimeValueType {
    using type = double;
  };

  template <>
  struct TimeValueType<TimeUnit::ticks> {
    using type = sim_tick_t;
  };

  template <TimeUnit U>
  using time_value_t = typename TimeValueType<U>::type;

  template <TimeUnit U>
  using delay_arg_t = time_value_t<U>;

  namespace detail {
    [[nodiscard]] inline sim_tick_t vpi_time_to_ticks(const s_vpi_time& time) noexcept {
      const auto high = static_cast<std::uint32_t>(time.high);
      const auto low = static_cast<std::uint32_t>(time.low);

      return (static_cast<sim_tick_t>(high) << 32u) |
        static_cast<sim_tick_t>(low);
    }

    inline void set_vpi_time_from_ticks(s_vpi_time& time,
                                        const sim_tick_t tick_count) noexcept {
      const auto high = static_cast<std::uint32_t>(tick_count >> 32u);
      const auto low = static_cast<std::uint32_t>(tick_count & 0xffffffffULL);

      time.type = vpiSimTime;
      time.high = static_cast<PLI_UINT32>(high);
      time.low = static_cast<PLI_UINT32>(low);
    }

    [[nodiscard]] inline sim_tick_t current_vpi_time_ticks() noexcept {
      s_vpi_time time{};
      time.type = vpiSimTime;
      vpi_get_time(nullptr, &time);
      return vpi_time_to_ticks(time);
    }
  } // namespace detail

  /**
   * @class TestBase
   *
   * @brief Base class for DUT unit testing operations.
   */
  class TestBase {
  public:
    explicit TestBase()
      : dutName()
        , vpi_time_precision_exp10_(0)
        , vpi_tick_period_s_(0.0L)
        , netMap() {
    }

    // Set the DUT name
    void setDutName(const std::string& name) {
      dutName = name;
      std::printf("Top level DUT: %s\n", dutName.c_str());
    }

    virtual void initNets() = 0; // Force derived classes to implement

    // Function for updating effective VPI simulator time precision
    void updateVpiTimePrecision(const int precision_exp10) {
      vpi_time_precision_exp10_ = precision_exp10;
      vpi_tick_period_s_ = std::pow(10.0L, static_cast<long double>(precision_exp10));
    }

    [[nodiscard]] int vpiTimePrecisionExp10() const noexcept {
      return vpi_time_precision_exp10_;
    }

    [[nodiscard]] long double vpiTickPeriodSeconds() const noexcept {
      return vpi_tick_period_s_;
    }

    // Net operations
    void addNet(const std::string& key, unsigned int length); // add net to netMap
    vpiHandle getNetHandle(const std::string& key); // get vpi handle of given net
    unsigned int getNetLength(const std::string& key); // get bit length of given net

    // Auxiliary operations
    static char bin_to_hex_char(const std::string& bin);
    static std::string bin_to_hex(const std::string& bin);
    static std::string hex_to_bin(const std::string& hex);

    // ============================================================
    // AwaitWrite
    // ============================================================
    class AwaitWrite {
    public:
      // Main constructor. Gets reference to base class object and delay for event scheduling
      AwaitWrite(TestBase& parentRef, const sim_tick_t delay_ticks)
        : cb_handle(nullptr)
          , parent(parentRef)
          , delay_ticks(delay_ticks)
          , grouped_writes()
          , handle(nullptr) {
      }

      // Coroutine service functions
      bool await_ready() const noexcept { return false; }
      void await_suspend(std::coroutine_handle<> h);
      void await_resume() noexcept;

      // adds write operation to grouped_writes; parameter is uint64
      void write(const std::string& netStr, unsigned long long int value);

      // adds force operation to grouped_writes; parameter is uint64
      void force(const std::string& netStr, unsigned long long int value);

      // adds force-release operation to grouped_writes
      void release(const std::string& netStr);

      // adds write operation to grouped_writes; parameter is string
      void write(const std::string& netStr, const std::string& valStr, unsigned int base = 2);

      // adds force operation to grouped_writes; parameter is string
      void force(const std::string& netStr, const std::string& valStr, unsigned int base = 2);

      void setDelay() {
        delay_ticks = 0;
      }

      template <TimeUnit U>
      void setDelay(delay_arg_t<U> delay);

    private:
      vpiHandle cb_handle; // handle for a callback
      TestBase& parent; // reference to the DUT test object of Test class
      sim_tick_t delay_ticks; // raw simulator tick delay
      std::unordered_map<std::string, t_write_value> grouped_writes; // write ops
      std::coroutine_handle<> handle; // coroutine handle
    };

    // ============================================================
    // AwaitRead
    // ============================================================
    class AwaitRead {
    public:
      // Main constructor. Gets reference to base class object and delay for event scheduling
      AwaitRead(TestBase& parentRef, const sim_tick_t delay_ticks)
        : cb_handle(nullptr)
          , parent(parentRef)
          , delay_ticks(delay_ticks)
          , grouped_reads()
          , resume_time_ticks(0)
          , handle(nullptr) {
      }

      // adds read operation to grouped_reads
      void read(const std::string& netStr);

      unsigned long long int getNum(const std::string& netStr);

      std::string getBinStr(const std::string& netStr); // For binary value string
      std::string getHexStr(const std::string& netStr); // For hex value string

      // Coroutine service functions
      bool await_ready() const noexcept { return false; }
      void await_suspend(std::coroutine_handle<> h);
      void await_resume() noexcept;

      void setDelay() {
        delay_ticks = 0;
      }

      template <TimeUnit U>
      void setDelay(delay_arg_t<U> delay);

      template <TimeUnit U>
      time_value_t<U> getTime() const;

    private:
      vpiHandle cb_handle; // handle for a callback
      std::string getStr(const std::string& netStr, unsigned int base = 2);
      TestBase& parent; // reference to the DUT test object of Test class
      sim_tick_t delay_ticks; // raw simulator tick delay
      std::unordered_map<std::string, t_read_value> grouped_reads; // read ops
      sim_tick_t resume_time_ticks; // current read simulation time in raw ticks
      std::coroutine_handle<> handle; // coroutine handle
    };

    // ============================================================
    // AwaitChange
    // ============================================================
    class AwaitChange {
    public:
      // Main constructor. Gets reference to base class and net monitored for change
      AwaitChange(TestBase& parentRef, std::string net)
        : parent(parentRef)
          , net(std::move(net))
          , change_target_value(0)
          , change_is_targeted(false)
          , rd_change_value()
          , cb_handle(nullptr)
          , resume_time_ticks(0)
          , handle(nullptr) {
      }

      // Targeted change constructor
      AwaitChange(TestBase& parentRef, std::string net, unsigned long long int target_value)
        : parent(parentRef)
          , net(std::move(net))
          , change_target_value(target_value)
          , change_is_targeted(true)
          , rd_change_value()
          , cb_handle(nullptr)
          , resume_time_ticks(0)
          , handle(nullptr) {
        rd_change_value.strValue.clear();
        rd_change_value.uintValues.clear();
      }

      template <TimeUnit U>
      time_value_t<U> getTime() const;

      // Coroutine service functions
      bool await_ready() const noexcept { return false; }
      void await_suspend(std::coroutine_handle<> h);
      void await_resume() noexcept;

      unsigned long long int getNum();
      std::string getBinStr();
      std::string getHexStr();

    private:
      std::string getStr(unsigned int base);
      TestBase& parent; // reference to the DUT test object of Test class
      std::string net; // net name for monitored change
      unsigned long long int change_target_value; // target value for monitored change

      // flag telling whether or not the change monitoring is looking for certain target
      bool change_is_targeted;
      t_read_value rd_change_value; // holds the change value being read

      vpiHandle cb_handle; // handle for a callback for cbValueChange
      sim_tick_t resume_time_ticks; // current change simulation time in raw ticks
      std::coroutine_handle<> handle; // coroutine handle
    };

    // ============================================================
    // RunTask  (top-level test coroutines)
    // ============================================================
    struct RunTask {
      struct promise_type {
        using Handle = std::coroutine_handle<promise_type>;

        TestBase* test_instance{nullptr};

        RunTask get_return_object() {
          return RunTask{Handle::from_promise(*this)};
        }

        std::suspend_never initial_suspend() noexcept { return {}; }

        // When the top-level test coroutine finishes, destroy its frame.
        auto final_suspend() noexcept {
          struct FinalAwaiter {
            bool await_ready() const noexcept { return false; }

            void await_suspend(Handle h) noexcept {
              // Coroutine has fully finished, safe to destroy.
              h.destroy();
            }

            void await_resume() noexcept {
            }
          };
          return FinalAwaiter{};
        }

        void return_void() {
        }

        void unhandled_exception() { std::terminate(); }
      };

      using Handle = std::coroutine_handle<promise_type>;
      Handle handle{};

      explicit RunTask(Handle h) : handle(h) {
      }

      RunTask(const RunTask&) = default;
      RunTask& operator=(const RunTask&) = default;
      RunTask(RunTask&&) = default;
      RunTask& operator=(RunTask&&) = default;

      ~RunTask() = default; // non-owning wrapper; coroutine self-destroys
    };

    // ============================================================
    // RunUserTask  (nested user coroutines: delay_ns, clock, etc.)
    // ============================================================
    struct RunUserTask {
      struct promise_type {
        using Handle = std::coroutine_handle<promise_type>;

        // Parent coroutine to resume when this child finishes
        std::coroutine_handle<> parentHandle{};

        // Create the coroutine object
        RunUserTask get_return_object() {
          return RunUserTask{Handle::from_promise(*this)};
        }

        // Start suspended; parent explicitly resumes via co_await
        std::suspend_always initial_suspend() noexcept {
          return {};
        }

        // On completion: resume parent, then destroy child frame
        auto final_suspend() noexcept {
          struct FinalAwaiter {
            bool await_ready() const noexcept { return false; }

            void await_suspend(Handle h) noexcept {
              auto& promise = h.promise();

              if (promise.parentHandle) {
                promise.parentHandle.resume();
              }

              // Child coroutine owns itself and destroys its frame here
              h.destroy();
            }

            void await_resume() noexcept {
            }
          };
          return FinalAwaiter{};
        }

        void return_void() {
        }

        void unhandled_exception() { std::terminate(); }
      };

      using Handle = std::coroutine_handle<promise_type>;
      Handle handle{};

      explicit RunUserTask(Handle h) : handle(h) {
        if (!h) {
          std::terminate();
        }
      }

      // Non-owning: do NOT call destroy() here; child destroys itself in final_suspend.
      ~RunUserTask() = default;

      RunUserTask(const RunUserTask&) = default;
      RunUserTask& operator=(const RunUserTask&) = default;
      RunUserTask(RunUserTask&&) = default;
      RunUserTask& operator=(RunUserTask&&) = default;

      // The operator co_await will track the parent's handle and then resume this child
      auto operator co_await() noexcept {
        struct Awaiter {
          Handle childHandle;
          bool await_ready() const noexcept { return false; }

          // Save caller's handle, resume child, so child can eventually resume caller
          void await_suspend(std::coroutine_handle<> caller) noexcept {
            childHandle.promise().parentHandle = caller;
            childHandle.resume();
          }

          void await_resume() noexcept {
            // Parent continues after child is completely finished
          }
        };
        return Awaiter{handle};
      }
    };

    // ============================================================
    // Factory helpers
    // ============================================================
    AwaitWrite getCoWrite() {
      return AwaitWrite{*this, 0};
    }

    template <TimeUnit U>
    AwaitWrite getCoWrite(const delay_arg_t<U> delay) {
      return AwaitWrite{*this, delay_to_ticks_<U>(delay)};
    }

    AwaitChange getCoChange(const std::string& net) {
      return AwaitChange{*this, net};
    }

    AwaitRead getCoRead() {
      return AwaitRead{*this, 0};
    }

    template <TimeUnit U>
    AwaitRead getCoRead(const delay_arg_t<U> delay) {
      return AwaitRead{*this, delay_to_ticks_<U>(delay)};
    }

    AwaitChange getCoChange(const std::string& net, unsigned long long int target_value) {
      return AwaitChange{*this, net, target_value};
    }

    // ============================================================
    // Test registration
    // ============================================================
    void registerTest(const std::string& name, std::function<std::coroutine_handle<>()> func) {
      RegistrationHelper::registerTest(this, name, func);
    }

    // handles for coroutine test functions
    std::vector<std::coroutine_handle<>> test_handles;

  private:
    [[nodiscard]] long double require_vpi_tick_period_s_() const {
      if (vpi_tick_period_s_ <= 0.0L) {
        throw std::runtime_error("RapidVPI VPI time precision has not been initialized");
      }
      return vpi_tick_period_s_;
    }

    template <TimeUnit U>
    [[nodiscard]] sim_tick_t delay_to_ticks_(const delay_arg_t<U> delay) const {
      if constexpr (U == TimeUnit::ticks) {
        return delay;
      }
      else {
        if (delay <= 0.0) {
          return 0;
        }

        const long double raw_ticks =
          static_cast<long double>(delay) * TimeUnitSeconds<U>::value /
          require_vpi_tick_period_s_();

        return ceil_delay_ticks_(raw_ticks);
      }
    }

    template <TimeUnit U>
    [[nodiscard]] time_value_t<U> ticks_to_time_(const sim_tick_t tick_count) const {
      if constexpr (U == TimeUnit::ticks) {
        return tick_count;
      }
      else {
        const long double seconds =
          static_cast<long double>(tick_count) * require_vpi_tick_period_s_();
        const long double value = seconds / TimeUnitSeconds<U>::value;
        return static_cast<double>(value);
      }
    }

    [[nodiscard]] static sim_tick_t ceil_delay_ticks_(const long double raw_ticks) {
      if (raw_ticks <= 0.0L) {
        return 0;
      }
      if (!std::isfinite(raw_ticks)) {
        throw std::overflow_error("RapidVPI delay exceeds sim_tick_t range");
      }

      constexpr long double rel_eps = 1.0e-12L;
      const long double nearest = std::floor(raw_ticks + 0.5L);
      const long double diff = raw_ticks > nearest ? raw_ticks - nearest : nearest - raw_ticks;
      const long double scale = raw_ticks > 1.0L ? raw_ticks : 1.0L;

      long double rounded_ticks;
      if (diff <= rel_eps * scale) {
        rounded_ticks = nearest;
      }
      else {
        rounded_ticks = std::ceil(raw_ticks);
      }

      const long double max_ticks =
        static_cast<long double>(std::numeric_limits<sim_tick_t>::max());
      if (rounded_ticks > max_ticks) {
        throw std::overflow_error("RapidVPI delay exceeds sim_tick_t range");
      }

      return static_cast<sim_tick_t>(rounded_ticks);
    }

    std::string dutName; // name of the DUT
    int vpi_time_precision_exp10_; // vpi_get(vpiTimePrecision, nullptr) result
    long double vpi_tick_period_s_; // physical duration of one raw VPI tick
    std::unordered_map<std::string, t_netmap_value> netMap; // [key, value] list of DUT signals
  };

  template <TimeUnit U>
  inline void TestBase::AwaitWrite::setDelay(const delay_arg_t<U> delay) {
    delay_ticks = parent.delay_to_ticks_<U>(delay);
  }

  template <TimeUnit U>
  inline void TestBase::AwaitRead::setDelay(const delay_arg_t<U> delay) {
    delay_ticks = parent.delay_to_ticks_<U>(delay);
  }

  template <TimeUnit U>
  inline time_value_t<U> TestBase::AwaitRead::getTime() const {
    return parent.ticks_to_time_<U>(resume_time_ticks);
  }

  template <TimeUnit U>
  inline time_value_t<U> TestBase::AwaitChange::getTime() const {
    return parent.ticks_to_time_<U>(resume_time_ticks);
  }
} // namespace test

#endif // DUT_TOP_TESTBASE_HPP
