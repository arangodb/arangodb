//  (C) Copyright Raffi Enficiaud 2018.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//! @file
//! Allowing for suites with same name, ticket trac #12597
// *****************************************************************************

#define BOOST_TEST_MODULE test_clashing_names_suites_ok
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE( dummy_suite )
BOOST_AUTO_TEST_CASE( ts1 )
{
    BOOST_CHECK(true);
}
BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_SUITE( dummy_suite )
BOOST_AUTO_TEST_CASE( ts2 )
{
    BOOST_CHECK(true);
}
BOOST_AUTO_TEST_SUITE_END()
