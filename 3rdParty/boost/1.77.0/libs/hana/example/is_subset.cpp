// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/is_subset.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


static_assert(
    hana::is_subset(hana::make_tuple(1, '2', 3.3), hana::make_tuple(3.3, 1, '2', nullptr))
, "");

// is_subset can be applied in infix notation
static_assert(
    hana::make_tuple(1, '2', 3.3) ^hana::is_subset^ hana::make_tuple(3.3, 1, '2', nullptr)
, "");

int main() { }
