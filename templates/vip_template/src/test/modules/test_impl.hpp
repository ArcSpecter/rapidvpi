#ifndef TEST_IMPL_HPP
#define TEST_IMPL_HPP

#include <rapidvpi/testbase/testbase.hpp>

namespace test {
  class Test; // Forward declaration of Test

  class TestImpl {
  public:
    using RunTask = TestBase::RunTask;
    explicit TestImpl(Test& base, int value);

    // User test coroutines
    RunTask run3();
    RunTask run4();

  private:
    Test& test;
    int x; // some variable received during test object creation
  };
} // namespace test

#endif // TEST_IMPL_HPP
