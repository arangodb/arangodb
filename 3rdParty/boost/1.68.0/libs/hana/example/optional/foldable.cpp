// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/fold_right.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/plus.hpp>
namespace hana = boost::hana;


static_assert(hana::fold_right(hana::nothing, 1, hana::plus) == 1, "");
static_assert(hana::fold_right(hana::just(4), 1, hana::plus) == 5, "");

int main() { }
