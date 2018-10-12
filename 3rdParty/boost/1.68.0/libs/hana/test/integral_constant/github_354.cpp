// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/core/to.hpp>
#include <boost/hana/integral_constant.hpp>
namespace hana = boost::hana;


static_assert(hana::is_convertible<hana::bool_<true>, bool>::value, "");
static_assert(hana::to<bool>(hana::bool_c<true>) == true, "");

static_assert(hana::is_convertible<hana::integral_constant<int, 1>, int>::value, "");
static_assert(hana::to<int>(hana::integral_c<int, 1>) == 1, "");

static_assert(hana::is_convertible<hana::integral_constant<long, 1l>, long>::value, "");
static_assert(hana::to<long>(hana::integral_c<long, 1l>) == 1l, "");

int main() { }
