// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/any_of.hpp>
#include <boost/hana/detail/wrong.hpp>
#include <boost/hana/integral_constant.hpp>
namespace hana = boost::hana;


template <typename I>
struct is_even {
    static constexpr bool value = I::value % 2 == 0;
};


static_assert(!hana::detail::any_of<is_even>::value, "");
static_assert(!hana::detail::any_of<is_even, hana::int_<1>>::value, "");
static_assert(!hana::detail::any_of<is_even, hana::int_<1>, hana::int_<3>>::value, "");
static_assert(!hana::detail::any_of<is_even, hana::int_<1>, hana::int_<3>, hana::int_<5>>::value, "");
static_assert(!hana::detail::any_of<is_even, hana::int_<1>, hana::int_<3>, hana::int_<5>, hana::int_<7>>::value, "");

static_assert(hana::detail::any_of<is_even, hana::int_<0>>::value, "");
static_assert(hana::detail::any_of<is_even, hana::int_<0>, hana::int_<2>>::value, "");
static_assert(hana::detail::any_of<is_even, hana::int_<0>, hana::int_<2>, hana::int_<4>>::value, "");
static_assert(hana::detail::any_of<is_even, hana::int_<0>, hana::int_<2>, hana::int_<4>, hana::int_<6>>::value, "");

static_assert(hana::detail::any_of<is_even, hana::int_<0>, hana::int_<1>>::value, "");
static_assert(hana::detail::any_of<is_even, hana::int_<0>, hana::int_<1>, hana::int_<2>>::value, "");
static_assert(hana::detail::any_of<is_even, hana::int_<0>, hana::int_<1>, hana::int_<2>, hana::int_<3>>::value, "");
static_assert(hana::detail::any_of<is_even, hana::int_<0>, hana::int_<1>, hana::int_<2>, hana::int_<3>, hana::int_<4>>::value, "");
static_assert(hana::detail::any_of<is_even, hana::int_<1>, hana::int_<3>, hana::int_<5>, hana::int_<8>>::value, "");
static_assert(hana::detail::any_of<is_even, hana::int_<1>, hana::int_<8>, hana::int_<5>, hana::int_<7>>::value, "");

// Make sure we short-circuit properly
template <typename ...Dummy>
struct fail {
    static_assert(hana::detail::wrong<Dummy...>{},
        "this must never be instantiated");
};
static_assert(hana::detail::any_of<is_even, hana::int_<1>, hana::int_<2>, fail<>>::value, "");
static_assert(hana::detail::any_of<is_even, hana::int_<1>, hana::int_<2>, fail<>, hana::int_<3>>::value, "");
static_assert(hana::detail::any_of<is_even, hana::int_<1>, hana::int_<2>, fail<>, hana::int_<4>>::value, "");

int main() { }
