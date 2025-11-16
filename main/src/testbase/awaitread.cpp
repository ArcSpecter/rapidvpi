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
  // ============================================================
  // Delay helpers
  // ============================================================
  template <TimeUnit T>
  void TestBase::AwaitRead::setDelay(const double delay) {
    constexpr double factor = TimeUnitConversion<T>::factor;
    double adjusted_delay = delay * factor / parent.sim_time_unit;
    this->delay = static_cast<unsigned long long int>(adjusted_delay);
  }

  void TestBase::AwaitRead::setDelay(const double delay) {
    setDelay<ns>(delay);
  }

  template void TestBase::AwaitRead::setDelay<ps>(const double);
  template void TestBase::AwaitRead::setDelay<ns>(const double);
  template void TestBase::AwaitRead::setDelay<us>(const double);
  template void TestBase::AwaitRead::setDelay<ms>(const double);

  template <TimeUnit T>
  double TestBase::AwaitRead::getTime() const {
    return static_cast<double>(rdTime) * parent.sim_time_unit / TimeUnitConversion<T>::factor;
  }

  double TestBase::AwaitRead::getTime() const {
    return getTime<ns>();
  }

  template double TestBase::AwaitRead::getTime<ps>() const;
  template double TestBase::AwaitRead::getTime<ns>() const;
  template double TestBase::AwaitRead::getTime<us>() const;
  template double TestBase::AwaitRead::getTime<ms>() const;

  // ============================================================
  // Core awaitable
  // ============================================================
  void TestBase::AwaitRead::await_suspend(std::coroutine_handle<> h) {
    handle = h;

    std::printf("[DBG] AwaitRead::await_suspend enter, delay=%llu, handle=%p\n",
                static_cast<unsigned long long>(delay),
                static_cast<void*>(handle.address()));

    // Allocate callback data on heap (owned until callback fires)
    auto callbackData = std::make_unique<scheduler::SchedulerCallbackData>();
    callbackData->handle = h;

    // Persistent time for Questa (MUST be non-null for cbReadOnlySynch)
    callbackData->time.type = vpiSimTime;
    callbackData->time.high = 0;
    callbackData->time.low = static_cast<PLI_INT32>(delay);

    // We don't need .vpi_value for cbReadOnlySynch, so leave it default-init

    s_cb_data cb_data{};
    cb_data.reason = cbReadOnlySynch;
    cb_data.cb_rtn = &scheduler::read_callback;
    cb_data.obj = nullptr;
    cb_data.time = &callbackData->time; // <<< IMPORTANT: non-null persistent time
    cb_data.value = nullptr; // allowed for cbReadOnlySynch
    cb_data.user_data =
      reinterpret_cast<PLI_BYTE8*>(callbackData.get());

    std::printf("[DBG] AwaitRead::await_suspend: calling vpi_register_cb (cbReadOnlySynch)\n");
    vpiHandle cbH = vpi_register_cb(&cb_data);
    if (cbH == nullptr) {
      std::printf("[WARNING]\tCannot register VPI Callback. TestBase::AwaitRead:: %s\n",
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
      // unique_ptr auto-frees callbackData
      return;
    }

    std::printf("[DBG] AwaitRead::await_suspend: vpi_register_cb OK, cb_handle=%p\n",
                static_cast<void*>(cbH));

    // Hand ownership of callbackData to the simulator callback
    (void)callbackData.release();
    cb_handle = cbH;
  }

  void TestBase::AwaitRead::await_resume() noexcept {
    std::printf("[DBG] AwaitRead::await_resume enter\n");

    // sample current simulation time
    const vpiHandle cbH = nullptr;
    s_vpi_time tim{};
    tim.type = vpiSimTime;
    vpi_get_time(cbH, &tim);
    rdTime = (static_cast<unsigned long long>(tim.high) << 32) |
      static_cast<unsigned long long>(tim.low);

    std::printf("[DBG] AwaitRead::await_resume: time=%llu, num_grouped_reads=%zu\n",
                rdTime, grouped_reads.size());

    s_vpi_value read_val{};
    read_val.format = vpiVectorVal;

    for (auto& pair : grouped_reads) {
      const std::string& netStr = pair.first;
      std::printf("[DBG] AwaitRead::await_resume: reading net '%s'\n", netStr.c_str());

      vpi_get_value(parent.getNetHandle(netStr), &read_val);

      const unsigned int vecval_len =
        (parent.getNetLength(netStr) + 31) / 32; // number of 32-bit chunks required

      std::string final_strValue;
      final_strValue.reserve(vecval_len * 32);

      // Build numeric and string versions
      for (int i = static_cast<int>(vecval_len) - 1; i >= 0; --i) {
        const unsigned int avalue = read_val.value.vector[i].aval;
        const unsigned int bvalue = read_val.value.vector[i].bval;

        // Only keep up to 64 bits numerically (2 x 32-bit chunks)
        if (vecval_len - static_cast<unsigned int>(i) <= 2) {
          pair.second.uintValues.push_back(avalue);
        }

        for (int j = 31; j >= 0; --j) {
          char bit_representation;
          const bool a_bit = (avalue & (1u << j)) != 0;
          const bool b_bit = (bvalue & (1u << j)) != 0;

          if (b_bit) {
            bit_representation = a_bit ? 'x' : 'z';
          }
          else {
            bit_representation = a_bit ? '1' : '0';
          }
          final_strValue.push_back(bit_representation);
        }
      }

      pair.second.strValue = std::move(final_strValue);
      std::printf("[DBG] AwaitRead::await_resume: net '%s' strValue length=%zu\n",
                  netStr.c_str(), pair.second.strValue.length());
    }

    // NOTE:
    //  - cbReadOnlySynch is one-shot; Questa removes the callback after firing.
    //  - Our scheduler::read_callback deletes SchedulerCallbackData.
    //  - Here we just clear local book-keeping if needed; no vpi_remove_cb().
  }

  // ============================================================
  // Value getters
  // ============================================================
  std::string TestBase::AwaitRead::getStr(const std::string& netStr,
                                          const unsigned int base) {
    constexpr char EMPTY_STRING[] = "";
    if (const auto result = grouped_reads.find(netStr); result != grouped_reads.end()) {
      if (base == 16) {
        return bin_to_hex(result->second.strValue);
      }
      return result->second.strValue;
    }

    std::printf("[WARNING]\tNo value for net: %s\n", netStr.c_str());
    return EMPTY_STRING;
  }

  std::string TestBase::AwaitRead::getBinStr(const std::string& netStr) {
    return getStr(netStr, 2);
  }

  std::string TestBase::AwaitRead::getHexStr(const std::string& netStr) {
    return getStr(netStr, 16);
  }

  unsigned long long int TestBase::AwaitRead::getNum(const std::string& netStr) {
    if (const auto result = grouped_reads.find(netStr); result != grouped_reads.end()) {
      if (const size_t value_count = result->second.uintValues.size(); value_count == 1) {
        unsigned int single_value = result->second.uintValues.back();
        result->second.uintValues.pop_back();
        return single_value;
      }

      if (result->second.uintValues.size() >= 2) {
        unsigned int value_low = result->second.uintValues.back();
        result->second.uintValues.pop_back();
        unsigned int value_high = result->second.uintValues.back();
        result->second.uintValues.pop_back();
        return (static_cast<unsigned long long int>(value_high) << 32) | value_low;
      }
    }

    std::printf("[WARNING]\tNo value for net: %s\n", netStr.c_str());
    return 0;
  }

  void TestBase::AwaitRead::read(const std::string& netStr) {
    constexpr char EMPTY_STRING[] = "";
    t_read_value readValue;
    readValue.uintValues = {};
    readValue.strValue = EMPTY_STRING;
    grouped_reads.insert(std::make_pair(netStr, readValue));
  }
} // namespace test
