// Copyright (c) 2019-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <type_traits>

#include <boost/pfr/core.hpp>

template <class T>
struct unconstrained_forwarding_ref {
    unconstrained_forwarding_ref() = default;
    unconstrained_forwarding_ref(const unconstrained_forwarding_ref&) = default;
    unconstrained_forwarding_ref(unconstrained_forwarding_ref&&) = default;
    unconstrained_forwarding_ref& operator=(const unconstrained_forwarding_ref&) = default;
    unconstrained_forwarding_ref& operator=(unconstrained_forwarding_ref&&) = default;

    template <class U>
    constexpr unconstrained_forwarding_ref(U&& val)
        : value_{std::forward<U>(val)}
    {}

    T value_{};
};

struct int_element {
    int value_;
};


struct aggregate_unconstrained {
    unconstrained_forwarding_ref<int> a;
    unconstrained_forwarding_ref<int_element> b;
};

int main() {
    using sanity = decltype(aggregate_unconstrained{
      boost::pfr::detail::ubiq_lref_constructor{0},
      boost::pfr::detail::ubiq_lref_constructor{1},
    });
    static_assert(
        std::is_same<
            sanity, aggregate_unconstrained
        >::value,
        "Precise reflection with template constructors sanity check fails"
    );

    boost::pfr::detail::enable_if_constructible_helper_t<aggregate_unconstrained, 2> foo;
    (void)foo;

    static_assert(
        std::is_same<
            boost::pfr::tuple_element_t<0, aggregate_unconstrained>,
            unconstrained_forwarding_ref<int>
        >::value,
        "Precise reflection with template constructors fails to work"
    );

    static_assert(
        std::is_same<
            boost::pfr::tuple_element_t<1, aggregate_unconstrained>,
            unconstrained_forwarding_ref<int_element>
        >::value,
        "Precise reflection with template constructors fails to work"
    );

    aggregate_unconstrained aggr{3, 4};
    return boost::pfr::get<1>(aggr).value_.value_ - 4;
}
