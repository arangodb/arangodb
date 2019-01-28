// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/all_of.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/find_if.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/mod.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/optional.hpp>
namespace hana = boost::hana;


auto odd = [](auto x) {
    return x % hana::int_c<2> != hana::int_c<0>;
};

BOOST_HANA_CONSTANT_CHECK(hana::find_if(hana::just(hana::int_c<3>), odd) == hana::just(hana::int_c<3>));
BOOST_HANA_CONSTANT_CHECK(hana::find_if(hana::just(hana::int_c<2>), odd) == hana::nothing);
BOOST_HANA_CONSTANT_CHECK(hana::find_if(hana::nothing, odd) == hana::nothing);

BOOST_HANA_CONSTANT_CHECK(hana::all_of(hana::just(hana::int_c<3>), odd));
BOOST_HANA_CONSTANT_CHECK(hana::all_of(hana::nothing, odd));

int main() { }
