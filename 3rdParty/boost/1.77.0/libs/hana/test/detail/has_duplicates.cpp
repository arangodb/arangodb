// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/has_duplicates.hpp>
#include <boost/hana/integral_constant.hpp>
namespace hana = boost::hana;


static_assert(!hana::detail::has_duplicates<>::value, "");

static_assert(!hana::detail::has_duplicates<
    hana::int_<0>
>::value, "");

static_assert(!hana::detail::has_duplicates<
    hana::int_<0>, hana::int_<1>
>::value, "");

static_assert(!hana::detail::has_duplicates<
    hana::int_<0>, hana::int_<1>, hana::int_<2>
>::value, "");

static_assert(hana::detail::has_duplicates<
    hana::int_<0>, hana::int_<0>, hana::int_<2>
>::value, "");

static_assert(hana::detail::has_duplicates<
    hana::int_<0>, hana::int_<1>, hana::int_<0>
>::value, "");

static_assert(hana::detail::has_duplicates<
    hana::int_<0>, hana::int_<1>, hana::int_<2>, hana::int_<1>
>::value, "");

static_assert(hana::detail::has_duplicates<
    hana::int_<0>, hana::int_<1>, hana::int_<2>, hana::int_<2>
>::value, "");

static_assert(hana::detail::has_duplicates<
    hana::int_<0>, hana::int_<1>, hana::int_<2>, hana::int_<1>, hana::int_<1>
>::value, "");

static_assert(hana::detail::has_duplicates<
    hana::int_<0>, hana::int_<1>, hana::int_<2>, hana::int_<1>, hana::int_<2>
>::value, "");

// Make sure it uses deep equality
static_assert(hana::detail::has_duplicates<
    hana::int_<0>, hana::long_<0>, hana::int_<2>, hana::int_<3>
>::value, "");

static_assert(hana::detail::has_duplicates<
    hana::int_<0>, hana::int_<1>, hana::int_<2>, hana::long_<1>
>::value, "");

int main() { }
