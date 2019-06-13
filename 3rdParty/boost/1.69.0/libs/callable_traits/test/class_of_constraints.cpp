
/*
Copyright Barrett Adair 2016-2017

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)

*/

#include <boost/callable_traits/class_of.hpp>
#include "test.hpp"

struct foo;

template<typename T>
struct is_substitution_failure_class_of {

    template<typename>
    static auto test(...) -> std::true_type;

    template<typename A,
        typename std::remove_reference<
            TRAIT(class_of, A)>::type* = nullptr>
    static auto test(int) -> std::false_type;

    static constexpr bool value = decltype(test<T>(0))::value;
};

int main() {

    auto lambda = [](){};

    CT_ASSERT(is_substitution_failure_class_of<decltype(lambda)>::value);
    CT_ASSERT(is_substitution_failure_class_of<decltype(lambda)&>::value);
    CT_ASSERT(is_substitution_failure_class_of<void>::value);
    CT_ASSERT(is_substitution_failure_class_of<void*>::value);
    CT_ASSERT(is_substitution_failure_class_of<int>::value);
    CT_ASSERT(is_substitution_failure_class_of<int &>::value);
    CT_ASSERT(is_substitution_failure_class_of<int()>::value);
    CT_ASSERT(is_substitution_failure_class_of<int(*)()>::value);
    CT_ASSERT(is_substitution_failure_class_of<int(**)()>::value);
    CT_ASSERT(is_substitution_failure_class_of<int(&)()>::value);
    CT_ASSERT(is_substitution_failure_class_of<int (foo::** const)()>::value);
}

