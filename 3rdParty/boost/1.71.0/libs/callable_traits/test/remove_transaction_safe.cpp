/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

#include "test.hpp"
#include <boost/callable_traits/remove_transaction_safe.hpp>

template<typename Safe, typename NotSafe>
void test() {

    CT_ASSERT(std::is_same<NotSafe, TRAIT(remove_transaction_safe, Safe)>::value);

    //sanity check
    #ifdef BOOST_CLBL_TRTS_ENABLE_TRANSACTION_SAFE
    CT_ASSERT(!std::is_same<Safe, NotSafe>::value);
    #endif
}

#define TEST_TRANSACTION_SAFE(not_safe) \
    test<not_safe BOOST_CLBL_TRTS_TRANSACTION_SAFE_SPECIFIER, not_safe>()

int main() {

    TEST_TRANSACTION_SAFE(int(int) LREF);
    TEST_TRANSACTION_SAFE(int(*)(int));
    TEST_TRANSACTION_SAFE(int(int, ...) RREF);
    TEST_TRANSACTION_SAFE(int(*)(int, ...));

    struct foo;

    TEST_TRANSACTION_SAFE(int(foo::*)(int));
    TEST_TRANSACTION_SAFE(int(foo::*)(int) const);
    TEST_TRANSACTION_SAFE(int(foo::*)(int, ...));
    TEST_TRANSACTION_SAFE(int(foo::*)(int, ...) volatile);
}

