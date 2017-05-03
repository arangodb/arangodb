// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/if.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


static_assert(hana::if_(true, 1, 2) == 1, "");
static_assert(hana::if_(false, 1, 2) == 2, "");

static_assert(
    hana::if_(hana::true_c,
        hana::make_tuple('t', 'r', 'u', 'e'),
        hana::make_tuple('f', 'a', 'l', 's', 'e')
    )
        ==
    hana::make_tuple('t', 'r', 'u', 'e')
, "");

int main() { }
