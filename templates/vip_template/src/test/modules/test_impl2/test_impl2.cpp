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


#include "test_impl2.hpp"
#include "test.hpp"

namespace test {
  TestImpl2::TestImpl2(Test& test) : test(test) {
    test.registerTest("run", [this]() { return this->run().handle; });
    test.registerTest("clock_gen", [this]() { return this->clock_gen().handle; });
  }

  TestImpl2::RunTask TestImpl2::run() {
    auto awaiter = test.getCoWrite(0); // Use the `test` reference
    awaiter.write("clk", 0);
    awaiter.write("a", 0);
    awaiter.write("b", 0);
    awaiter.write("rst", 0);
    co_await awaiter;

    awaiter.setDelay(10);
    awaiter.write("rst", 1);
    co_await awaiter;

    auto await_force = test.getCoWrite(12);
    await_force.force("c", 0xabcd);
    co_await await_force;

    auto await_release = test.getCoWrite(2);
    await_release.release("c");
    co_await await_release;

    co_return;
  }

  TestImpl2::RunTask TestImpl2::clock_gen() {
    auto awaiter = test.getCoWrite(5); // Use the `test` reference

    for (int i = 0; i < 4; i++) {
      awaiter.write("clk", 1);
      co_await awaiter;
      awaiter.write("clk", 0);
      co_await awaiter;
    }

    co_return;
  }
} // namespace test
