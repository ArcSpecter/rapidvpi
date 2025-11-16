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
#ifndef DUT_TOP_SCHEDULER_HPP
#define DUT_TOP_SCHEDULER_HPP

#include <coroutine>
#include <vpi_user.h>

namespace scheduler {

  struct SchedulerCallbackData {
    std::coroutine_handle<> handle{};             // coroutine to resume

    // For targeted cbValueChange
    unsigned long long cb_change_target_value{};  // target value
    unsigned int       cb_change_target_value_length{}; // bit-length of monitored signal

    // Persistent VPI time/value storage for this callback
    // These must remain valid while the callback is registered.
    s_vpi_time  time{};       // stays valid for cb_data.time
    s_vpi_value vpi_value{};  // stays valid for cb_data.value
  };

  PLI_INT32 write_callback(p_cb_data data);
  PLI_INT32 read_callback(p_cb_data data);
  PLI_INT32 change_callback(p_cb_data data);
  PLI_INT32 change_callback_targeted(p_cb_data data);

} // namespace scheduler

#endif // DUT_TOP_SCHEDULER_HPP
