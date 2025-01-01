//
// Created by r2com on 9/17/24.
//
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
