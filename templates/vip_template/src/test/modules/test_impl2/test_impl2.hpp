#ifndef TEST_IMPL2_HPP
#define TEST_IMPL2_HPP

#include <rapidvpi/testbase/testbase.hpp>

namespace test {
  class Test; // Forward declaration of Test

  class TestImpl2 {
  public:
    using RunTask = TestBase::RunTask;
    explicit TestImpl2(Test& base);

    // User test coroutines
    RunTask run();
    RunTask clock_gen();

  private:
    Test& test;
  };
} // namespace test

#endif // TEST_IMPL2_HPP
