//  Unit test for boost::lexical_cast for https://svn.boost.org/trac/boost/ticket/11669.
//
//  See http://www.boost.org for most recent version, including documentation.
//
//  Copyright Antony Polukhin, 2015-2019.
//
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt).

#include <string>
#define BOOST_LCAST_NO_COMPILE_TIME_PRECISION
#include <boost/lexical_cast.hpp>

#include <boost/test/unit_test.hpp>

void main_check() {
    BOOST_CHECK(!boost::lexical_cast<std::string>(2.12345).empty());
    BOOST_CHECK(!boost::lexical_cast<std::string>(2.12345678).empty());
}

boost::unit_test::test_suite *init_unit_test_suite(int, char *[])
{
    boost::unit_test::test_suite *suite =
        BOOST_TEST_SUITE("lexical_cast no-compile-time-precision check");
    suite->add(BOOST_TEST_CASE(main_check));
    return suite;
}
