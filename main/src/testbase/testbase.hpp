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
#include <memory>
#include <exception>
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

  // Enum class and conversion factors
  enum class TimeUnit { ms, us, ns, ps };

  inline constexpr auto ms = TimeUnit::ms;
  inline constexpr auto us = TimeUnit::us;
  inline constexpr auto ns = TimeUnit::ns;
  inline constexpr auto ps = TimeUnit::ps;

  template <TimeUnit T>
  struct TimeUnitConversion;

  template <>
  struct TimeUnitConversion<TimeUnit::ms> {
    static constexpr double factor = 1e-3;
  };

  template <>
  struct TimeUnitConversion<TimeUnit::us> {
    static constexpr double factor = 1e-6;
  };

  template <>
  struct TimeUnitConversion<TimeUnit::ns> {
    static constexpr double factor = 1e-9;
  };

  template <>
  struct TimeUnitConversion<TimeUnit::ps> {
    static constexpr double factor = 1e-12;
  };

  /**
   * @class TestBase
   *
   * @brief Base class for DUT unit testing operations.
   */
  class TestBase {
  public:
    explicit TestBase()
      : dutName()
        , sim_time_unit(0.0)
        , netMap() {
    }

    // Set the DUT name
    void setDutName(const std::string& name) {
      dutName = name;
      std::printf("Top level DUT: %s\n", dutName.c_str());
    }

    virtual void initNets() = 0; // Force derived classes to implement

    // Function for updating simulation time unit
    void updateSimTimeUnit(const double SimTimeUnit) {
      sim_time_unit = SimTimeUnit;
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
      AwaitWrite(TestBase& parentRef, const unsigned long long int delay)
        : cb_handle(nullptr)
          , parent(parentRef)
          , delay(delay)
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

      // for modifying the delay of the callback object
      template <TimeUnit T>
      void setDelay(double delay);
      // same as setDelay but with default ns precision argument
      void setDelay(double delay);

    private:
      vpiHandle cb_handle; // handle for a callback
      TestBase& parent; // reference to the DUT test object of Test class
      unsigned long long int delay; // simulation time delay
      std::unordered_map<std::string, t_write_value> grouped_writes; // write ops
      std::coroutine_handle<> handle; // coroutine handle
    };

    // ============================================================
    // AwaitRead
    // ============================================================
    class AwaitRead {
    public:
      // Main constructor. Gets reference to base class object and delay for event scheduling
      AwaitRead(TestBase& parentRef, const unsigned long long int delay)
        : cb_handle(nullptr)
          , parent(parentRef)
          , delay(delay)
          , grouped_reads()
          , rdTime(0)
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

      // for modifying the delay of the callback object
      template <TimeUnit T>
      void setDelay(double delay);
      void setDelay(double delay); // Default ns format

      template <TimeUnit T>
      double getTime() const;
      double getTime() const; // Default ns format

    private:
      vpiHandle cb_handle; // handle for a callback
      std::string getStr(const std::string& netStr, unsigned int base = 2);
      TestBase& parent; // reference to the DUT test object of Test class
      unsigned long long int delay; // simulation time delay
      std::unordered_map<std::string, t_read_value> grouped_reads; // read ops
      unsigned long long rdTime; // Current read simulation time
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
          , rdTime(0)
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
          , rdTime(0)
          , handle(nullptr) {
        rd_change_value.strValue.clear();
        rd_change_value.uintValues.clear();
      }

      template <TimeUnit T>
      double getTime() const;
      double getTime() const; // Default ns format

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
      unsigned long long rdTime; // Current read simulation time
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
    template <TimeUnit T>
    AwaitWrite getCoWrite(const double delay) {
      constexpr double factor = TimeUnitConversion<T>::factor;
      double adjusted_delay = delay * factor / sim_time_unit;
      return AwaitWrite{*this, static_cast<unsigned long long int>(adjusted_delay)};
    }

    AwaitWrite getCoWrite(const double delay) {
      return getCoWrite<ns>(delay);
    }

    AwaitChange getCoChange(const std::string& net) {
      return AwaitChange{*this, net};
    }

    template <TimeUnit T>
    AwaitRead getCoRead(const double delay) {
      constexpr double factor = TimeUnitConversion<T>::factor;
      double adjusted_delay = delay * factor / sim_time_unit;
      return AwaitRead{*this, static_cast<unsigned long long int>(adjusted_delay)};
    }

    AwaitRead getCoRead(const double delay) {
      return getCoRead<ns>(delay);
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
    std::string dutName; // name of the DUT
    double sim_time_unit; // simulation time unit
    std::unordered_map<std::string, t_netmap_value> netMap; // [key, value] list of DUT signals
  };
} // namespace test

#endif // DUT_TOP_TESTBASE_HPP
