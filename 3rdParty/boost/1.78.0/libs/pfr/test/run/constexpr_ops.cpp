// Copyright (c) 2016-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/ops.hpp>
#include <boost/pfr/io.hpp>

#include <iostream>
#include <typeinfo>
#include <tuple>
#include <sstream>
#include <set>
#include <string>

#include <boost/config.hpp>
#include <boost/core/lightweight_test.hpp>

#ifdef __clang__
#   pragma clang diagnostic ignored "-Wmissing-braces"
#endif

union test_union {
    int i;
    float f;
};

constexpr bool operator< (test_union l, test_union r) noexcept { return l.i <  r.i; }
constexpr bool operator<=(test_union l, test_union r) noexcept { return l.i <= r.i; }
constexpr bool operator> (test_union l, test_union r) noexcept { return l.i >  r.i; }
constexpr bool operator>=(test_union l, test_union r) noexcept { return l.i >= r.i; }
constexpr bool operator==(test_union l, test_union r) noexcept { return l.i == r.i; }
constexpr bool operator!=(test_union l, test_union r) noexcept { return l.i != r.i; }


template <class T>
void test_constexpr_comparable() {
    using namespace boost::pfr;
    constexpr T s1 {110, 1, true, 6,17,8,9,10,11};
    constexpr T s2 = s1;
    constexpr T s3 {110, 1, true, 6,17,8,9,10,11111};
    static_assert(eq(s1, s2), "");
    static_assert(le(s1, s2), "");
    static_assert(ge(s1, s2), "");
    static_assert(!ne(s1, s2), "");
    static_assert(!eq(s1, s3), "");
    static_assert(ne(s1, s3), "");
    static_assert(lt(s1, s3), "");
    static_assert(gt(s3, s2), "");
    static_assert(le(s1, s3), "");
    static_assert(ge(s3, s2), "");
}

namespace foo {
struct comparable_struct {
    int i; short s; bool bl; int a,b,c,d,e,f;
};
}

int main() {
    // MSVC fails to use strucutred bindings in constexpr:
    //
    // error C2131: expression did not evaluate to a constant
    // pfr/detail/functional.hpp(21): note: failure was caused by a read of a variable outside its lifetime
#if !defined(_MSC_VER) || (_MSC_VER >= 1927) || !BOOST_PFR_USE_CPP17
    test_constexpr_comparable<foo::comparable_struct>();

    struct local_comparable_struct {
        int i; short s; bool bl; int a,b,c,d,e,f;
    };
    test_constexpr_comparable<local_comparable_struct>();

    struct local_comparable_struct_with_union {
        int i; short s; bool bl; int a,b,c,d,e; test_union u;
    };
    test_constexpr_comparable<local_comparable_struct>();
#endif
    return boost::report_errors();
}


