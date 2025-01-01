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
