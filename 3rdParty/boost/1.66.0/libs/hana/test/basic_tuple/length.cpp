// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/basic_tuple.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/length.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::length(hana::make_basic_tuple()),
        hana::size_c<0>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::length(hana::make_basic_tuple(ct_eq<0>{})),
        hana::size_c<1>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::length(hana::make_basic_tuple(ct_eq<0>{}, ct_eq<1>{})),
        hana::size_c<2>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::length(hana::make_basic_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
        hana::size_c<3>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::length(hana::make_basic_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})),
        hana::size_c<4>
    ));
}
