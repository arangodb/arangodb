// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTANT_CHECK(hana::int_c<2> ^hana::in^ hana::make_tuple(2, hana::int_c<2>, hana::int_c<3>, 'x'));

int main() { }
