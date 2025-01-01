# RapidVPI
![alt text](docs/media/rapidvpi_logo.png)

Blazingly fast, modern C++ API using coroutines for efficient RTL verification and co-simulation via the VPI interface.


### Introduction

The RapidVPI API allows you to write modern C++ code for verification and co-simulation of digital RTL HDL designs using any simulator which supports VPI interface. Currently, as of now this library was tested with Iverilog. The API abstracts many of the VPI related boilerplate and tedious mechanisms and implements all the necessary signal driving and reading via the convenient coroutine mechanism native to C++ since version 20. User just creates the coroutine awaitable object, adds operations to it such as read, write and fires it up with co_await statement.

### Why RapidVPI?
The motivation for the development of this library was the ability to use all the flexible and advanced features of modern C++ during the RTL verification or co-simulation since verification itself is a software task and should use the proper software design tool such as C++; SystemVerilog is not a tool which is as powerful as C++ when it comes to modeling some advanced complex system. Additionally, SystemVerilog running with all of its features requires a very expensive license for the EDA tools, such a basic feature as randomization of variables in a class for example is not supported even in a lower tier paid simulators. The VPI interface on the other hand is supported by most lowest tier packaged commercial simulators, and of course it is supported by Iverilog which is a free and fast tool for Verilog simulation.

### What does writing RapidVPI code look like?
```c++
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
  ```

