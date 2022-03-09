// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/fold_left.hpp>
#include <boost/hana/fold_right.hpp>
#include <boost/hana/minus.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/plus.hpp>
namespace hana = boost::hana;


static_assert(hana::fold_left(hana::make_pair(1, 3), 0, hana::plus) == 4, "");
static_assert(hana::fold_right(hana::make_pair(1, 3), 0, hana::minus) == -2, "");

int main() { }
