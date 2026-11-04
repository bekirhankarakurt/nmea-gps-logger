#include "test_framework.hpp"

#include <exception>

int main() {
    int run = 0;
    for (const auto& test : testing::registry()) {
        const int before = testing::failures();
        try {
            test.body();
        } catch (const testing::AbortTest&) {
            // Already reported.
        } catch (const std::exception& e) {
            testing::report(test.name, 0, std::string("unexpected exception: ") + e.what());
        }
        ++run;
        std::cout << (testing::failures() == before ? "[ OK ] " : "[FAIL] ") << test.name << '\n';
    }
    std::cout << run << " tests, " << testing::failures() << " failures\n";
    return testing::failures() == 0 ? 0 : 1;
}
