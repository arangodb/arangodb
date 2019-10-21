/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

#include <boost/callable_traits/detail/config.hpp>
#include "test.hpp"

#include <boost/callable_traits/remove_noexcept.hpp>

template<typename Noexcept, typename NotNoexcept>
void test() {

    CT_ASSERT(std::is_same<NotNoexcept,  TRAIT(remove_noexcept, Noexcept)>::value);
}

#define TEST_NOEXCEPT(not_noexcept) \
    test<not_noexcept BOOST_CLBL_TRTS_NOEXCEPT_SPECIFIER, not_noexcept>()

int main() {

    TEST_NOEXCEPT(int(int) LREF);
    TEST_NOEXCEPT(int(*)(int));
    TEST_NOEXCEPT(int(int, ...) RREF);
    TEST_NOEXCEPT(int(*)(int, ...));

    struct foo;

    TEST_NOEXCEPT(int(foo::*)(int));
    TEST_NOEXCEPT(int(foo::*)(int) const);
    TEST_NOEXCEPT(int(foo::*)(int, ...));
    TEST_NOEXCEPT(int(foo::*)(int, ...) volatile);
}

