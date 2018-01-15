// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/prepend.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::prepend(hana::tuple_c<long>, hana::long_c<0>),
        hana::tuple_c<long, 0>
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::prepend(hana::tuple_c<unsigned int, 1>, hana::uint_c<0>),
        hana::tuple_c<unsigned int, 0, 1>
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::prepend(hana::tuple_c<long long, 1, 2>, hana::llong_c<0>),
        hana::tuple_c<long long, 0, 1, 2>
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::prepend(hana::tuple_c<unsigned long, 1, 2, 3>, hana::ulong_c<0>),
        hana::tuple_c<unsigned long, 0, 1, 2, 3>
    ));
}
