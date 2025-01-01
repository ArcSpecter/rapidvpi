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
    co_await test.getCoWrite(7.25); // Use the `test` reference

    auto awchange = test.getCoChange("clk", 1); // get next clk rising change
    co_await awchange;

    auto awaiter2 = test.getCoWrite(0); // write at that clk edge
    awaiter2.write("b", 0xc000000000);
    awaiter2.write("a", "111");
    co_await awaiter2;

    auto awRd = test.getCoRead(0);
    awRd.read("c"); // this will create placeholder of key,value pair for read "c"
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
