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

  // Enum class and conversion factors (already defined)
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
   *
   * This class provides functionalities for handing DUT unit test operations,
   * such as simulating various tasks like writing and reading signals, and monitoring
   * net changes.
   */
  class TestBase {
  public:
    explicit TestBase() {
      sim_time_unit = 0;
    }

    // Set the DUT name
    void setDutName(const std::string& name) {
      dutName = name;
      printf("Top level DUT: %s\n", dutName.c_str());
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

    // This class is an Awaiter object to be used for RunTask coroutine
    class AwaitWrite {
    public:
      // Main constructor. Gets reference to base class object and delay for
      // the event scheduling
      AwaitWrite(TestBase& parentRef, const unsigned long long int delay) : parent(parentRef), delay(delay) {
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
      TestBase& parent; // reference to the DUT test object of Test class
      unsigned long long int delay; // simulation time delay
      std::unordered_map<std::string, t_write_value> grouped_writes; // write ops
      std::coroutine_handle<> handle; // coroutine handle
    };


    class AwaitRead {
    public:
      // Main constructor. Gets reference to base class object and delay for
      // the event scheduling
      AwaitRead(TestBase& parentRef, const unsigned long long int delay) : parent(parentRef), delay(delay) {
      }

      // adds write operation to grouped_writes
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

      // same as setDelay but with default ns precision argument
      void setDelay(double delay);

    private:
      std::string getStr(const std::string& netStr, unsigned int base = 2);
      TestBase& parent; // reference to the DUT test object of Test class
      unsigned long long int delay; // simulation time delay
      std::unordered_map<std::string, t_read_value> grouped_reads; // read ops
      std::coroutine_handle<> handle; // coroutine handle
    };

    // This class is an Awaiter object to be used for RunTask coroutine
    class AwaitChange {
    public:
      // Main constructor. Gets reference to base class and net monitored for change
      AwaitChange(TestBase& parentRef, std::string net) : parent(parentRef), net(std::move(net)) {
        change_is_targeted = false;
      }

      // Targeted change constructor. Gets reference to base class, net monitored for a change
      // and the target change for which the net is monitored
      AwaitChange(TestBase& parentRef, std::string net, unsigned long long int target_value)
        : parent(parentRef), net(std::move(net)), change_target_value(target_value) {
        change_is_targeted = true;
        rd_change_value.strValue = "";
        rd_change_value.uintValues = {};
      }

      // adds write operation to grouped_writes
      void write(const std::string& netStr, unsigned int value);

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
      std::coroutine_handle<> handle; // coroutine handle
    };


    /**
     * @struct RunTask
     *
     * @brief Coroutine task structure for handling asynchronous operations.
     * The RunTask type is the main type used for the user to instantiate his parallel tests
     * After the tests are declared and registered in the Test class, the test manager object
     * is created in the API core engine and concurrent test execution starts
     */
    struct RunTask {
      struct promise_type {
        TestBase* test_instance;
        std::coroutine_handle<> handle;

        RunTask get_return_object() {
          return RunTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }

        void return_void() {
        }

        void unhandled_exception() { std::terminate(); }
      };

      std::coroutine_handle<promise_type> handle;
    };


    /**
     * @struct RunUserTask
     *
     * @brief Provides management of a coroutine for executing user-defined tasks with parent-child coroutine relationships.
     *
     * This structure represents a coroutine adapter that tracks parent coroutine handles and manages task execution flow.
     * The associated promise type ensures proper initialization, suspension, resumption, and destruction of the coroutine.
     * It also facilitates awaiting of child coroutines by the parent coroutine.
     */
    struct RunUserTask {
      struct promise_type {
        // We'll keep track of the parent handle so we can resume it later
        std::coroutine_handle<> parentHandle;

        // Create the coroutine object
        RunUserTask get_return_object() {
          return RunUserTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        // Start suspended
        std::suspend_always initial_suspend() noexcept {
          return {};
        }

        // In final_suspend, resume the parent (if valid)
        auto final_suspend() noexcept {
          struct FinalAwaiter {
            bool await_ready() const noexcept { return false; }
            // Resume the parent handle stored in promise_type when the child finishes
            void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
              auto& promise = h.promise();
              if (promise.parentHandle) {
                promise.parentHandle.resume();
              }
            }

            void await_resume() noexcept {
            }
          };
          return FinalAwaiter{};
        }

        // Standard boilerplate
        void return_void() {
        }

        void unhandled_exception() {
          std::terminate();
        }
      };

      using Handle = std::coroutine_handle<promise_type>;
      Handle handle;

      explicit RunUserTask(Handle h) : handle(h) {
        if (!h) {
          std::terminate();
        }
      }

      ~RunUserTask() {
        if (handle) {
          handle.destroy();
        }
      }

      // The operator co_await will track the parent's handle and then resume this child
      auto operator co_await() noexcept {
        struct Awaiter {
          Handle childHandle;
          bool await_ready() const noexcept { return false; }

          // Save caller's handle, resume child, so child can eventually resume caller
          void await_suspend(std::coroutine_handle<> caller) noexcept {
            // Store the parent's handle so final_suspend can resume it
            childHandle.promise().parentHandle = caller;
            childHandle.resume();
          }

          void await_resume() noexcept {
          }
        };
        return Awaiter{handle};
      }
    };


    /**
     * @brief Returns an AwaitWrite object with adjusted delay for co-routine event scheduling.
     *
     * This function creates an AwaitWrite object, adjusting the provided delay according to the
     * relevant time unit conversion factor and simulation time unit.
     *
     * @param delay The delay in the specified time unit before the write operation is executed.
     * @return AwaitWrite object configured with the adjusted delay.
     */
    template <TimeUnit T>
    AwaitWrite getCoWrite(const double delay) {
      constexpr double factor = TimeUnitConversion<T>::factor;
      double adjusted_delay = delay * factor / sim_time_unit; // Mixed compile-time and runtime
      return AwaitWrite{*this, static_cast<unsigned long long int>(adjusted_delay)};
    }

    /**
     * @brief Returns an AwaitWrite object with a delay specified in nanoseconds
     * for co-routine event scheduling.
     *
     * @param delay The delay in nanoseconds before the write operation is executed.
     * @return AwaitWrite object configured with the given delay.
     */
    AwaitWrite getCoWrite(const double delay) {
      return getCoWrite<ns>(delay);
    }

    /**
     * @brief Returns an AwaitChange object for monitoring changes to a specified net.
     *
     * This function creates an AwaitChange object that allows the coroutine to wait
     * for changes to occur on the specified net.
     *
     * @param net The name of the net to be monitored for changes.
     * @return AwaitChange object configured to monitor the specified net.
     */
    AwaitChange getCoChange(const std::string& net) {
      return AwaitChange{*this, net};
    }

    /**
     * @brief Returns an AwaitRead object with adjusted delay for co-routine event scheduling.
     *
     * This function creates an AwaitRead object, adjusting the provided delay according to the
     * relevant time unit conversion factor and simulation time unit.
     *
     * @param delay The delay in the specified time unit before the read operation is executed.
     * @return AwaitRead object configured with the adjusted delay.
     */
    template <TimeUnit T>
    AwaitRead getCoRead(const double delay) {
      constexpr double factor = TimeUnitConversion<T>::factor;
      double adjusted_delay = delay * factor / sim_time_unit; // Mixed compile-time and runtime
      return AwaitRead{*this, static_cast<unsigned long long int>(adjusted_delay)};
    }

    /**
     * @brief Returns an AwaitRead object with a delay specified in nanoseconds
     * for co-routine event scheduling.
     *
     * @param delay The delay in nanoseconds before the read operation is executed.
     * @return AwaitRead object configured with the given delay.
     */
    AwaitRead getCoRead(const double delay) {
      return getCoRead<ns>(delay);
    }

    /**
     * @brief Returns an AwaitChange object for monitoring a specified net until a target value is reached.
     *
     * This function creates an AwaitChange object that allows the coroutine to wait
     * until the specified net changes to the provided target value.
     *
     * @param net The name of the net to be monitored for changes.
     * @param target_value The target value that the net is expected to change to.
     * @return AwaitChange object configured to monitor the specified net for the target value.
     */
    AwaitChange getCoChange(const std::string& net, unsigned long long int target_value) {
      return AwaitChange{*this, net, target_value};
    }


    /**
     * @brief Registers a test function with a specified name.
     *
     * This function allows the registration of a coroutine test function
     * in the test framework by associating it with a unique name.
     *
     * @param name The name to associate with the test function.
     * @param func The coroutine test function to be registered.
     */
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
}
#endif //DUT_TOP_TESTBASE_HPP
