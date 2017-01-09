// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/struct.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/keys.hpp>

#include "minimal_struct.hpp"
#include <laws/base.hpp>
#include <support/seq.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


template <int i = 0>
struct undefined { };

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::keys(obj()),
        ::seq()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::keys(obj(undefined<0>{})),
        ::seq(hana::int_c<0>)
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::keys(obj(undefined<0>{}, undefined<1>{})),
        ::seq(hana::int_c<0>, hana::int_c<1>)
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::keys(obj(undefined<0>{}, undefined<1>{}, undefined<2>{})),
        ::seq(hana::int_c<0>, hana::int_c<1>, hana::int_c<2>)
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::keys(obj(undefined<0>{}, undefined<1>{}, undefined<2>{}, undefined<3>{})),
        ::seq(hana::int_c<0>, hana::int_c<1>, hana::int_c<2>, hana::int_c<3>)
    ));
}
