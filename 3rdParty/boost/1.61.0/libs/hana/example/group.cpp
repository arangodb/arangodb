// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/comparing.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/group.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/length.hpp>
#include <boost/hana/range.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


// group without a predicate
BOOST_HANA_CONSTANT_CHECK(
    hana::group(hana::make_tuple(hana::int_c<1>, hana::long_c<1>, hana::type_c<int>, hana::char_c<'x'>, hana::char_c<'x'>))
        == hana::make_tuple(
            hana::make_tuple(hana::int_c<1>, hana::long_c<1>),
            hana::make_tuple(hana::type_c<int>),
            hana::make_tuple(hana::char_c<'x'>, hana::char_c<'x'>)
        )
);


// group with a predicate
constexpr auto tuples = hana::make_tuple(
    hana::range_c<int, 0, 1>,
    hana::range_c<int, 0, 2>,
    hana::range_c<int, 1, 3>,
    hana::range_c<int, 2, 6>
);

BOOST_HANA_CONSTANT_CHECK(
    hana::group(tuples, hana::comparing(hana::length))
        == hana::make_tuple(
            hana::make_tuple(
                hana::range_c<int, 0, 1>
            ),
            hana::make_tuple(
                hana::range_c<int, 0, 2>,
                hana::range_c<int, 1, 3>
            ),
            hana::make_tuple(
                hana::range_c<int, 2, 6>
            )
        )
);


// group.by is syntactic sugar
static_assert(
    hana::group.by(hana::comparing(hana::decltype_),
                   hana::make_tuple(1, 2, 3, 'x', 'y', 4.4, 5.5))
        == hana::make_tuple(
            hana::make_tuple(1, 2, 3),
            hana::make_tuple('x', 'y'),
            hana::make_tuple(4.4, 5.5)
        )
, "");

int main() { }
