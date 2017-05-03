// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/take_while.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;
using namespace hana::literals;


BOOST_HANA_CONSTANT_CHECK(
    hana::take_while(hana::tuple_c<int, 0, 1, 2, 3>, hana::less.than(2_c))
    ==
    hana::tuple_c<int, 0, 1>
);

int main() { }
