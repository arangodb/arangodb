// Copyright (c) 2019-2021 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <type_traits>

#include <boost/pfr/core.hpp>

template <class T>
struct constrained_template {
    constrained_template() = default;

    template <
      class U = T,
      std::enable_if_t<
          std::is_constructible<T, U&&>::value
          || sizeof(decltype(T{std::declval<U&&>()}))
      , bool> = false>
    constexpr constrained_template(U&& val)
        : value_{std::forward<U>(val)}
    {}

    T value_;
};

struct int_element {
    int value_;
};

struct aggregate_constrained {
    constrained_template<short> a;
    constrained_template<int_element> b;
};

int main() {
    static_assert(
        std::is_same<
            boost::pfr::tuple_element_t<0, aggregate_constrained>,
            constrained_template<short>
        >::value,
        "Precise reflection with template constructors fails to work"
    );

    static_assert(
        std::is_same<
            boost::pfr::tuple_element_t<1, aggregate_constrained>,
            constrained_template<int_element>
        >::value,
        "Precise reflection with template constructors fails to work"
    );

    short s = 3;
    aggregate_constrained aggr{s, 4};
    return boost::pfr::get<1>(aggr).value_.value_ - 4;
}
