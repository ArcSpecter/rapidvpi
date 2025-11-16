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


#include "testbase.hpp"

namespace test {
  template <TimeUnit T>
  void TestBase::AwaitWrite::setDelay(const double delay) {
    constexpr double factor = TimeUnitConversion<T>::factor;
    double adjusted_delay = delay * factor / parent.sim_time_unit; // Mixed compile-time and runtime
    this->delay = static_cast<unsigned long long int>(adjusted_delay);
  }

  void TestBase::AwaitWrite::setDelay(const double delay) {
    setDelay<ns>(delay);
  }

  // Explicit instantiations
  template void TestBase::AwaitWrite::setDelay<ps>(const double);
  template void TestBase::AwaitWrite::setDelay<ns>(const double);
  template void TestBase::AwaitWrite::setDelay<us>(const double);
  template void TestBase::AwaitWrite::setDelay<ms>(const double);

  /**
   * Suspends the coroutine and registers a scheduler callback to resume it after a delay.
   *
   * @param h The coroutine handle to be suspended and resumed later.
   */
  void TestBase::AwaitWrite::await_suspend(std::coroutine_handle<> h) {
    handle = h;

    // Allocate and prepare the callback data
    auto callbackData = std::make_unique<scheduler::SchedulerCallbackData>();
    callbackData->handle = h; // Store the coroutine handle

    // register scheduler callback here
    s_cb_data cb_data{};
    s_vpi_time tim{};

    tim.high = 0;
    tim.low = delay;
    tim.type = vpiSimTime;

    cb_data.reason = cbAfterDelay;
    cb_data.cb_rtn = &scheduler::write_callback;
    cb_data.obj = nullptr;
    cb_data.time = &tim;
    cb_data.value = nullptr; // allowed for cbAfterDelay

    // hand pointer but do NOT release yet
    cb_data.user_data = reinterpret_cast<PLI_BYTE8*>(callbackData.get());

    vpiHandle cbH = vpi_register_cb(&cb_data);
    if (cbH == nullptr) {
      std::printf("[WARNING]\tCannot register VPI Callback. TestBase::AwaitWrite:: %s\n",
                  __FUNCTION__);

      // Optional: dump VPI error info for debug
      s_vpi_error_info err{};
      if (vpi_chk_error(&err)) {
        std::printf("[VPI ERROR]\tcode=%s msg=%s file=%s line=%d\n",
                    err.code ? err.code : "(null)",
                    err.message ? err.message : "(null)",
                    err.file ? err.file : "(null)",
                    static_cast<int>(err.line));
      }
      else {
        std::printf("[VPI ERROR]\tNo additional VPI error info.\n");
      }

      cb_handle = nullptr;
      // registration failed -> unique_ptr auto-frees callbackData
      return;
    }

    // success: scheduler::write_callback will delete user_data
    (void)callbackData.release();
    cb_handle = cbH; // save handle so that we can unregister inside await_resume()
  }

  /**
   * Resumes the coroutine by executing all pending write operations stored in grouped_writes.
   *
   * @details This method processes each write operation by converting the stored value into a
   * hardware-compatible format and executes the write operation.
   */
  void TestBase::AwaitWrite::await_resume() noexcept {
    s_vpi_value val{};
    val.format = vpiVectorVal;

    // Scan through grouped_writes and perform write for each record
    for (const auto& pair : grouped_writes) {
      const std::string& key = pair.first;
      const unsigned int vecval_len =
        (parent.getNetLength(key) + 31) / 32; // number of 32-bit chunks required

      std::vector<s_vpi_vecval> write_vecval(vecval_len, {0, 0}); // Initialize all elements to {0, 0}

      if (pair.second.strValue.empty()) {
        // Handle numeric write
        unsigned long long int temp_value = pair.second.ullValue;

        // Split the value into 32-bit chunks and store them in write_vecval
        for (unsigned int i = 0; i < vecval_len; ++i) {
          write_vecval[i].aval = static_cast<PLI_INT32>(temp_value & 0xFFFFFFFF); // Extract 32 bits
          write_vecval[i].bval = 0; // no bval used here
          temp_value >>= 32; // next chunk
        }
      }
      else {
        // Handle string write
        const std::string& value = pair.second.strValue;

        // Fill write_vecval with bits from the string in reverse order
        size_t bit_index = 0;
        for (auto it = value.rbegin(); it != value.rend(); ++it, ++bit_index) {
          const size_t vecval_index = bit_index / 32;
          const size_t bit_position = bit_index % 32;
          const auto bit_mask = static_cast<PLI_INT32>(1u << bit_position);

          switch (*it) {
          case '1':
            write_vecval[vecval_index].aval |= bit_mask;
            break;
          case '0':
            // already 0
            break;
          case 'x':
            write_vecval[vecval_index].aval |= bit_mask; // 1
            write_vecval[vecval_index].bval |= bit_mask; // X
            break;
          case 'z':
            write_vecval[vecval_index].aval &= ~bit_mask; // 0
            write_vecval[vecval_index].bval |= bit_mask; // Z
            break;
          default:
            std::printf("[WARNING]\tInvalid binary character used in write(): %c\n", *it);
            break;
          }
        }
      }

      val.value.vector = write_vecval.data();
      vpi_put_value(parent.getNetHandle(key), &val, nullptr, pair.second.flag);
    }

    // Done operations, remove them from the list
    grouped_writes.clear();
    grouped_writes.rehash(0);

    // Unregister and free the callback only if it actually exists
    if (cb_handle != nullptr) {
      vpi_remove_cb(cb_handle);
      vpi_free_object(cb_handle);
      cb_handle = nullptr;
    }
  }

  /**
   * Adds a write operation to the grouped writes with the vpiNoDelay flag
   * The flag indicates that this is a regular write to the HDL net
   */
  void TestBase::AwaitWrite::write(const std::string& netStr,
                                   const unsigned long long int value) {
    t_write_value write_value{};
    write_value.flag = vpiNoDelay; // For regular write we use no delay flag
    write_value.ullValue = value;
    grouped_writes.insert(std::make_pair(netStr, write_value));
  }

  /**
   * Adds a write operation to the grouped writes with the vpiForceFlag flag
   * The flag indicates that this is a force on a net.
   */
  void TestBase::AwaitWrite::force(const std::string& netStr,
                                   const unsigned long long int value) {
    t_write_value write_value{};
    write_value.flag = vpiForceFlag; // Force net
    write_value.ullValue = value;
    grouped_writes.insert(std::make_pair(netStr, write_value));
  }

  /**
   * Releases a previously forced value on the specified net.
   */
  void TestBase::AwaitWrite::release(const std::string& netStr) {
    t_write_value write_value{};
    write_value.flag = vpiReleaseFlag; // release force
    write_value.ullValue = 0;
    grouped_writes.insert(std::make_pair(netStr, write_value));
  }

  /**
   * String write, vpiNoDelay.
   */
  void TestBase::AwaitWrite::write(const std::string& netStr,
                                   const std::string& valStr,
                                   const unsigned int base) {
    t_write_value write_value{};
    write_value.flag = vpiNoDelay; // regular write

    if (base == 16) {
      write_value.strValue = hex_to_bin(valStr);
    }
    else {
      write_value.strValue = valStr;
    }

    grouped_writes.insert(std::make_pair(netStr, write_value));
  }

  /**
   * String force, vpiForceFlag.
   */
  void TestBase::AwaitWrite::force(const std::string& netStr,
                                   const std::string& valStr,
                                   const unsigned int base) {
    t_write_value write_value{};
    write_value.flag = vpiForceFlag; // force net

    if (base == 16) {
      write_value.strValue = hex_to_bin(valStr);
    }
    else {
      write_value.strValue = valStr;
    }

    grouped_writes.insert(std::make_pair(netStr, write_value));
  }
} // namespace test
