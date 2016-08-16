// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>
#include <boost/hana/find.hpp>
#include <boost/hana/find_if.hpp>
#include <boost/hana/functional/compose.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;


// First get the type of the object, and then call the trait on it.
constexpr auto is_integral = hana::compose(hana::trait<std::is_integral>, hana::decltype_);
constexpr auto is_class = hana::compose(hana::trait<std::is_class>, hana::decltype_);

static_assert(
    hana::find_if(hana::make_tuple(1.0, 2, '3'), is_integral)
        ==
    hana::just(2)
, "");

BOOST_HANA_CONSTANT_CHECK(
    hana::find_if(hana::make_tuple(1.0, 2, '3'), is_class)
        ==
    hana::nothing
);

BOOST_HANA_CONSTANT_CHECK(
    hana::find(hana::make_tuple(hana::int_c<1>, hana::char_c<'c'>, hana::type_c<void>), hana::type_c<void>)
        ==
    hana::just(hana::type_c<void>)
);

BOOST_HANA_CONSTANT_CHECK(
    hana::find(hana::make_tuple(hana::int_c<1>, hana::char_c<'c'>, hana::type_c<void>), hana::type_c<int>)
        ==
    hana::nothing
);

int main() { }
