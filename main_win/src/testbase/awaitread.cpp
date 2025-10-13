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
  void TestBase::AwaitRead::setDelay(const double delay) {
    constexpr double factor = TimeUnitConversion<T>::factor;
    double adjusted_delay = delay * factor / parent.sim_time_unit; // Mixed compile-time and runtime
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


  /**
   * Suspends the coroutine and registers a callback with the scheduler to resume it.
   *
   * @param h The coroutine handle to be suspended and later resumed.
   */
  void TestBase::AwaitRead::await_suspend(std::coroutine_handle<> h) {
    handle = h;

    // Allocate and prepare the callback data
    auto callbackData = std::make_unique<scheduler::SchedulerCallbackData>();
    callbackData->handle = h; // Store the coroutine handle

    // register scheduler callback here
    s_cb_data cb_data;
    s_vpi_time tim;

    tim.high = 0;
    tim.low = delay;
    tim.type = vpiSimTime;

    cb_data.reason = cbReadOnlySynch;
    cb_data.cb_rtn = &scheduler::read_callback;
    cb_data.obj = nullptr;
    cb_data.time = &tim;
    cb_data.value = nullptr;
    cb_data.user_data = reinterpret_cast<PLI_BYTE8*>(callbackData.release()); // Pass ownership


    if (vpiHandle cbH; (cbH = vpi_register_cb(&cb_data)) == nullptr)
      printf("[WARNING]\tCannot register VPI Callback: %s\n", __FUNCTION__);
    else
      this->cb_handle = cbH; // save handle so that we can unregister inside await_resume()
  }

  /**
   * Resumes the coroutine after a read operation and processes read values.
   *
   * This function retrieves values from the simulator for each grouped read operation.
   * It converts the retrieved values into both a string format and a vector of unsigned integers.
   * The string representation uses '1', '0', 'x', and 'z' to indicate the state of each bit.
   * The unsigned integer vector contains the values of the highest two 32-bit chunks.
   */
  void TestBase::AwaitRead::await_resume() noexcept {
    s_vpi_value read_val;
    read_val.format = vpiVectorVal;

    // These are for getting a current time reading
    const vpiHandle cbH = nullptr;
    s_vpi_time tim;
    tim.type = vpiSimTime;
    vpi_get_time(cbH, &tim);
    rdTime = (static_cast<unsigned long long>(tim.high) << 32) | static_cast<unsigned long long>(tim.low);

    for (auto& pair : grouped_reads) {
      vpi_get_value(parent.getNetHandle(pair.first), &read_val);

      const unsigned int vecval_len = (parent.getNetLength(pair.first) + 31) / 32; // number of 32-bit chunks required

      // Allocate enough space for the resulting string
      std::string final_strValue;
      final_strValue.reserve(vecval_len * 32);

      for (int i = static_cast<int>(vecval_len) - 1; i >= 0; --i) {
        // Start from the high end and go toward 0
        const unsigned int avalue = read_val.value.vector[i].aval;
        const unsigned int bvalue = read_val.value.vector[i].bval;

        // Only push back the first two 32-bit chunks
        if (vecval_len - i <= 2) {
          pair.second.uintValues.push_back(avalue);
        }

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
          final_strValue.push_back(bit_representation); // Append directly
        }
      }

      // Since we reversed the chunks but processed bits order correctly, add final string directly
      pair.second.strValue = std::move(final_strValue);
    }

    // Unregister the callback
    vpi_remove_cb(cb_handle);

    // Free the callback object
    vpi_free_object(cb_handle);

    // Clear the stored handle
    cb_handle = nullptr;
  }

  /**
   * Retrieves a string representation of a net's value from grouped reads.
   *
   * This function searches for the given string net name within the `grouped_reads` map.
   * If found, it returns the corresponding value as a string. If the `base`
   * parameter is 16, it converts the value to a hexadecimal string, otherwise it returns
   * the binary representation of a string. If not found,
   * it logs a warning and returns an empty string.
   *
   * @param netStr The name of the net to be searched in `grouped_reads`.
   * @param base The numerical base for the string representation of the net's value. Currently supports base 16 (hex).
   * @return The string representation of the net's value, or an empty string if the net is not found.
   */
  std::string TestBase::AwaitRead::getStr(const std::string& netStr, const unsigned int base) {
    constexpr char EMPTY_STRING[] = "";
    if (const auto result = grouped_reads.find(netStr); result != grouped_reads.end()) {
      if (base == 16) {
        return bin_to_hex(result->second.strValue);
      }
      return result->second.strValue;
    }

    printf("[WARNING]\tNo value for net: %s\n", netStr.c_str());
    return EMPTY_STRING;
  }

  // for binary version
  std::string TestBase::AwaitRead::getBinStr(const std::string& netStr) {
    return getStr(netStr, 2);
  }

  // for hex version
  std::string TestBase::AwaitRead::getHexStr(const std::string& netStr) {
    return getStr(netStr, 16);
  }

  /**
   * Retrieves a 64-bit unsigned integer from the grouped reads map.
   *
   * This function searches for the specified net string in the `grouped_reads` map.
   * If found, it checks the number of available 32-bit unsigned integer values.
   * - If there is only one 32-bit value, it returns it as a 64-bit integer.
   * - If there are two 32-bit values, it combines them to form a 64-bit integer,
   *   with the second value considered as the most significant bits.
   * If the net string is not found, it logs a warning message and returns 0.
   *
   * @param netStr The net string to be searched in the `grouped_reads` map.
   * @return A 64-bit unsigned integer extracted from the `grouped_reads` map or 0 if the net string is not found.
   */
  unsigned long long int TestBase::AwaitRead::getNum(const std::string& netStr) {
    if (const auto result = grouped_reads.find(netStr); result != grouped_reads.end()) {
      if (const size_t value_count = result->second.uintValues.size(); value_count == 1) {
        // Only one 32-bit value available
        unsigned int single_value = result->second.uintValues.back();
        result->second.uintValues.pop_back();
        return single_value;
      }
      // Get LS word
      unsigned int value_low = result->second.uintValues.back();
      result->second.uintValues.pop_back();
      // Get MS word
      unsigned int value_high = result->second.uintValues.back();
      result->second.uintValues.pop_back();
      return (static_cast<unsigned long long int>(value_high) << 32) + value_low;
    }

    printf("[WARNING]\tNo value for net: %s\n", netStr.c_str());
    return 0;
  }

  /**
   * Adds a new read operation to the grouped_reads map.
   *
   * This function initializes a read operation by creating an entry in the
   * `grouped_reads` map with the specified net string and a default read value.
   * This default value has an empty string for `strValue` and an empty vector
   * for `uintValues`.
   *
   * @param netStr The name of the net to be added to the `grouped_reads` map.
   */
  void TestBase::AwaitRead::read(const std::string& netStr) {
    constexpr char EMPTY_STRING[] = "";
    t_read_value readValue;
    readValue.uintValues = {};
    readValue.strValue = EMPTY_STRING;
    grouped_reads.insert(std::make_pair(netStr, readValue));
  }
}
