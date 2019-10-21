// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/experimental/types.hpp>
#include <boost/hana/is_empty.hpp>
#include <boost/hana/not.hpp>
namespace hana = boost::hana;


template <int> struct x;

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::is_empty(
        hana::experimental::types<>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(
        hana::experimental::types<x<0>>{}
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(
        hana::experimental::types<x<0>, x<1>>{}
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(
        hana::experimental::types<x<0>, x<1>, x<2>>{}
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(
        hana::experimental::types<x<0>, x<1>, x<2>, x<3>>{}
    )));
}
