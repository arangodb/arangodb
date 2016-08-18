// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include "minimal.hpp"

#include <boost/hana/concept/constant.hpp>
#include <boost/hana/value.hpp>
namespace hana = boost::hana;


// Make sure we really satisfy Constant.
static_assert(hana::Constant<minimal_constant<int, 0>>::value, "");
static_assert(hana::Constant<minimal_constant<int, 1>>::value, "");
static_assert(hana::Constant<minimal_constant<int, 2>>::value, "");
static_assert(hana::Constant<minimal_constant<int, 3>>::value, "");

// Make sure we can use hana::value<> properly.
static_assert(hana::value<minimal_constant<int, 0>>() == 0, "");
static_assert(hana::value<minimal_constant<int, 1>>() == 1, "");
static_assert(hana::value<minimal_constant<int, 2>>() == 2, "");
static_assert(hana::value<minimal_constant<int, 3>>() == 3, "");

// Check the equivalence between `value(...)` and `value<decltype(...)>()`.
static_assert(hana::value(minimal_constant<int, 0>{}) == hana::value<minimal_constant<int, 0>>(), "");
static_assert(hana::value(minimal_constant<int, 1>{}) == hana::value<minimal_constant<int, 1>>(), "");
static_assert(hana::value(minimal_constant<int, 2>{}) == hana::value<minimal_constant<int, 2>>(), "");
static_assert(hana::value(minimal_constant<int, 3>{}) == hana::value<minimal_constant<int, 3>>(), "");


int main() { }
