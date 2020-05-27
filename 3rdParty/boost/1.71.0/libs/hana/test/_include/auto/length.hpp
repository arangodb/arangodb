// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_LENGTH_HPP
#define BOOST_HANA_TEST_AUTO_LENGTH_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/length.hpp>

#include "test_case.hpp"


namespace _test_length_detail { template <int> struct undefined { }; }

TestCase test_length{[]{
    namespace hana = boost::hana;
    using _test_length_detail::undefined;

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::length(MAKE_TUPLE()),
        hana::size_c<0>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::length(MAKE_TUPLE(undefined<1>{})),
        hana::size_c<1>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::length(MAKE_TUPLE(undefined<1>{}, undefined<2>{})),
        hana::size_c<2>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::length(MAKE_TUPLE(undefined<1>{}, undefined<2>{}, undefined<3>{})),
        hana::size_c<3>
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::length(MAKE_TUPLE(undefined<1>{}, undefined<2>{}, undefined<3>{}, undefined<4>{}, undefined<5>{}, undefined<6>{})),
        hana::size_c<6>
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_LENGTH_HPP
