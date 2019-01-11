/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

#include <boost/callable_traits/is_transaction_safe.hpp>
#include "test.hpp"

template<typename Safe, typename NotSafe>
void test() {

    CT_ASSERT( is_transaction_safe<Safe>::value
        // for when tx safe is disabled
        || std::is_same<Safe, NotSafe>::value);
    CT_ASSERT(! is_transaction_safe<NotSafe>::value);
}

#define TEST_TRANSACTION_SAFE(not_safe) \
    test<not_safe BOOST_CLBL_TRTS_TRANSACTION_SAFE_SPECIFIER, not_safe>()

int main() {

    TEST_TRANSACTION_SAFE(int(*)());

    #ifndef BOOST_CLBL_TRTS_DISABLE_ABOMINABLE_FUNCTIONS
    TEST_TRANSACTION_SAFE(int(...) volatile LREF);
    TEST_TRANSACTION_SAFE(int() const);
    #endif

    TEST_TRANSACTION_SAFE(int(*)(...));

    struct foo;

    TEST_TRANSACTION_SAFE(int(foo::*)());
    TEST_TRANSACTION_SAFE(int(foo::*)() const volatile);
    TEST_TRANSACTION_SAFE(int(foo::*)(...));
    TEST_TRANSACTION_SAFE(int(foo::*)(...) const volatile RREF);
}

