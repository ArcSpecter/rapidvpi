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

  // User coroutines
  TestImpl2::RunUserTask TestImpl::clock(const int n, const int edge) const {
    for (int i = 0; i < n; ++i) {
      // Wait for the clk signal to change to the given edge value
      co_await test.getCoChange("clk", edge);
    }
    co_return;
  }

  TestImpl2::RunUserTask TestImpl::delay_ns(const double delay) const {
    co_await test.getCoWrite(delay);
    co_return;
  }

  TestImpl::RunTask TestImpl::run3() {
    // Optional: consume the first boring change (X->0, reset, whatever)
    {
      auto first = test.getCoChange("c");
      co_await first;
    }

    // Now wait for the next change with a *fresh* awaiter
    auto second = test.getCoChange("c");
    co_await second;

    printf("Awaited Numeric value for the 'c' is : %llx\n", second.getNum());
    printf("Awaited Hex String value for the 'c' is : %s\n", second.getHexStr().c_str());
    printf("Awaited Bin String value for the 'c' is : %s\n", second.getBinStr().c_str());

    co_return;
  }


  TestImpl::RunTask TestImpl::run4() {
    co_await test.getCoWrite(7.25);

    auto awchange = test.getCoChange("clk", 1); // get next clk rising change
    co_await awchange;
    double aw_change_time = awchange.getTime<ns>();
    printf("Read time at clk=1 change is: %f ns\n", aw_change_time);

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
    double read_time = awRd.getTime<ns>();
    printf("Read time is: %f ns\n", read_time);

    // Here goes example of user coroutines
    co_await delay_ns(3.3);

    // Now let's read the current time
    auto tread = test.getCoRead(0);
    co_await tread;
    double tread_val = tread.getTime<ns>();
    printf("Read time after delay: %f ns\n", tread_val);

    co_await clock(); // Wait just next rising edge

    // Let's read time again
    auto tread2 = test.getCoRead(0);
    co_await tread2;
    double tread_val2 = tread2.getTime<ns>();
    printf("Read time after next clock: %f ns\n", tread_val2);

    // Example of accessing test function in upper Test class
    test.some_func();
    printf("value received during  test creation: %d\n", x);


    co_return;
  }
} // namespace test
