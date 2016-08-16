// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/any_of.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/concept/struct.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/not.hpp>

#include "minimal_struct.hpp"
namespace hana = boost::hana;


template <int i = 0>
struct undefined { };

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::any_of(
        obj(),
        undefined<>{}
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::any_of(
        obj(undefined<0>{}),
        hana::equal.to(hana::int_c<0>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::any_of(
        obj(undefined<0>{}),
        hana::equal.to(hana::int_c<1>)
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::any_of(
        obj(undefined<0>{}, undefined<1>{}),
        hana::equal.to(hana::int_c<0>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::any_of(
        obj(undefined<0>{}, undefined<1>{}),
        hana::equal.to(hana::int_c<1>)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::any_of(
        obj(undefined<0>{}, undefined<1>{}),
        hana::equal.to(hana::int_c<2>)
    )));
}
