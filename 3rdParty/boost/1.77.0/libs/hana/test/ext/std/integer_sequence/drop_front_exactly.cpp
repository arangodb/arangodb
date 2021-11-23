// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/drop_front_exactly.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/integer_sequence.hpp>
#include <boost/hana/integral_constant.hpp>

#include <utility>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(std::index_sequence<0>{}),
        std::index_sequence<>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(std::index_sequence<0, 1>{}),
        std::index_sequence<1>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(std::index_sequence<0, 1, 2>{}),
        std::index_sequence<1, 2>{}
    ));


    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(std::index_sequence<0, 1, 2>{}, hana::size_c<2>),
        std::index_sequence<2>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(std::index_sequence<0, 1, 2, 3>{}, hana::size_c<2>),
        std::index_sequence<2, 3>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(std::index_sequence<0, 1, 2, 3>{}, hana::size_c<3>),
        std::index_sequence<3>{}
    ));
}
