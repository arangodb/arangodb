// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/front.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


struct x0;
struct x1;
struct x2;

int main() {
    // tuple_t
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::front(hana::tuple_t<x0>),
        hana::type_c<x0>
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::front(hana::tuple_t<x0, x1>),
        hana::type_c<x0>
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::front(hana::tuple_t<x0, x1, x2>),
        hana::type_c<x0>
    ));

    // tuple_c
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::front(hana::tuple_c<int, 0>),
        hana::int_c<0>
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::front(hana::tuple_c<int, 0, 1>),
        hana::int_c<0>
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::front(hana::tuple_c<int, 0, 1, 2>),
        hana::int_c<0>
    ));
}
