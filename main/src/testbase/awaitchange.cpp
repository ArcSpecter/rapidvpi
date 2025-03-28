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

  /**
   * This method suspends the current coroutine by registering a callback that will resume
   * it when a value change occurs on a monitored net in a simulation environment.
   *
   * @param h The coroutine handle that needs to be resumed upon the value change event.
   */
  void TestBase::AwaitChange::await_suspend(std::coroutine_handle<> h) {
    handle = h;

    // Allocate and prepare the callback data
    auto callbackData = std::make_unique<scheduler::SchedulerCallbackData>();
    callbackData->handle = h; // Store the coroutine handle

    // register scheduler callback here
    s_cb_data cb_data;
    s_vpi_time tim;

    tim.high = 0;
    tim.low = 0;
    tim.type = vpiSimTime;

    cb_data.reason = cbValueChange;
    if (change_is_targeted == true) {
      // we looking for change to specific value
      callbackData->cb_change_target_value = change_target_value;
      callbackData->cb_change_target_value_length = parent.getNetLength(net);
      cb_data.cb_rtn = &scheduler::change_callback_targeted;
    }
    else {
      cb_data.cb_rtn = &scheduler::change_callback;
    }
    cb_data.obj = parent.getNetHandle(net);
    cb_data.time = &tim;
    cb_data.value = nullptr;
    cb_data.user_data = reinterpret_cast<PLI_BYTE8*>(callbackData.release()); // Pass ownership


    if (vpiHandle cbH; (cbH = vpi_register_cb(&cb_data)) == nullptr)
      printf("[WARNING]\tCannot register VPI Callback: %s\n", __FUNCTION__);
    else {
      this->cb_handle = cbH; // save handle so that we can unregister inside await_resume()
    }
  }

  /**
   * Resume the suspended coroutine and process the value change on a monitored net.
   *
   * This method gets the new value of the monitored net, processes it to update
   * the internal `rd_change_value` with the respective string and numeric representation
   * of the net's vector value, and then cleans up by unregistering and freeing the
   * callback object used for monitoring the value change.
   */
  void TestBase::AwaitChange::await_resume() noexcept {
    // These are for getting a current time reading
    const vpiHandle cbH = nullptr;
    s_vpi_time tim;
    tim.type = vpiSimTime;
    vpi_get_time(cbH, &tim);
    rdTime = (static_cast<unsigned long long>(tim.high) << 32) | static_cast<unsigned long long>(tim.low);

    // Read the value being changed
    s_vpi_value read_val;
    read_val.format = vpiVectorVal;
    vpi_get_value(parent.getNetHandle(net), &read_val);
    const unsigned short int net_length = parent.getNetLength(net);

    // Clear the previous change value
    rd_change_value.strValue.clear();
    rd_change_value.uintValues.clear();

    const unsigned int vecval_len = (net_length + 31) / 32; // number of 32-bit chunks required

    // Allocate enough space for the resulting string
    std::string final_strValue;
    final_strValue.reserve(vecval_len * 32);

    // Iterate from least significant chunk to most significant chunk for uintValues
    // But for string value, iterate from most significant to least significant for correct bit order.
    for (int i = 0; i < static_cast<int>(vecval_len); ++i) {
      // Append the numeric values correctly
      rd_change_value.uintValues.push_back(read_val.value.vector[i].aval);
    }

    for (int i = static_cast<int>(vecval_len) - 1; i >= 0; --i) {
      const unsigned int avalue = read_val.value.vector[i].aval;
      const unsigned int bvalue = read_val.value.vector[i].bval;

      // Process bits from MSB to LSB for each 32-bit word
      for (int j = 31; j >= 0; --j) {
        char bit_representation;
        const bool a_bit = (avalue & (1u << j)) != 0;
        const bool b_bit = (bvalue & (1u << j)) != 0;

        if (b_bit) {
          if (a_bit) {
            bit_representation = 'x';
          }
          else {
            bit_representation = 'z';
          }
        }
        else {
          bit_representation = a_bit ? '1' : '0';
        }
        final_strValue.push_back(bit_representation);
      }
    }

    // Assign the properly formatted string value
    rd_change_value.strValue = std::move(final_strValue);

    // Unregister the callback
    vpi_remove_cb(cb_handle);

    // Free the callback object
    vpi_free_object(cb_handle);

    // Clear the stored handle
    cb_handle = nullptr;
  }

  /**
   * Retrieves a single 64-bit unsigned integer value from the stored vector of 32-bit values.
   *
   * If there is only one 32-bit value available, it will be returned as a 32-bit value.
   * If there are at least two 32-bit values available, they will be combined into a 64-bit value,
   * with the most significant word first.
   * If no values are available, a warning will be printed and zero will be returned.
   *
   * @return A 64-bit unsigned integer value formed from the available 32-bit values.
   */
  unsigned long long int TestBase::AwaitChange::getNum() {
    if (const size_t value_count = rd_change_value.uintValues.size(); value_count == 1) {
      // Only one 32-bit value available
      const unsigned int single_value = rd_change_value.uintValues.back();
      rd_change_value.uintValues.pop_back();
      return single_value;
    }

    if (rd_change_value.uintValues.size() >= 2) {
      // Get MS word first
      const unsigned int value_high = rd_change_value.uintValues.back();
      rd_change_value.uintValues.pop_back();
      // Get LS word next
      const unsigned int value_low = rd_change_value.uintValues.back();
      rd_change_value.uintValues.pop_back();

      const unsigned long long result = (static_cast<unsigned long long int>(value_high) << 32) | value_low;
      return result;
    }

    printf("[WARNING]\tNo value available\n");
    return 0;
  }

  /**
   * Retrieves the string representation of the monitored net's value.
   *
   * This method checks if the string value of the monitored net is not empty. If the
   * `base` parameter is 16, the string value is converted to hexadecimal representation
   * and returned. Otherwise, the binary string value is returned. If the string value
   * is empty, an empty string is returned.
   *
   * @param base The numerical base (e.g., 16 for hexadecimal) used to format the return value.
   * @return The string representation of the net's value, formatted according to the given base.
   */
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

  // for bin string
  std::string TestBase::AwaitChange::getBinStr() {
    return getStr(2);
  }

  // for hex string
  std::string TestBase::AwaitChange::getHexStr() {
    return getStr(16);
  }

}
