// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_IS_EMPTY_HPP
#define BOOST_HANA_TEST_AUTO_IS_EMPTY_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/is_empty.hpp>
#include <boost/hana/not.hpp>

#include "test_case.hpp"


namespace _test_is_empty_detail { template <int i> struct undefined { }; }

TestCase test_is_empty{[]{
    namespace hana = boost::hana;
    using _test_is_empty_detail::undefined;

    BOOST_HANA_CONSTANT_CHECK(hana::is_empty(
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(
        MAKE_TUPLE(undefined<0>{})
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(
        MAKE_TUPLE(undefined<0>{}, undefined<1>{})
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(
        MAKE_TUPLE(undefined<0>{}, undefined<1>{}, undefined<2>{})
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(
        MAKE_TUPLE(undefined<0>{}, undefined<1>{}, undefined<2>{}, undefined<3>{})
    )));

    // Check with a runtime value
    {
        int i = 3; // <- runtime value
        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(MAKE_TUPLE(i))));
        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(MAKE_TUPLE(i, i))));
    }

#ifndef MAKE_TUPLE_NO_CONSTEXPR
    static_assert(hana::is_empty(MAKE_TUPLE()), "");
    static_assert(hana::not_(hana::is_empty(MAKE_TUPLE(undefined<0>{}))), "");
    static_assert(hana::not_(hana::is_empty(MAKE_TUPLE(undefined<0>{}, undefined<1>{}))), "");
#endif
}};

#endif // !BOOST_HANA_TEST_AUTO_IS_EMPTY_HPP
