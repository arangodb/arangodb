// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/drop_front.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/experimental/types.hpp>
#include <boost/hana/integral_constant.hpp>
namespace hana = boost::hana;


template <int> struct x;

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<>{}, hana::size_c<0>),
        hana::experimental::types<>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<>{}, hana::size_c<1>),
        hana::experimental::types<>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>>{}, hana::size_c<0>),
        hana::experimental::types<x<0>>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>>{}, hana::size_c<1>),
        hana::experimental::types<>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>>{}, hana::size_c<2>),
        hana::experimental::types<>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>, x<1>>{}, hana::size_c<0>),
        hana::experimental::types<x<0>, x<1>>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>, x<1>>{}, hana::size_c<1>),
        hana::experimental::types<x<1>>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>, x<1>>{}, hana::size_c<2>),
        hana::experimental::types<>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>, x<1>>{}, hana::size_c<3>),
        hana::experimental::types<>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>, x<1>, x<2>>{}, hana::size_c<0>),
        hana::experimental::types<x<0>, x<1>, x<2>>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>, x<1>, x<2>>{}, hana::size_c<1>),
        hana::experimental::types<x<1>, x<2>>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>, x<1>, x<2>>{}, hana::size_c<2>),
        hana::experimental::types<x<2>>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>, x<1>, x<2>>{}, hana::size_c<3>),
        hana::experimental::types<>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>, x<1>, x<2>>{}, hana::size_c<4>),
        hana::experimental::types<>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>, x<1>, x<2>, x<3>>{}, hana::size_c<0>),
        hana::experimental::types<x<0>, x<1>, x<2>, x<3>>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>, x<1>, x<2>, x<3>>{}, hana::size_c<1>),
        hana::experimental::types<x<1>, x<2>, x<3>>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>, x<1>, x<2>, x<3>>{}, hana::size_c<2>),
        hana::experimental::types<x<2>, x<3>>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>, x<1>, x<2>, x<3>>{}, hana::size_c<3>),
        hana::experimental::types<x<3>>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>, x<1>, x<2>, x<3>>{}, hana::size_c<4>),
        hana::experimental::types<>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(hana::experimental::types<x<0>, x<1>, x<2>, x<3>>{}, hana::size_c<5>),
        hana::experimental::types<>{}
    ));
}
