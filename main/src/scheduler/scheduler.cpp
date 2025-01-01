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

namespace scheduler {
    PLI_INT32 write_callback(p_cb_data data) {
        // Retrieve the user data passed to the callback
        auto* callbackData = reinterpret_cast<SchedulerCallbackData*>(data->user_data);

        // Resume the coroutine
        callbackData->handle.resume();

        // Clean up the allocated memory
        delete callbackData;

        return 0;
    }

    PLI_INT32 read_callback(p_cb_data data) {
        // Retrieve the user data passed to the callback
        auto* callbackData = reinterpret_cast<SchedulerCallbackData*>(data->user_data);

        // Resume the coroutine
        callbackData->handle.resume();

        // Clean up the allocated memory
        delete callbackData;

        return 0;
    }

    /**
     * @brief Callback function triggered by a value change event on a monitored net.
     *
     * This function resumes the coroutine associated with the event and cleans up the
     * allocated memory for the callback data. It is invoked when the monitored net's
     * value changes.
     *
     * @param data Pointer to the callback data structure.
     * @return Always returns 0 indicating successful execution.
     *
     * The user data contains an `SchedulerCallbackData` structure which holds the coroutine
     * handle to be resumed.
     */
    PLI_INT32 change_callback(p_cb_data data) {
        // Retrieve the user data passed to the callback
        auto* callbackData = reinterpret_cast<SchedulerCallbackData*>(data->user_data);

        // Resume the coroutine
        callbackData->handle.resume();

        // Clean up the allocated memory
        delete callbackData;

        return 0;
    }


    /**
     * @brief Callback function triggered by a value change event on a monitored net.
     *
     * This function is called when the value of a monitored net changes. It checks
     * if this change matches the targeted change value specified in the user data.
     * If the change matches, the respective coroutine is resumed.
     *
     * If the change does not math the monitored change, then coroutine is not resumed
     * and simulator will again call this callback on next change.
     *
     * @param data Pointer to the callback data structure.
     * @return Always returns 0 indicating successful execution.
     *
     * The user data contains a `SchedulerCallbackData` structure which holds the coroutine handle to be resumed.
     * The value of the monitored net is read and compared against the target value stored in the callback data.
     * If the value matches, the coroutine is resumed and the allocated memory for the callback data is cleaned up.
     */

    PLI_INT32 change_callback_targeted(p_cb_data data) {
        // Retrieve the user data passed to the callback
        auto* callbackData = reinterpret_cast<SchedulerCallbackData*>(data->user_data);
        const unsigned short int net_length = callbackData->cb_change_target_value_length;

        // read the value of the net monitored for a change
        s_vpi_value read_val;
        read_val.format = vpiVectorVal;
        vpi_get_value(data->obj, &read_val);

        // Determine the comparison based on the length
        if (net_length <= 32) {
            if (callbackData->cb_change_target_value == static_cast<uint32_t>(read_val.value.vector[0].aval)) {
                // Resume the coroutine
                callbackData->handle.resume();

                // Clean up the allocated memory
                delete callbackData;
            }
        } else {
            auto combined_value = static_cast<uint64_t>(read_val.value.vector[1].aval);
            combined_value = (combined_value << 32) | static_cast<uint32_t>(read_val.value.vector[0].aval);

            if (callbackData->cb_change_target_value == combined_value) {
                // Resume the coroutine
                callbackData->handle.resume();

                // Clean up the allocated memory
                delete callbackData;
            }
        }

        return 0;
    }





}
