// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTANT_CHECK(hana::contains(hana::make_tuple(2, hana::int_c<2>, hana::int_c<3>, 'x'), hana::int_c<3>));

BOOST_HANA_CONSTANT_CHECK(hana::contains(hana::make_set(hana::int_c<3>, hana::type_c<void>), hana::type_c<void>));

// contains can be applied in infix notation
BOOST_HANA_CONSTANT_CHECK(
    hana::make_tuple(2, hana::int_c<2>, hana::int_c<3>, 'x') ^hana::contains^ hana::int_c<2>
);

int main() { }
