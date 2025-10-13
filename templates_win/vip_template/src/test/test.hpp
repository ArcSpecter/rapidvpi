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


#ifndef DUT_TOP_TEST_HPP
#define DUT_TOP_TEST_HPP

#include <rapidvpi/testbase/testbase.hpp>
#include <memory>
#include "./modules/test_impl.hpp"
#include "./modules/test_impl2/test_impl2.hpp"

// Declare the factory registration function
extern "C" void userRegisterFactory();

namespace test {
  class Test : public TestBase {
  public:
    Test() : impl(*this, 42), impl2(*this) {dutName = "dut_top";}

    void initDutName();
    void initNets() override;

    // Common test functions
    void some_func() { printf("some_func() called\n"); }

  private:
    std::string dutName;
    TestImpl impl;
    TestImpl2 impl2;
  };
} // namespace test

#endif // DUT_TOP_TEST_HPP
