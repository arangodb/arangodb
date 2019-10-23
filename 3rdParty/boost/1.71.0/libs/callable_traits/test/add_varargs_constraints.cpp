/*
Copyright Barrett Adair 2016-2017

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)

*/

#include <boost/callable_traits/add_varargs.hpp>
#include <tuple>
#include "test.hpp"

struct foo;

template<typename T>
struct is_substitution_failure_add_varargs {

    template<typename>
    static auto test(...) -> std::true_type;

    template<typename A,
        typename std::remove_reference<
            TRAIT(add_varargs, A)>::type* = nullptr>
    static auto test(int) -> std::false_type;

    static constexpr bool value = decltype(test<T>(0))::value;
};

int main() {

    auto lambda = [](){};

    CT_ASSERT(is_substitution_failure_add_varargs<decltype(lambda)>::value);
    CT_ASSERT(is_substitution_failure_add_varargs<decltype(lambda)&>::value);
    CT_ASSERT(is_substitution_failure_add_varargs<int>::value);
    CT_ASSERT(is_substitution_failure_add_varargs<int &>::value);
    CT_ASSERT(is_substitution_failure_add_varargs<int (* const &)()>::value);
    CT_ASSERT(is_substitution_failure_add_varargs<int (foo::* &)()>::value);
    CT_ASSERT(is_substitution_failure_add_varargs<int (foo::* const)()>::value);
    CT_ASSERT(is_substitution_failure_add_varargs<int (foo::* const &)()>::value);
    CT_ASSERT(is_substitution_failure_add_varargs<int (foo::* volatile)()>::value);
    CT_ASSERT(is_substitution_failure_add_varargs<void>::value);
    CT_ASSERT(is_substitution_failure_add_varargs<void*>::value);
    CT_ASSERT(is_substitution_failure_add_varargs<void(**)()>::value);
}
