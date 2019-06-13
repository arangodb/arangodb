// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include "minimal.hpp"

#include <boost/hana/concept/constant.hpp>
#include <boost/hana/core/to.hpp>
namespace hana = boost::hana;


static_assert(hana::is_convertible<minimal_constant_tag<bool>, bool>::value, "");
static_assert(hana::to<bool>(minimal_constant<bool, true>{}) == true, "");

static_assert(hana::is_convertible<minimal_constant_tag<int>, int>::value, "");
static_assert(hana::to<int>(minimal_constant<int, 1>{}) == 1, "");

static_assert(hana::is_convertible<minimal_constant_tag<long>, long>::value, "");
static_assert(hana::to<long>(minimal_constant<long, 1>{}) == 1l, "");

int main() { }
