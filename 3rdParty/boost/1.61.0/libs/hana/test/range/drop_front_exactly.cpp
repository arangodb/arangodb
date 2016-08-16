// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/make.hpp>
#include <boost/hana/drop_front_exactly.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/range.hpp>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(hana::make_range(hana::int_c<0>, hana::int_c<0>), hana::int_c<0>),
        hana::make_range(hana::int_c<0>, hana::int_c<0>)
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(hana::make_range(hana::int_c<0>, hana::int_c<1>), hana::int_c<0>),
        hana::make_range(hana::int_c<0>, hana::int_c<1>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(hana::make_range(hana::int_c<0>, hana::int_c<1>), hana::int_c<1>),
        hana::make_range(hana::int_c<1>, hana::int_c<1>)
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(hana::make_range(hana::int_c<0>, hana::int_c<2>), hana::int_c<0>),
        hana::make_range(hana::int_c<0>, hana::int_c<2>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(hana::make_range(hana::int_c<0>, hana::int_c<2>), hana::int_c<1>),
        hana::make_range(hana::int_c<1>, hana::int_c<2>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(hana::make_range(hana::int_c<0>, hana::int_c<2>), hana::int_c<2>),
        hana::make_range(hana::int_c<2>, hana::int_c<2>)
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(hana::make_range(hana::int_c<0>, hana::int_c<3>), hana::int_c<0>),
        hana::make_range(hana::int_c<0>, hana::int_c<3>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(hana::make_range(hana::int_c<0>, hana::int_c<3>), hana::int_c<1>),
        hana::make_range(hana::int_c<1>, hana::int_c<3>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(hana::make_range(hana::int_c<0>, hana::int_c<3>), hana::int_c<2>),
        hana::make_range(hana::int_c<2>, hana::int_c<3>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(hana::make_range(hana::int_c<0>, hana::int_c<3>), hana::int_c<3>),
        hana::make_range(hana::int_c<3>, hana::int_c<3>)
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(hana::make_range(hana::int_c<20>, hana::int_c<50>), hana::int_c<10>),
        hana::make_range(hana::int_c<30>, hana::int_c<50>)
    ));
}
