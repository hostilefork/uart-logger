//
// uart_logger.cpp
//
// Copyright (c) 2021 hostilefork.com
// MIT License
//

#include "uart_logger.h"

namespace Uprint {

const Unspaced<const char*> nospace { "" };
const Unspaced<const char*> comma { ", " };  // includes own space on the right

UartLogger uprint;

}  // end namespace Uprint


//=//// MOCKING TESTS //////////////////////////////////////////////////////=//
//
// !!! The goal is to hook this up to Google Test and CMake.

#ifdef MOCKING
    #include <iostream>

    using namespace Uprint;

    template<typename... Args>
    inline std::string uprint_str(Args... args) {
        std::stringstream().swap(uprint_out);  // clear buffer
        uprint(args...);  // writes go to uprint_out due to fake uart_puts()
        std::string result = uprint_out.str();  // copy contiguous string
        std::stringstream().swap(uprint_out);  // clear buffer again
        return result;
    }

    #define ASSERT_EQ(expr_left, expr_right) \
    do { \
        auto l = expr_left; \
        auto r = expr_right; \
        std::cout << l; \
        if (r != l) { \
            std::cout << "MISMATCH, expected:" << r << "\n"; \
            assert(r == l); \
        } \
    } while (0)

    int main() {
        ASSERT_EQ(uprint_str(), "\n");
        ASSERT_EQ(uprint_str(1), "1\n");
        
        ASSERT_EQ(uprint_str(1, 2), "1 2\n");

        ASSERT_EQ(uprint_str(1, nospace, 2), "12\n");
        ASSERT_EQ(uprint_str(1, nospace, 2, 3), "12 3\n");

        ASSERT_EQ(uprint_str(hex(0)), "0\n");
        ASSERT_EQ(uprint_str(hex(10)), "A\n");

        ASSERT_EQ(uprint_str(binary(0)), "0\n");
        ASSERT_EQ(uprint_str(binary(4)), "100\n");

        ASSERT_EQ(uprint_str(true, false), "true false\n");

        ASSERT_EQ(uprint_str("Foo", comma, "Bar"), "Foo, Bar\n");

        ASSERT_EQ(uprint_str("[", unspaced(304), "]"), "[304]\n");

        ASSERT_EQ(
            uprint_str("Current:", units(5, "mA")),
            "Current: 5mA\n"
        );

        ASSERT_EQ(uprint_str(comma, comma), ", , \n");

        ASSERT_EQ(uprint_str(comma, comma, comma), ", , , \n");
    }
#endif
