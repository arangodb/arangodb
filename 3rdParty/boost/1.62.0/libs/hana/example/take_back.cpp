// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/take_back.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTANT_CHECK(hana::take_back(hana::make_tuple(1, '2', 3.3), hana::size_c<0>) == hana::make_tuple());
static_assert(hana::take_back(hana::make_tuple(1, '2', 3.3), hana::size_c<1>) == hana::make_tuple(3.3), "");
static_assert(hana::take_back(hana::make_tuple(1, '2', 3.3), hana::size_c<2>) == hana::make_tuple('2', 3.3), "");
static_assert(hana::take_back(hana::make_tuple(1, '2', 3.3), hana::size_c<3>) == hana::make_tuple(1, '2', 3.3), "");
static_assert(hana::take_back(hana::make_tuple(1, '2', 3.3), hana::size_c<4>) == hana::make_tuple(1, '2', 3.3), "");

int main() { }
