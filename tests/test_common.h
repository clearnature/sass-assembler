/* ============================================================================
 * HunTian SASS Assembler — 轻量测试框架
 * ============================================================================ */

#ifndef SASS_TEST_H
#define SASS_TEST_H

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <sstream>

// ─── 断言宏 ───
#define ASSERT_EQ(a, b) \
    do { if ((a) != (b)) { \
        std::cerr << "  FAIL: " #a " != " #b \
                  << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
        return false; \
    } } while(0)

#define ASSERT_TRUE(x) \
    do { if (!(x)) { \
        std::cerr << "  FAIL: " #x " is not true" \
                  << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
        return false; \
    } } while(0)

#define ASSERT_FALSE(x) \
    do { if (x) { \
        std::cerr << "  FAIL: " #x " is not false" \
                  << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
        return false; \
    } } while(0)

#define ASSERT_STR_EQ(a, b) \
    do { if (std::string(a) != std::string(b)) { \
        std::cerr << "  FAIL: " #a " != " #b \
                  << " at " << __FILE__ << ":" << __LINE__ << "\n"; \
        return false; \
    } } while(0)

// ─── 测试注册 ───
struct TestCase {
    std::string name;
    std::function<bool()> func;
};

static std::vector<TestCase>& get_tests() {
    static std::vector<TestCase> tests;
    return tests;
}

#define TEST(name) \
    static bool test_##name(); \
    static struct Reg_##name { Reg_##name() { \
        get_tests().push_back({#name, test_##name}); \
    } } reg_##name; \
    static bool test_##name()

// ─── 运行器 ───
inline int run_all_tests() {
    auto& tests = get_tests();
    int passed = 0, failed = 0;

    std::cout << "Running " << tests.size() << " tests...\n\n";

    for (auto& t : tests) {
        std::cout << "  " << t.name << "... ";
        if (t.func()) {
            std::cout << "PASS\n";
            passed++;
        } else {
            std::cout << "FAIL\n";
            failed++;
        }
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed"
              << " out of " << tests.size() << " tests.\n";

    return failed;
}

#endif // SASS_TEST_H
