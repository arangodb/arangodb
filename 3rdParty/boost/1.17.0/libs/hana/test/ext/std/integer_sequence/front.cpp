// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/integer_sequence.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>
#include <boost/hana/front.hpp>

#include <type_traits>
#include <utility>
namespace hana = boost::hana;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::front(std::index_sequence<0>{}),
        std::integral_constant<std::size_t, 0>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::front(std::index_sequence<0, 1>{}),
        std::integral_constant<std::size_t, 0>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::front(std::integer_sequence<int, -3, 1, 2, 3, 4>{}),
        std::integral_constant<int, -3>{}
    ));
}
