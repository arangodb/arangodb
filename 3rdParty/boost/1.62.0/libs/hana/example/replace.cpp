// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/equal.hpp>
#include <boost/hana/replace.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


static_assert(
    hana::replace(hana::make_tuple(1, 1, 1, 2, 3, 1, 4, 5), 1, 0)
            ==
    hana::make_tuple(0, 0, 0, 2, 3, 0, 4, 5)
, "");

int main() { }
