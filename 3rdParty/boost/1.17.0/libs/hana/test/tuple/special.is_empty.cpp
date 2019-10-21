// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/is_empty.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


struct x0;
struct x1;
struct x2;

int main() {
    // tuple_t
    BOOST_HANA_CONSTANT_CHECK(hana::is_empty(hana::tuple_t<>));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(hana::tuple_t<x0>)));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(hana::tuple_t<x0, x1>)));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(hana::tuple_t<x0, x1, x2>)));

    // tuple_c
    BOOST_HANA_CONSTANT_CHECK(hana::is_empty(hana::tuple_c<int>));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(hana::tuple_c<int, 0>)));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(hana::tuple_c<int, 0, 1>)));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(hana::tuple_c<int, 0, 1, 2>)));
}
