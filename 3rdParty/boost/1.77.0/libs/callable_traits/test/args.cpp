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

struct foo7 {
    int bar() { return{}; }
};

using std::is_same;

int main() {

    {
        using pmf = decltype(&foo1::bar);
        using args_t =  TRAIT(args, pmf);
        CT_ASSERT(is_same<args_t, std::tuple<foo1&, char, float&, int>>{});
    } {
        using pmf = decltype(&foo2::bar);
        using args_t =  TRAIT(args, pmf);
        CT_ASSERT(is_same<args_t, std::tuple<foo2&, char, float&, int>>{});
    } {
        using args_t =  TRAIT(args, foo3);
        CT_ASSERT(is_same<args_t, std::tuple<char, float&, int>>{});
    } {
        using args_t =  TRAIT(args, foo4);
        CT_ASSERT(is_same<args_t, std::tuple<char, float&, int>>{});
    } {
        using args_t =  TRAIT(args, decltype(foo5));
        CT_ASSERT(is_same<args_t, std::tuple<char, float&, int>>{});
    } {
        using args_t =  TRAIT(args, decltype(foo6));
        CT_ASSERT(is_same<args_t, std::tuple<char, float&, int>>{});
    }

    return 0;
}
