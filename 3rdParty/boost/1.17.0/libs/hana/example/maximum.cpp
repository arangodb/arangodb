// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/greater.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/maximum.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


int main() {
    // without a predicate
    BOOST_HANA_CONSTANT_CHECK(
        hana::maximum(hana::tuple_c<int, -1, 0, 2, -4, 6, 9>) == hana::int_c<9>
    );

    // with a predicate
    auto smallest = hana::maximum(hana::tuple_c<int, -1, 0, 2, -4, 6, 9>, [](auto x, auto y) {
        return x > y; // order is reversed!
    });
    BOOST_HANA_CONSTANT_CHECK(smallest == hana::int_c<-4>);
}
