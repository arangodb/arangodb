// Copyright (c) 2016-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/pfr/core.hpp>


template <class T1>
void test_counts_on_multiple_chars_impl() {
    using boost::pfr::tuple_size_v;

    struct t1_0 { T1 v1; };
#if !defined(__GNUC__) || __GNUC__ != 8
    // GCC-8 has big problems with this test:
    //    error: 'constexpr ubiq_constructor::operator Type&() const [with Type = test_counts_on_multiple_chars()::t2*]',
    //    declared using local type 'test_counts_on_multiple_chars()::t2', is used but never defined [-fpermissive]
    //
    // Fixed in GCC-9.
    static_assert(tuple_size_v<T1*> == 1, "");
#endif

    struct t1_0_1 { t1_0 t1; };
    static_assert(tuple_size_v<t1_0_1> == 1, "");

    struct t1_0_2 { t1_0 t1; t1_0 t2; };
    static_assert(tuple_size_v<t1_0_2> == 2, "");
}

template <class T>
void test_counts_on_multiple_chars() {
    using boost::pfr::tuple_size_v;

    test_counts_on_multiple_chars_impl<T>();

    struct t2 { T v1; T v2; };
    static_assert(tuple_size_v<t2> == 2, "");

    test_counts_on_multiple_chars_impl<t2>();
    test_counts_on_multiple_chars_impl<T[2]>();

    test_counts_on_multiple_chars_impl<T[3]>();
    test_counts_on_multiple_chars_impl<T[4]>();

    struct t8 { T v1; T v2; T v3; T v4; T v5; T v6; T v7; T v8; };
    static_assert(tuple_size_v<t8> == 8, "");
    test_counts_on_multiple_chars_impl<t8>();
}

int main() {
    test_counts_on_multiple_chars< BOOST_PFR_RUN_TEST_ON >();
}

