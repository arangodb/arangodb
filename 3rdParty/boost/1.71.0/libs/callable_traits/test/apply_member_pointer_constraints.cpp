/*<-
Copyright (c) 2016 Barrett Adair

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
->*/

#include <boost/callable_traits/apply_member_pointer.hpp>
#include "test.hpp"

struct foo;

template<typename T, typename U>
struct is_substitution_failure_apply_member_pointer {

    template<typename, typename>
    static auto test(...) -> std::true_type;

    template<typename A, typename B,
        typename std::remove_reference<
            TRAIT(apply_member_pointer, A, B)>::type* = nullptr>
    static auto test(int) -> std::false_type;

    static constexpr bool value = decltype(test<T, U>(0))::value;
};

int main() {
    CT_ASSERT(is_substitution_failure_apply_member_pointer<void, foo>::value);
    CT_ASSERT(is_substitution_failure_apply_member_pointer<int,  int>::value);
    CT_ASSERT(is_substitution_failure_apply_member_pointer<void, int>::value);
}

