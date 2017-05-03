// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/integral_constant.hpp>

#include <boost/hana/and.hpp>
#include <boost/hana/div.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/greater.hpp>
#include <boost/hana/greater_equal.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/less_equal.hpp>
#include <boost/hana/minus.hpp>
#include <boost/hana/mod.hpp>
#include <boost/hana/mult.hpp>
#include <boost/hana/negate.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/or.hpp>
#include <boost/hana/plus.hpp>
namespace hana = boost::hana;


// Arithmetic operators
BOOST_HANA_CONSTANT_CHECK(+hana::int_c<1> == hana::int_c<1>);
BOOST_HANA_CONSTANT_CHECK(-hana::int_c<1> == hana::int_c<-1>);
BOOST_HANA_CONSTANT_CHECK(hana::int_c<1> + hana::int_c<2> == hana::int_c<3>);
BOOST_HANA_CONSTANT_CHECK(hana::int_c<1> - hana::int_c<2> == hana::int_c<-1>);
BOOST_HANA_CONSTANT_CHECK(hana::int_c<3> * hana::int_c<2> == hana::int_c<6>);
BOOST_HANA_CONSTANT_CHECK(hana::int_c<6> / hana::int_c<3> == hana::int_c<2>);
BOOST_HANA_CONSTANT_CHECK(hana::int_c<6> % hana::int_c<4> == hana::int_c<2>);
BOOST_HANA_CONSTANT_CHECK(~hana::int_c<6> == hana::int_c<~6>);
BOOST_HANA_CONSTANT_CHECK((hana::int_c<6> & hana::int_c<3>) == hana::int_c<6 & 3>);
BOOST_HANA_CONSTANT_CHECK((hana::int_c<4> | hana::int_c<2>) == hana::int_c<4 | 2>);
BOOST_HANA_CONSTANT_CHECK((hana::int_c<6> ^ hana::int_c<3>) == hana::int_c<6 ^ 3>);
BOOST_HANA_CONSTANT_CHECK((hana::int_c<6> << hana::int_c<3>) == hana::int_c<(6 << 3)>);
BOOST_HANA_CONSTANT_CHECK((hana::int_c<6> >> hana::int_c<3>) == hana::int_c<(6 >> 3)>);

// Comparison operators
BOOST_HANA_CONSTANT_CHECK(hana::int_c<0> == hana::int_c<0>);
BOOST_HANA_CONSTANT_CHECK(hana::int_c<1> != hana::int_c<0>);
BOOST_HANA_CONSTANT_CHECK(hana::int_c<0> < hana::int_c<1>);
BOOST_HANA_CONSTANT_CHECK(hana::int_c<0> <= hana::int_c<1>);
BOOST_HANA_CONSTANT_CHECK(hana::int_c<0> <= hana::int_c<0>);
BOOST_HANA_CONSTANT_CHECK(hana::int_c<1> > hana::int_c<0>);
BOOST_HANA_CONSTANT_CHECK(hana::int_c<1> >= hana::int_c<0>);
BOOST_HANA_CONSTANT_CHECK(hana::int_c<0> >= hana::int_c<0>);

// Logical operators
BOOST_HANA_CONSTANT_CHECK(hana::int_c<3> || hana::int_c<0>);
BOOST_HANA_CONSTANT_CHECK(hana::int_c<3> && hana::int_c<1>);
BOOST_HANA_CONSTANT_CHECK(!hana::int_c<0>);
BOOST_HANA_CONSTANT_CHECK(!!hana::int_c<3>);

int main() { }
