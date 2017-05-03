// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/div.hpp>
#include <boost/hana/functional/reverse_partial.hpp>
namespace hana = boost::hana;


constexpr auto half = hana::reverse_partial(hana::div, 2);
static_assert(half(4) == 2, "");
static_assert(half(8) == 4, "");

int main() { }
