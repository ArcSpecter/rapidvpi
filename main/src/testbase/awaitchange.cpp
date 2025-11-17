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
  double TestBase::AwaitChange::getTime() const {
    return static_cast<double>(rdTime) * parent.sim_time_unit / TimeUnitConversion<T>::factor;
  }

  double TestBase::AwaitChange::getTime() const {
    return getTime<ns>();
  }

  template double TestBase::AwaitChange::getTime<ps>() const;
  template double TestBase::AwaitChange::getTime<ns>() const;
  template double TestBase::AwaitChange::getTime<us>() const;
  template double TestBase::AwaitChange::getTime<ms>() const;

  void TestBase::AwaitChange::await_suspend(std::coroutine_handle<> h) {
    handle = h;

#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] AwaitChange::await_suspend enter, net='%s', targeted=%d, handle=%p\n",
                net.c_str(),
                static_cast<int>(change_is_targeted),
                static_cast<void*>(handle.address()));
#endif

    auto callbackData = std::make_unique<scheduler::SchedulerCallbackData>();
    callbackData->handle = h;

    vpiHandle net_handle = parent.getNetHandle(net);
#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] AwaitChange::await_suspend net_handle=%p\n",
                static_cast<void*>(net_handle));
#endif

    if (net_handle == nullptr) {
      std::printf("[ERROR]\tAwaitChange::await_suspend: net '%s' has NULL handle, "
                  "cannot register cbValueChange.\n",
                  net.c_str());
      return;
    }
    // ---- Persistent time + value storage for Questa (MUST be non-null) ----
    callbackData->time.type = vpiSimTime;
    callbackData->time.high = 0;
    callbackData->time.low = 0;

    callbackData->vpi_value.format = vpiIntVal;
    callbackData->vpi_value.value.integer = 0;

    // Prepare cb_data
    s_cb_data cb_data{};
    cb_data.reason = cbValueChange;
    cb_data.obj = net_handle;
    cb_data.time = &callbackData->time; // <- non-null now
    cb_data.value = &callbackData->vpi_value; // <- non-null (from previous fix)

    if (change_is_targeted) {
      callbackData->cb_change_target_value = change_target_value;
      callbackData->cb_change_target_value_length = parent.getNetLength(net);
      cb_data.cb_rtn = &scheduler::change_callback_targeted;
#ifdef RAPIDVPI_DEBUG
      std::printf("[DBG] AwaitChange::await_suspend: targeted change, target=%llu len=%u\n",
                  static_cast<unsigned long long>(change_target_value),
                  parent.getNetLength(net));
#endif
    }
    else {
      cb_data.cb_rtn = &scheduler::change_callback;
#ifdef RAPIDVPI_DEBUG
      std::printf("[DBG] AwaitChange::await_suspend: non-targeted change\n");
#endif
    }

    cb_data.user_data = reinterpret_cast<PLI_BYTE8*>(callbackData.get());

#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] AwaitChange::await_suspend: calling vpi_register_cb (cbValueChange)\n");
#endif
    vpiHandle cbH = vpi_register_cb(&cb_data);
    if (cbH == nullptr) {
      std::printf("[WARNING]\tCannot register VPI Callback. TestBase::AwaitChange:: %s for net '%s'\n",
                  __FUNCTION__, net.c_str());

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
      return; // unique_ptr auto-frees callbackData
    }

#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] AwaitChange::await_suspend: vpi_register_cb OK, cb_handle=%p\n",
                static_cast<void*>(cbH));
#endif

    callbackData->cb_handle = cbH;
    (void)callbackData.release(); // hand off lifetime to the callback
    cb_handle = cbH;
  }


  void TestBase::AwaitChange::await_resume() noexcept {
#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] AwaitChange::await_resume enter, net='%s'\n", net.c_str());
#endif

    // Get current simulation time
    const vpiHandle cbH = nullptr;
    s_vpi_time tim{};
    tim.type = vpiSimTime;
    vpi_get_time(cbH, &tim);
    rdTime = (static_cast<unsigned long long>(tim.high) << 32) |
      static_cast<unsigned long long>(tim.low);

#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] AwaitChange::await_resume: time=%llu\n",
                static_cast<unsigned long long>(rdTime));
#endif

    // Read the value being changed
    s_vpi_value read_val{};
    read_val.format = vpiVectorVal;
    vpi_get_value(parent.getNetHandle(net), &read_val);
    const unsigned short int net_length = parent.getNetLength(net);

#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] AwaitChange::await_resume: net_length=%u\n",
                static_cast<unsigned int>(net_length));
#endif

    // Clear the previous change value
    rd_change_value.strValue.clear();
    rd_change_value.uintValues.clear();

    const unsigned int vecval_len =
      (static_cast<unsigned int>(net_length) + 31) / 32; // number of 32-bit chunks required

    std::string final_strValue;
    final_strValue.reserve(vecval_len * 32);

    // numeric values: push in natural order (LS chunk to MS chunk)
    for (unsigned int i = 0; i < vecval_len; ++i) {
      rd_change_value.uintValues.push_back(
        static_cast<unsigned int>(read_val.value.vector[i].aval));
    }

    // string value: process from MS chunk down to LS chunk
    for (int i = static_cast<int>(vecval_len) - 1; i >= 0; --i) {
      const unsigned int avalue = read_val.value.vector[i].aval;
      const unsigned int bvalue = read_val.value.vector[i].bval;

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

    rd_change_value.strValue = std::move(final_strValue);

#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] AwaitChange::await_resume: strValue length=%zu\n",
                rd_change_value.strValue.size());
#endif

    // scheduler::change_callback(_targeted) already removed the cb
    // and deleted user_data on the firing edge.
    cb_handle = nullptr;
  }

  unsigned long long int TestBase::AwaitChange::getNum() {
    if (const size_t value_count = rd_change_value.uintValues.size(); value_count == 1) {
      const unsigned int single_value = rd_change_value.uintValues.back();
      rd_change_value.uintValues.pop_back();
      return single_value;
    }

    if (rd_change_value.uintValues.size() >= 2) {
      const unsigned int value_high = rd_change_value.uintValues.back();
      rd_change_value.uintValues.pop_back();
      const unsigned int value_low = rd_change_value.uintValues.back();
      rd_change_value.uintValues.pop_back();

      const unsigned long long result =
        (static_cast<unsigned long long int>(value_high) << 32) | value_low;
      return result;
    }

    std::printf("[WARNING]\tNo value available\n");
    return 0;
  }

  std::string TestBase::AwaitChange::getStr(const unsigned int base) {
    constexpr char EMPTY_STRING[] = "";

    if (!rd_change_value.strValue.empty()) {
      if (base == 16) {
        return bin_to_hex(rd_change_value.strValue);
      }
      return rd_change_value.strValue;
    }

    return EMPTY_STRING;
  }

  std::string TestBase::AwaitChange::getBinStr() {
    return getStr(2);
  }

  std::string TestBase::AwaitChange::getHexStr() {
    return getStr(16);
  }
} // namespace test
