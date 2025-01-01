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

#include "testbase.hpp"


namespace test {
  /**
   * @brief Adds a net to the netMap with a specified key and length.
   *
   * This function initializes the vpi_handle for a given net by constructing the
   * full hierarchical name from the dutName and the provided key. It also sets
   * the length of the net in the netMap.
   *
   * @param key The name of the net to add.
   * @param length The length of the net in bits.
   */
  void TestBase::addNet(const std::string& key, const unsigned int length) {
    netMap[key].vpi_handle = vpi_handle_by_name((dutName + "." + key).c_str(), nullptr);
    netMap[key].length = length;
  }

  /**
   * @brief Retrieves the vpi_handle for a given net.
   *
   * This function looks up the specified net in the netMap and returns its corresponding vpi_handle.
   *
   * @param key The name of the net whose vpi_handle is to be retrieved.
   * @return The vpi_handle associated with the specified net.
   */
  vpiHandle TestBase::getNetHandle(const std::string& key) {
    return netMap[key].vpi_handle;
  }

  /**
   * @brief Retrieves the length of a specified net in bits.
   *
   * This function looks up the specified net in the netMap and returns its length.
   *
   * @param key The name of the net whose length is to be retrieved.
   * @return The length of the net in bits.
   */
  unsigned int TestBase::getNetLength(const std::string& key) {
    return netMap[key].length;
  }


}
