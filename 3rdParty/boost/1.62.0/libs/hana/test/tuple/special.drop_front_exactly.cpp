// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/drop_front_exactly.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


struct x0;
struct x1;
struct x2;

int main() {
    // tuple_t
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(hana::tuple_t<x0>),
        hana::tuple_t<>
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(hana::tuple_t<x0, x1>),
        hana::tuple_t<x1>
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(hana::tuple_t<x0, x1, x2>),
        hana::tuple_t<x1, x2>
    ));

    // tuple_c
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(hana::tuple_c<int, 0>),
        hana::tuple_c<int>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(hana::tuple_c<int, 0, 1>),
        hana::tuple_c<int, 1>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(hana::tuple_c<int, 0, 1, 2>),
        hana::tuple_c<int, 1, 2>
    ));
}
