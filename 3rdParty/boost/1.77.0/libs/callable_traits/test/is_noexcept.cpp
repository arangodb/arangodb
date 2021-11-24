/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

#include <boost/callable_traits/detail/config.hpp>

#include <boost/callable_traits/is_noexcept.hpp>
#include "test.hpp"

template<typename Noexcept, typename NotNoexcept>
void test() {

    CT_ASSERT( is_noexcept<Noexcept>::value
        // for old compilers, TEST_NOEXCEPT_QUAL is empty so types are same
        || std::is_same<Noexcept, NotNoexcept>::value);

    CT_ASSERT(! is_noexcept<NotNoexcept>::value);
}

#define TEST_NOEXCEPT(not_noexcept) test<not_noexcept BOOST_CLBL_TRTS_NOEXCEPT_SPECIFIER, not_noexcept>()

int main() {

    TEST_NOEXCEPT(int(*)());

    #ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS
    TEST_NOEXCEPT(int() const);
    TEST_NOEXCEPT(int(...) volatile LREF);
    #endif

    TEST_NOEXCEPT(int(*)(...));

    struct foo;

    TEST_NOEXCEPT(int(foo::*)());
    TEST_NOEXCEPT(int(foo::*)() const volatile);
    TEST_NOEXCEPT(int(foo::*)(...));
    TEST_NOEXCEPT(int(foo::*)(...) const volatile RREF);
}

