// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/core/make.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/range.hpp>

#include <support/cnumeric.hpp>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::contains(
        hana::make_range(hana::int_c<0>, hana::int_c<0>),
        cnumeric<int, 0>
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        hana::make_range(hana::int_c<0>, hana::int_c<1>),
        cnumeric<int, 0>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        hana::make_range(hana::int_c<0>, hana::int_c<10>),
        cnumeric<int, 3>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        hana::make_range(hana::int_c<0>, hana::int_c<10>),
        cnumeric<int, 9>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        hana::make_range(hana::int_c<-10>, hana::int_c<10>),
        cnumeric<int, -10>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        hana::make_range(hana::int_c<-10>, hana::int_c<10>),
        cnumeric<int, -5>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::contains(
        hana::make_range(hana::int_c<-10>, hana::int_c<0>),
        cnumeric<int, 3>
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::contains(
        hana::make_range(hana::int_c<0>, hana::int_c<10>),
        cnumeric<int, 15>
    )));
}
