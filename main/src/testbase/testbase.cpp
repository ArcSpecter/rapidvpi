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


#include <string>
#include <cstdio>
#include "testbase.hpp"

namespace test {
  void TestBase::addNet(const std::string& key, const unsigned int length) {
    const std::string full_name = dutName + "." + key;
    vpiHandle h = vpi_handle_by_name(const_cast<char*>(full_name.c_str()), nullptr);

    if (h == nullptr) {
      std::printf("[ERROR]\tvpi_handle_by_name failed for '%s'\n", full_name.c_str());
    }
    else {
      std::printf("[INFO]\tRegistered net '%s' (full: '%s'), handle=%p, len=%u\n",
                  key.c_str(), full_name.c_str(), static_cast<void*>(h), length);
    }

    t_netmap_value entry{};
    entry.vpi_handle = h;
    entry.length = length;
    netMap[key] = entry;
  }

  vpiHandle TestBase::getNetHandle(const std::string& key) {
    const auto it = netMap.find(key);
    if (it == netMap.end()) {
      std::printf("[ERROR]\tgetNetHandle: key '%s' not found in netMap\n", key.c_str());
      return nullptr;
    }
    if (it->second.vpi_handle == nullptr) {
      std::printf("[ERROR]\tgetNetHandle: key '%s' has NULL vpi_handle\n", key.c_str());
    }
    return it->second.vpi_handle;
  }

  unsigned int TestBase::getNetLength(const std::string& key) {
    const auto it = netMap.find(key);
    if (it == netMap.end()) {
      std::printf("[ERROR]\tgetNetLength: key '%s' not found in netMap\n", key.c_str());
      return 0;
    }
    return it->second.length;
  }
} // namespace test
