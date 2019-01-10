// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/integer_sequence.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>
#include <boost/hana/unpack.hpp>

#include <laws/base.hpp>

#include <type_traits>
#include <utility>
namespace hana = boost::hana;


int main() {
    hana::test::_injection<0> f{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(std::integer_sequence<int>{}, f),
        f()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(std::integer_sequence<int, 0>{}, f),
        f(std::integral_constant<int, 0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(std::integer_sequence<int, 0, 1>{}, f),
        f(std::integral_constant<int, 0>{}, std::integral_constant<int, 1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(std::integer_sequence<int, 0, 1, 2>{}, f),
        f(std::integral_constant<int, 0>{}, std::integral_constant<int, 1>{},
          std::integral_constant<int, 2>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(std::integer_sequence<int, 0, 1, 2, 3>{}, f),
        f(std::integral_constant<int, 0>{}, std::integral_constant<int, 1>{},
          std::integral_constant<int, 2>{}, std::integral_constant<int, 3>{})
    ));
}
