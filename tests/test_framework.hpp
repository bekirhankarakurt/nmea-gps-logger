#pragma once
// Minimal dependency-free unit test framework.

#include <cmath>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace testing {

struct TestCase {
    const char* name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failures() {
    static int count = 0;
    return count;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> body) { registry().push_back({name, std::move(body)}); }
};

inline void report(const char* file, int line, const std::string& message) {
    ++failures();
    std::cerr << file << ':' << line << ": FAILED: " << message << '\n';
}

/// Thrown by REQUIRE to abort the current test case.
struct AbortTest {};

}  // namespace testing

#define TEST_CONCAT_INNER(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_INNER(a, b)

#define TEST(name)                                                                  \
    static void name();                                                             \
    static const ::testing::Registrar TEST_CONCAT(registrar_, name){#name, &name};  \
    static void name()

#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            ::testing::report(__FILE__, __LINE__, "CHECK(" #cond ")"); \
        }                                                          \
    } while (false)

#define REQUIRE(cond)                                                    \
    do {                                                                 \
        if (!(cond)) {                                                   \
            ::testing::report(__FILE__, __LINE__, "REQUIRE(" #cond ")"); \
            throw ::testing::AbortTest{};                                \
        }                                                                \
    } while (false)

#define CHECK_EQ(a, b)                                                                     \
    do {                                                                                   \
        const auto& test_a_ = (a);                                                         \
        const auto& test_b_ = (b);                                                         \
        if (!(test_a_ == test_b_)) {                                                       \
            std::ostringstream test_os_;                                                   \
            test_os_ << "CHECK_EQ(" #a ", " #b "): " << test_a_ << " != " << test_b_;      \
            ::testing::report(__FILE__, __LINE__, test_os_.str());                         \
        }                                                                                  \
    } while (false)

#define CHECK_NEAR(a, b, tol)                                                                    \
    do {                                                                                         \
        const double test_a_ = (a);                                                              \
        const double test_b_ = (b);                                                              \
        if (!(std::fabs(test_a_ - test_b_) <= (tol))) {                                          \
            std::ostringstream test_os_;                                                         \
            test_os_.precision(12);                                                              \
            test_os_ << "CHECK_NEAR(" #a ", " #b ", " #tol "): " << test_a_ << " vs " << test_b_; \
            ::testing::report(__FILE__, __LINE__, test_os_.str());                               \
        }                                                                                        \
    } while (false)

#define CHECK_THROWS(expr, type)                                                \
    do {                                                                        \
        bool test_thrown_ = false;                                              \
        try {                                                                   \
            (void)(expr);                                                       \
        } catch (const type&) {                                                 \
            test_thrown_ = true;                                                \
        }                                                                       \
        if (!test_thrown_) {                                                    \
            ::testing::report(__FILE__, __LINE__, "CHECK_THROWS(" #expr ", " #type ")"); \
        }                                                                       \
    } while (false)
