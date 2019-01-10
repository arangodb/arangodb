
/*
Copyright Barrett Adair 2016-2017

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.md or copy at http ://boost.org/LICENSE_1_0.txt)

*/

#include <boost/callable_traits/args.hpp>
#include "test.hpp"

struct foo;

template<typename T>
struct is_substitution_failure_args {

    template<typename>
    static auto test(...) -> std::true_type;

    template<typename U,
        TRAIT(boost::callable_traits::args, U, std::tuple)* = nullptr>
    static auto test(int) -> std::false_type;

    static constexpr bool value = decltype(test<T>(0))::value;
};

int main() {

    CT_ASSERT(is_substitution_failure_args<int>::value);
    CT_ASSERT(is_substitution_failure_args<int &>::value);
    CT_ASSERT(is_substitution_failure_args<int (** const &)()>::value);
    CT_ASSERT(is_substitution_failure_args<int (foo::** &)()>::value);
    CT_ASSERT(is_substitution_failure_args<int (foo::** const)()>::value);
    CT_ASSERT(is_substitution_failure_args<int (foo::** const &)()>::value);
    CT_ASSERT(is_substitution_failure_args<int (foo::** volatile)()>::value);
    CT_ASSERT(!is_substitution_failure_args<int (* const &)()>::value);
    CT_ASSERT(!is_substitution_failure_args<int (foo::* &)()>::value);
    CT_ASSERT(!is_substitution_failure_args<int (foo::* const)()>::value);
    CT_ASSERT(!is_substitution_failure_args<int (foo::* const &)()>::value);
    CT_ASSERT(!is_substitution_failure_args<int (foo::* volatile)()>::value);
    auto lambda = [](){};
    CT_ASSERT(!is_substitution_failure_args<decltype(lambda)&>::value);
    CT_ASSERT(is_substitution_failure_args<void>::value);
}

