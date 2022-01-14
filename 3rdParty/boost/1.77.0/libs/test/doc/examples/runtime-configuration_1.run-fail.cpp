//  Copyright (c) 2018 Raffi Enficiaud
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE runtime_configuration1
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test;

BOOST_AUTO_TEST_CASE(test_accessing_command_line)
{
    BOOST_TEST_REQUIRE( framework::master_test_suite().argc == 3 );
    BOOST_TEST( framework::master_test_suite().argv[1] == "--specific-param" );
    BOOST_TEST( framework::master_test_suite().argv[2] == "'additional value with quotes'" );
    BOOST_TEST_MESSAGE( "'argv[0]' contains " << framework::master_test_suite().argv[0] );
}
//]
