// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/functional/partial.hpp>
#include <boost/hana/plus.hpp>
namespace hana = boost::hana;


constexpr auto increment = hana::partial(hana::plus, 1);
static_assert(increment(2) == 3, "");

int main() { }
