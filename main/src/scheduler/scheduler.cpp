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
#include "scheduler.hpp"
#include <cstdio>
#include <cstdint>

namespace scheduler {
  PLI_INT32 write_callback(p_cb_data data) {
    // Retrieve the user data passed to the callback
    auto* callbackData = reinterpret_cast<SchedulerCallbackData*>(data->user_data);
    if (!callbackData) {
      return 0;
    }

    // Resume the coroutine
    callbackData->handle.resume();

    // Clean up the allocated memory
    delete callbackData;
    data->user_data = nullptr;

    return 0;
  }

  PLI_INT32 read_callback(p_cb_data data) {
    // Retrieve the user data passed to the callback
    auto* callbackData = reinterpret_cast<SchedulerCallbackData*>(data->user_data);
    if (!callbackData) {
      return 0;
    }

    // Resume the coroutine
    callbackData->handle.resume();

    // Clean up the allocated memory
    delete callbackData;
    data->user_data = nullptr;

    return 0;
  }

  /**
   * @brief Callback function triggered by a value change event on a monitored net.
   *
   * This is the NON-targeted variant: first change → resume coroutine.
   * The actual new value is not inspected here; the AwaitChange awaiter
   * will do the read in await_resume().
   */
  PLI_INT32 change_callback(p_cb_data data) {
    // Retrieve the user data passed to the callback
    auto* callbackData = reinterpret_cast<SchedulerCallbackData*>(data->user_data);
    if (!callbackData) {
      return 0;
    }

    // At this point, for Questa, data->value == &callbackData->vpi_value
    // and simulator has filled vpi_value for this event already.
    // We don't need it here; AwaitChange::await_resume() will read the net.

    // Resume the coroutine
    callbackData->handle.resume();

    // Clean up the allocated memory
    delete callbackData;
    data->user_data = nullptr;

    return 0;
  }

  /**
   * @brief Callback function triggered by a value change event on a monitored net.
   *
   * Targeted variant: only resumes coroutine once the changed value
   * equals cb_change_target_value.
   *
   * If the change does not match the monitored change, the coroutine is not resumed
   * and the callback remains registered. The simulator will invoke this callback
   * again on the next change.
   */
  PLI_INT32 change_callback_targeted(p_cb_data data) {
    // Retrieve the user data passed to the callback
    auto* callbackData = reinterpret_cast<SchedulerCallbackData*>(data->user_data);
    if (!callbackData) {
      return 0;
    }

    const unsigned int net_length = callbackData->cb_change_target_value_length;

    // For Questa: data->value points to the same s_vpi_value we set up
    // in AwaitChange::await_suspend (callbackData->vpi_value). We can either
    // trust that, or re-call vpi_get_value. Using simulator-filled value:
    s_vpi_value& read_val = callbackData->vpi_value;

    // Defensive: if someone ever registered with a different format,
    // make sure it is vector.
    if (read_val.format != vpiVectorVal) {
      read_val.format = vpiVectorVal;
      vpi_get_value(data->obj, &read_val);
    }

    bool match = false;

    if (net_length <= 32) {
      const uint32_t aval0 = static_cast<uint32_t>(read_val.value.vector[0].aval);
      match = (callbackData->cb_change_target_value == aval0);
    }
    else {
      // Combine the lowest two 32-bit chunks into a 64-bit value:
      // [vec[1].aval] = high, [vec[0].aval] = low
      uint64_t combined_value = static_cast<uint64_t>(read_val.value.vector[1].aval);
      combined_value = (combined_value << 32) |
        static_cast<uint32_t>(read_val.value.vector[0].aval);

      match = (callbackData->cb_change_target_value == combined_value);
    }

    if (match) {
      // Target reached: resume coroutine once, then free callbackData.
      callbackData->handle.resume();
      delete callbackData;
      data->user_data = nullptr;
    }
    // If not matched: do NOT delete callbackData, because this callback
    // will be called again on the next value change and still needs
    // that user_data pointer to remain valid.

    return 0;
  }
} // namespace scheduler
