// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/experimental/types.hpp>
#include <boost/hana/not.hpp>
namespace hana = boost::hana;


template <int> struct x;

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::experimental::types<>{},
        hana::experimental::types<>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::experimental::types<x<0>>{},
        hana::experimental::types<x<0>>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::experimental::types<x<0>, x<1>, x<2>>{},
        hana::experimental::types<x<0>, x<1>, x<2>>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        hana::experimental::types<>{},
        hana::experimental::types<x<0>>{}
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        hana::experimental::types<x<0>>{},
        hana::experimental::types<>{}
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        hana::experimental::types<x<0>>{},
        hana::experimental::types<x<1>>{}
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        hana::experimental::types<x<0>, x<1>, x<2>>{},
        hana::experimental::types<x<0>, x<1>, x<3>>{}
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        hana::experimental::types<x<0>, x<1>, x<2>>{},
        hana::experimental::types<x<0>, x<1>>{}
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        hana::experimental::types<x<0>, x<1>, x<2>>{},
        hana::experimental::types<x<0>, x<9>, x<2>>{}
    )));
}
