// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/equal.hpp>
#include <boost/hana/mult.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/zip_shortest_with.hpp>
namespace hana = boost::hana;


static_assert(
    hana::zip_shortest_with(hana::mult, hana::make_tuple(1, 2, 3, 4), hana::make_tuple(5, 6, 7, 8, "ignored"))
    ==
    hana::make_tuple(5, 12, 21, 32)
, "");


int main() { }
