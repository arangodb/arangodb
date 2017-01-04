// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/greater.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/minimum.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


int main() {
    // without a predicate
    BOOST_HANA_CONSTANT_CHECK(
        hana::minimum(hana::tuple_c<int, -1, 0, 2, -4, 6, 9>) == hana::int_c<-4>
    );

    // with a predicate
    auto largest = hana::minimum(hana::tuple_c<int, -1, 0, 2, -4, 6, 9>, [](auto x, auto y) {
        return x > y; // order is reversed!
    });
    BOOST_HANA_CONSTANT_CHECK(largest == hana::int_c<9>);
}
