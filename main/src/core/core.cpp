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

#include "core.hpp"

namespace core {
  static std::unique_ptr<test::TestBase> dut;

  static std::function<std::unique_ptr<test::TestBase>()> createTestInstance;

  void registerTestFactory(std::function<std::unique_ptr<test::TestBase>()> factory) {
    createTestInstance = std::move(factory);
  }

  PLI_INT32 sim_init(p_cb_data data) {
    if (!createTestInstance) {
      printf("[ERROR] Test instance factory is not registered.\n");
      return -1; // Return error
    }

    dut = createTestInstance();

    // Obtain simulation time unit and save it into the DUT object
    const double SimTimeUnit = std::pow(10, vpi_get(vpiTimePrecision, nullptr));
    dut->updateSimTimeUnit(SimTimeUnit);

    // Initialize all the Nets which user entered in Test class
    dut->initNets();

    auto& testManager = test::TestManager::getInstance();
#ifdef RAPIDVPI_DEBUG
    std::printf("[DBG] Test manager about to start...\n");
#endif

    for (const auto& [name, testFunctions] : testManager.getTests()) {
      for (const auto& testFunction : testFunctions) {
        auto handle = testFunction();
        dut->test_handles.push_back(handle);
      }
    }

    return 0;
  }

  void register_cb() {
    // Invoke the user-defined factory registration function
    userRegisterFactory();

    s_cb_data cb_data{};
    cb_data.reason = cbStartOfSimulation;
    cb_data.cb_rtn = &sim_init;
    cb_data.obj = nullptr;
    cb_data.time = nullptr;
    cb_data.value = nullptr;
    cb_data.user_data = nullptr;

    if (vpiHandle cbH; (cbH = vpi_register_cb(&cb_data)) == nullptr)
      printf("[WARNING] Cannot register VPI Callback: %s\n", __FUNCTION__);
    else
      vpi_free_object(cbH);
  }
}
