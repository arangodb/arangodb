// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/span.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


constexpr auto xs = hana::make_tuple(hana::int_c<1>, hana::int_c<2>, hana::int_c<3>, hana::int_c<4>);

BOOST_HANA_CONSTANT_CHECK(
    hana::span(xs, hana::less.than(hana::int_c<3>))
    ==
    hana::make_pair(hana::make_tuple(hana::int_c<1>, hana::int_c<2>),
                    hana::make_tuple(hana::int_c<3>, hana::int_c<4>))
);

BOOST_HANA_CONSTANT_CHECK(
    hana::span(xs, hana::less.than(hana::int_c<0>))
    ==
    hana::make_pair(hana::make_tuple(), xs)
);

BOOST_HANA_CONSTANT_CHECK(
    hana::span(xs, hana::less.than(hana::int_c<5>))
    ==
    hana::make_pair(xs, hana::make_tuple())
);

// span.by is syntactic sugar
BOOST_HANA_CONSTANT_CHECK(
    hana::span.by(hana::less.than(hana::int_c<3>), xs)
    ==
    hana::make_pair(hana::make_tuple(hana::int_c<1>, hana::int_c<2>),
                    hana::make_tuple(hana::int_c<3>, hana::int_c<4>))
);

int main() { }
