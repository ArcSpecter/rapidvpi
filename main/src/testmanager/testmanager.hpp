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


#ifndef TEST_MANAGER_H
#define TEST_MANAGER_H

#include <unordered_map>
#include <coroutine>
#include <functional>

namespace test {

    // This class holds the list of the coroutine test functions in `tests` and manages the registration
    // of those tests from Test class as well as firing up the coroutines from the core
    class TestManager {
    public:
        using CoroutineHandle = std::coroutine_handle<>;

        // retrieves singleton isntance of the test manager class
        static TestManager& getInstance() {
            static TestManager instance;
            return instance;
        }

        // registers the test by pushing the test coroutine into the list
        void registerTest(const std::string& name, std::function<CoroutineHandle()> testFunction) {
            tests[name].push_back(testFunction);
        }

        // gets the list of test coroutines which will be fired up inside core
        const std::unordered_map<std::string, std::vector<std::function<CoroutineHandle()>>>& getTests() const {
            return tests;
        }

    private:
        TestManager() = default;
        std::unordered_map<std::string, std::vector<std::function<CoroutineHandle()>>> tests;
    };

    // This class provides a static method to register test functions in the TestManager
    class RegistrationHelper {
    public:
        template<typename T>
        static void registerTest(T* instance, const std::string& name, std::function<std::coroutine_handle<>()> func) {
            TestManager::getInstance().registerTest(name, [instance, func]() { return func(); });
        }
    };

}

#endif // TEST_MANAGER_H
