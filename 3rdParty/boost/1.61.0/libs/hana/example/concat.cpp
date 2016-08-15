// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/concat.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;
using namespace hana::literals;


static_assert(
    hana::concat(hana::make_tuple(1, '2'), hana::make_tuple(3.3, 4_c))
        ==
    hana::make_tuple(1, '2', 3.3, 4_c)
, "");

int main() { }
