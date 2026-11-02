#pragma once

// Minimal dependency-free test harness.

#include <cstring>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace harness {

struct TestCase {
    const char* name;
    void (*fn)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& current_failures() {
    static int failures = 0;
    return failures;
}

struct Register {
    Register(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

inline void fail(const char* file, int line, const std::string& message) {
    ++current_failures();
    std::cerr << "    " << file << ":" << line << ": " << message << "\n";
}

// Runs every test whose name contains filter (all tests if filter is null).
inline int run_all(const char* filter) {
    int passed = 0;
    int failed = 0;
    for (const TestCase& test : registry()) {
        if (filter && !std::strstr(test.name, filter))
            continue;
        current_failures() = 0;
        try {
            test.fn();
        } catch (const std::exception& e) {
            fail(__FILE__, __LINE__, std::string("unexpected exception: ") + e.what());
        }
        if (current_failures() == 0) {
            ++passed;
        } else {
            ++failed;
            std::cerr << "FAIL " << test.name << "\n";
        }
    }
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

}

#define TEST(name)                                                    \
    static void name();                                               \
    static const harness::Register register_##name(#name, name);      \
    static void name()

#define CHECK(cond)                                                   \
    do {                                                              \
        if (!(cond))                                                  \
            harness::fail(__FILE__, __LINE__, "CHECK(" #cond ")");    \
    } while (0)

#define CHECK_EQ(actual, expected)                                    \
    do {                                                              \
        const auto& a_ = (actual);                                    \
        const auto& e_ = (expected);                                  \
        if (!(a_ == e_)) {                                            \
            std::ostringstream msg_;                                  \
            msg_ << "CHECK_EQ(" #actual ", " #expected ")\n"          \
                 << "      actual:   " << a_ << "\n"                  \
                 << "      expected: " << e_;                         \
            harness::fail(__FILE__, __LINE__, msg_.str());            \
        }                                                             \
    } while (0)

#define CHECK_THROWS(expr, ExceptionT)                                \
    do {                                                              \
        bool caught_ = false;                                         \
        try {                                                         \
            (void)(expr);                                             \
        } catch (const ExceptionT&) {                                 \
            caught_ = true;                                           \
        } catch (...) {                                               \
        }                                                             \
        if (!caught_)                                                 \
            harness::fail(__FILE__, __LINE__,                         \
                          "CHECK_THROWS(" #expr ", " #ExceptionT ")");\
    } while (0)
