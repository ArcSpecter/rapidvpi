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
  // cbAfterDelay -> used by AwaitWrite
  PLI_INT32 write_callback(p_cb_data data) {
#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] write_callback entered: data=%p reason=%d obj=%p\n",
                static_cast<void*>(data),
                data ? data->reason : -1,
                data ? static_cast<void*>(data->obj) : nullptr);
#endif

    auto* callbackData =
      data
        ? reinterpret_cast<SchedulerCallbackData*>(data->user_data)
        : nullptr;

#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] write_callback: user_data=%p\n",
                static_cast<void*>(callbackData));
#endif

    std::coroutine_handle<> h{};
    if (callbackData) {
      h = callbackData->handle;
    }

#ifdef RAPIDVPI_DEBUG
    if (h) {
      std::printf("[DBG] write_callback: resuming coroutine %p\n",
                  static_cast<void*>(h.address()));
    }
    else {
      std::printf("[DBG] write_callback: NULL handle or user_data\n");
    }
#endif
    if (h) {
      h.resume();
    }

    // cbAfterDelay is one-shot. Simulator will drop the callback;
    // we only need to free our user_data now.
    if (callbackData) {
      delete callbackData;
      data->user_data = nullptr;
    }

    return 0;
  }

  // cbReadOnlySynch -> used by AwaitRead
  PLI_INT32 read_callback(p_cb_data data) {
#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] read_callback entered: data=%p reason=%d obj=%p\n",
                static_cast<void*>(data),
                data ? data->reason : -1,
                data ? static_cast<void*>(data->obj) : nullptr);
#endif

    auto* callbackData =
      data
        ? reinterpret_cast<SchedulerCallbackData*>(data->user_data)
        : nullptr;

#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] read_callback: user_data=%p\n",
                static_cast<void*>(callbackData));
#endif

    std::coroutine_handle<> h{};
    if (callbackData) {
      h = callbackData->handle;
    }

#ifdef RAPIDVPI_DEBUG
    if (h) {
      std::printf("[DBG] read_callback: resuming coroutine %p\n",
                  static_cast<void*>(h.address()));
    }
    else {
      std::printf("[DBG] read_callback: NULL handle or user_data\n");
    }
#endif
    if (h) {
      h.resume();
    }

    // cbReadOnlySynch is also one-shot; simulator removes callback.
    // Free our user_data.
    if (callbackData) {
      delete callbackData;
      data->user_data = nullptr;
    }

    return 0;
  }

  // cbValueChange, non-targeted -> used by AwaitChange (any change)
  PLI_INT32 change_callback(p_cb_data data) {
#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] change_callback entered: data=%p reason=%d obj=%p\n",
                static_cast<void*>(data),
                data ? data->reason : -1,
                data ? data->obj : nullptr);
#endif

    auto* callbackData =
      data
        ? reinterpret_cast<SchedulerCallbackData*>(data->user_data)
        : nullptr;

#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] change_callback: user_data=%p\n",
                static_cast<void*>(callbackData));
#endif

    std::coroutine_handle<> h{};
    if (callbackData) {
      h = callbackData->handle;
    }

#ifdef RAPIDVPI_DEBUG
    if (h) {
      std::printf("[DBG] change_callback: resuming coroutine %p\n",
                  static_cast<void*>(h.address()));
    }
    else {
      std::printf("[DBG] change_callback: NULL handle or user_data\n");
    }
#endif
    if (h) {
      h.resume();
    }

    // Non-targeted cbValueChange: we only care about first change.
    // Remove callback and free user_data so it cannot fire again.
    if (callbackData) {
      if (callbackData->cb_handle) {
        vpi_remove_cb(callbackData->cb_handle);
        callbackData->cb_handle = nullptr;
      }
      delete callbackData;
      data->user_data = nullptr;
    }

    return 0;
  }

  // cbValueChange, targeted -> used by AwaitChange (target bit/value)
  PLI_INT32 change_callback_targeted(p_cb_data data) {
#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] change_callback_targeted entered: data=%p reason=%d obj=%p\n",
                static_cast<void*>(data),
                data ? data->reason : -1,
                data ? data->obj : nullptr);
#endif

    auto* callbackData =
      data
        ? reinterpret_cast<SchedulerCallbackData*>(data->user_data)
        : nullptr;

#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] change_callback_targeted: user_data=%p\n",
                static_cast<void*>(callbackData));
#endif

    if (!callbackData) {
#ifdef RAPIDVPI_DEBUG
      std::printf("[DBG] change_callback_targeted: NULL callbackData, returning.\n");
#endif
      return 0;
    }

    const unsigned int net_length = callbackData->cb_change_target_value_length;
#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] change_callback_targeted: target_value=%llu, len=%u\n",
                static_cast<unsigned long long>(callbackData->cb_change_target_value),
                net_length);
#endif

    // Read current value of the watched net
    s_vpi_value read_val{};
    read_val.format = vpiVectorVal;
    vpi_get_value(data->obj, &read_val);

    bool match = false;
    unsigned long long cur_val = 0;

    if (net_length <= 32) {
      const auto aval0 =
        static_cast<std::uint32_t>(read_val.value.vector[0].aval);
      cur_val = static_cast<unsigned long long>(aval0);

#ifdef RAPIDVPI_DEBUG
      std::printf("[DBG] change_callback_targeted: current value (<=32) = 0x%llx\n",
                  cur_val);
#endif

      if (callbackData->cb_change_target_value == cur_val) {
        match = true;
      }
    }
    else {
      const auto aval0 =
        static_cast<std::uint32_t>(read_val.value.vector[0].aval);
      const auto aval1 =
        static_cast<std::uint32_t>(read_val.value.vector[1].aval);

      std::uint64_t combined =
        (static_cast<std::uint64_t>(aval1) << 32) |
        static_cast<std::uint64_t>(aval0);
      cur_val = combined;

#ifdef RAPIDVPI_DEBUG
      std::printf("[DBG] change_callback_targeted: current value (>32) = 0x%llx\n",
                  cur_val);
#endif

      if (callbackData->cb_change_target_value == combined) {
        match = true;
      }
    }

    if (!match) {
#ifdef RAPIDVPI_DEBUG
      std::printf("[DBG] change_callback_targeted: no match, keep callback.\n");
#endif
      // Keep waiting; do NOT free callbackData yet.
      return 0;
    }

#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] change_callback_targeted: MATCH, resuming coroutine %p\n",
                static_cast<void*>(callbackData->handle.address()));
#endif

    std::coroutine_handle<> h = callbackData->handle;

    // On match: remove callback and free user_data so it cannot fire again.
    if (callbackData->cb_handle) {
      vpi_remove_cb(callbackData->cb_handle);
      callbackData->cb_handle = nullptr;
    }
    delete callbackData;
    data->user_data = nullptr;

    if (h) {
      h.resume();
    }

    return 0;
  }
} // namespace scheduler
