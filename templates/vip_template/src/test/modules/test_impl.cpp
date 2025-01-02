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

#include "test_impl.hpp"
#include "test.hpp"  // Include full definition of Test here

namespace test {
  TestImpl::TestImpl(Test& test, int value) : test(test), x(value) {
    test.registerTest("run3", [this]() { return this->run3().handle; });
    test.registerTest("run4", [this]() { return this->run4().handle; });
  }

  TestImpl::RunTask TestImpl::run3() {
    auto awchange = test.getCoChange("c"); // Use the `test` reference
    co_await awchange;
    co_await awchange;

    printf("Awaited Numeric value for the 'c' is : %llx\n", awchange.getNum());
    printf("Awaited Hex String value for the 'c' is : %s\n", awchange.getHexStr().c_str());
    printf("Awaited Bin String value for the 'c' is : %s\n", awchange.getBinStr().c_str());

    co_return;
  }

  TestImpl::RunTask TestImpl::run4() {
    co_await test.getCoWrite(7.25);

    auto awchange = test.getCoChange("clk", 1); // get next clk rising change
    co_await awchange;

    auto awaiter2 = test.getCoWrite(0); // write at that clk edge
    awaiter2.write("b", 0xc000000000);
    awaiter2.write("a", "111");
    co_await awaiter2;

    auto awRd = test.getCoRead(0);
    awRd.read("c");
    co_await awRd;
    std::string value_hex = awRd.getHexStr("c"); // Expecting hex output
    std::string value_bin = awRd.getBinStr("c"); // Binary output
    printf("numeric value of 'c' is: %llx\n", awRd.getNum("c"));
    printf("hex string is: %s\n", value_hex.c_str());
    printf("bin string is: %s\n", value_bin.c_str());

    test.some_func();
    printf("value received during  test creation: %d\n", x);

    co_return;
  }
} // namespace test
