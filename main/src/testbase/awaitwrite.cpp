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
#include <cstdio>

namespace test {
  template <TimeUnit T>
  void TestBase::AwaitWrite::setDelay(const double delay) {
    constexpr double factor = TimeUnitConversion<T>::factor;
    double adjusted_delay = delay * factor / parent.sim_time_unit;
    this->delay = static_cast<unsigned long long int>(adjusted_delay);
  }

  void TestBase::AwaitWrite::setDelay(const double delay) {
    setDelay<ns>(delay);
  }

  template void TestBase::AwaitWrite::setDelay<ps>(const double);
  template void TestBase::AwaitWrite::setDelay<ns>(const double);
  template void TestBase::AwaitWrite::setDelay<us>(const double);
  template void TestBase::AwaitWrite::setDelay<ms>(const double);

  void TestBase::AwaitWrite::await_suspend(std::coroutine_handle<> h) {
    handle = h;

    std::printf("[DBG] AwaitWrite::await_suspend enter, delay=%llu, handle=%p\n",
                static_cast<unsigned long long>(delay),
                static_cast<void*>(handle.address()));

    auto callbackData = std::make_unique<scheduler::SchedulerCallbackData>();
    callbackData->handle = h;

    callbackData->time.type = vpiSimTime;
    callbackData->time.high = 0;
    callbackData->time.low = delay;

    s_cb_data cb_data{};
    cb_data.reason = cbAfterDelay;
    cb_data.cb_rtn = &scheduler::write_callback;
    cb_data.obj = nullptr;
    cb_data.time = &callbackData->time; // persistent
    cb_data.value = nullptr; // allowed for cbAfterDelay

    cb_data.user_data = reinterpret_cast<PLI_BYTE8*>(callbackData.get());

    std::printf("[DBG] AwaitWrite::await_suspend: calling vpi_register_cb (cbAfterDelay)\n");
    vpiHandle cbH = vpi_register_cb(&cb_data);
    if (cbH == nullptr) {
      std::printf("[WARNING]\tCannot register VPI Callback. TestBase::AwaitWrite:: %s\n",
                  __FUNCTION__);

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
      return;
    }

    std::printf("[DBG] AwaitWrite::await_suspend: vpi_register_cb OK, cb_handle=%p\n",
                static_cast<void*>(cbH));

    (void)callbackData.release();
    cb_handle = cbH;
  }

  void TestBase::AwaitWrite::await_resume() noexcept {
    std::printf("[DBG] AwaitWrite::await_resume enter, grouped_writes=%zu\n",
                grouped_writes.size());

    s_vpi_value val{};
    val.format = vpiVectorVal;

    // Scan through grouped_writes and perform write for each record
    for (const auto& pair : grouped_writes) {
      const std::string& key = pair.first;
      const unsigned int vecval_len =
        (parent.getNetLength(key) + 31) / 32; // number of 32-bit chunks required

      std::vector<s_vpi_vecval> write_vecval(vecval_len, {0, 0}); // Initialize all elements to {0, 0}

      std::printf("[DBG] AwaitWrite::await_resume: net='%s', len=%u, flag=%d\n",
                  key.c_str(), parent.getNetLength(key), pair.second.flag);

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

      std::printf("[DBG] AwaitWrite::await_resume: calling vpi_put_value on '%s'\n",
                  key.c_str());
      vpi_put_value(parent.getNetHandle(key), &val, nullptr, pair.second.flag);
    }

    // Done operations, remove them from the list
    grouped_writes.clear();
    grouped_writes.rehash(0);

    // *** IMPORTANT ***:
    // Do NOT call vpi_remove_cb or vpi_free_object here for cbAfterDelay.
    // Questa removes one-shot callbacks itself when they fire.
    // Leaving this empty avoids double-free / dangling-handle UB.
  }


  void TestBase::AwaitWrite::write(const std::string& netStr,
                                   const unsigned long long int value) {
    t_write_value write_value{};
    write_value.flag = vpiNoDelay;
    write_value.ullValue = value;
    grouped_writes.insert(std::make_pair(netStr, write_value));
  }

  void TestBase::AwaitWrite::force(const std::string& netStr,
                                   const unsigned long long int value) {
    t_write_value write_value{};
    write_value.flag = vpiForceFlag;
    write_value.ullValue = value;
    grouped_writes.insert(std::make_pair(netStr, write_value));
  }

  void TestBase::AwaitWrite::release(const std::string& netStr) {
    t_write_value write_value{};
    write_value.flag = vpiReleaseFlag;
    write_value.ullValue = 0;
    grouped_writes.insert(std::make_pair(netStr, write_value));
  }

  void TestBase::AwaitWrite::write(const std::string& netStr,
                                   const std::string& valStr,
                                   const unsigned int base) {
    t_write_value write_value{};
    write_value.flag = vpiNoDelay;

    if (base == 16) {
      write_value.strValue = hex_to_bin(valStr);
    }
    else {
      write_value.strValue = valStr;
    }

    grouped_writes.insert(std::make_pair(netStr, write_value));
  }

  void TestBase::AwaitWrite::force(const std::string& netStr,
                                   const std::string& valStr,
                                   const unsigned int base) {
    t_write_value write_value{};
    write_value.flag = vpiForceFlag;

    if (base == 16) {
      write_value.strValue = hex_to_bin(valStr);
    }
    else {
      write_value.strValue = valStr;
    }

    grouped_writes.insert(std::make_pair(netStr, write_value));
  }
} // namespace test
