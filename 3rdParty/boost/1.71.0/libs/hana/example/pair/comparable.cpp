// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/equal.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/pair.hpp>
namespace hana = boost::hana;


static_assert(hana::make_pair(1, 'x') == hana::make_pair(1, 'x'), "");
static_assert(hana::make_pair(2, 'x') != hana::make_pair(1, 'x'), "");
static_assert(hana::make_pair(1, 'y') != hana::make_pair(1, 'x'), "");

int main() { }
