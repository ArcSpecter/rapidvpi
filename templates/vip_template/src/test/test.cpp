#include "test.hpp"
#include "core.hpp"

extern "C" void userRegisterFactory() {
  core::registerTestFactory([]()
  {
    return std::make_unique<test::Test>();
  });
}

void test::Test::initDutName() {
  // User-defined logic to initialize the DUT name
  setDutName(dutName); // Set the name for the DUT
}

void test::Test::initNets() {
  initDutName(); // Ensure DUT name is initialized before adding nets
  addNet("clk", 1);
  addNet("rst", 1);
  addNet("a", 40);
  addNet("b", 40);
  addNet("c", 40);
};
