// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/front.hpp>
#include <boost/hana/greater.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/negate.hpp>
#include <boost/hana/ordering.hpp>
#include <boost/hana/sort.hpp>
#include <boost/hana/tuple.hpp>

#include <string>
namespace hana = boost::hana;
using namespace hana::literals;
using namespace std::literals;


// sort without a predicate
BOOST_HANA_CONSTANT_CHECK(
    hana::sort(hana::make_tuple(1_c, -2_c, 3_c, 0_c)) ==
               hana::make_tuple(-2_c, 0_c, 1_c, 3_c)
);

// sort with a predicate
BOOST_HANA_CONSTANT_CHECK(
    hana::sort(hana::make_tuple(1_c, -2_c, 3_c, 0_c), hana::greater) ==
               hana::make_tuple(3_c, 1_c, 0_c, -2_c)
);

int main() {
    // sort.by is syntactic sugar
    auto tuples = hana::make_tuple(
        hana::make_tuple(2_c, 'x', nullptr),
        hana::make_tuple(1_c, "foobar"s, hana::int_c<4>)
    );

    BOOST_HANA_RUNTIME_CHECK(
        hana::sort.by(hana::ordering(hana::front), tuples)
            == hana::make_tuple(
                hana::make_tuple(1_c, "foobar"s, hana::int_c<4>),
                hana::make_tuple(2_c, 'x', nullptr)
            )
    );
}
