/*

Copyright Barrett Adair 2015-2017
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

*/

#include <type_traits>
#include <cstdint>
#include <memory>
#include <boost/callable_traits.hpp>
#include "test.hpp"

struct foo1 {
    int bar(char, float&, int = 0) { return{}; }
};

struct foo2 {
    int bar(char, float&, int = 0, ...) { return{}; }
};

struct foo3 {
    int operator()(char, float&, int = 0) { return{}; }
};

struct foo4 {
    int operator()(char, float&, int = 0, ...) { return{}; }
};

int foo5(char, float&, int = 0) { return{}; }

int foo6(char, float&, int = 0, ...) { return{}; }

using std::is_same;

int main() {

    {
        using pmf = decltype(&foo1::bar);
        CT_ASSERT(std::is_same< return_type_t<pmf>, int>{});
    } {
        using pmf = decltype(&foo2::bar);
        CT_ASSERT(std::is_same< return_type_t<pmf>, int>{});
    } {
        CT_ASSERT(std::is_same< return_type_t<foo3>, int>{});
    } {
        CT_ASSERT(std::is_same< return_type_t<foo4>, int>{});
    } {
        CT_ASSERT(std::is_same< return_type_t<decltype(foo5)>, int>{});
    } {
        CT_ASSERT(std::is_same< return_type_t<decltype(foo6)>, int>{});
    }

    return 0;
}
