/*
Copyright Barrett Adair 2016-2017

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)

*/

#include <boost/callable_traits/remove_transaction_safe.hpp>
#include <tuple>
#include "test.hpp"

#ifndef BOOST_CLBL_TRTS_ENABLE_TRANSACTION_SAFE
int main(){}
#else

template<typename T>
struct is_substitution_failure_remove_tx_safe {

    template<typename>
    static auto test(...) -> std::true_type;

    template<typename A,
        typename std::remove_reference<
            TRAIT(remove_transaction_safe, A)>::type* = nullptr>
    static auto test(int) -> std::false_type;

    static constexpr bool value = decltype(test<T>(0))::value;
};

struct foo;

int main() {

    auto lambda = [](){};

    CT_ASSERT(is_substitution_failure_remove_tx_safe<decltype(lambda)>::value);
    CT_ASSERT(is_substitution_failure_remove_tx_safe<decltype(lambda)&>::value);
    CT_ASSERT(is_substitution_failure_remove_tx_safe<int>::value);
    CT_ASSERT(is_substitution_failure_remove_tx_safe<int &>::value);
    CT_ASSERT(is_substitution_failure_remove_tx_safe<int (* const &)()>::value);
    CT_ASSERT(is_substitution_failure_remove_tx_safe<int (foo::* &)()>::value);
    CT_ASSERT(is_substitution_failure_remove_tx_safe<int (foo::* const)()>::value);
    CT_ASSERT(is_substitution_failure_remove_tx_safe<int (foo::* const &)()>::value);
    CT_ASSERT(is_substitution_failure_remove_tx_safe<int (foo::* volatile)()>::value);
    CT_ASSERT(is_substitution_failure_remove_tx_safe<void>::value);
    CT_ASSERT(is_substitution_failure_remove_tx_safe<void*>::value);
    CT_ASSERT(is_substitution_failure_remove_tx_safe<void(**)()>::value);
}

#endif //#ifndef BOOST_CLBL_TRTS_ENABLE_TRANSACTION_SAFE
