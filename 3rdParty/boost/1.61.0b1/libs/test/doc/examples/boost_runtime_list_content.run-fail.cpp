//  (C) Copyright 2015 Boost.Test team.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE list_content
#include <boost/test/included/unit_test.hpp>
namespace utf=boost::unit_test;

//// --------------------------------------------------------------------------
// Test suite 1, disabled by default, s1/test2 is explicitely enabled.
BOOST_AUTO_TEST_SUITE( s1,
* utf::disabled()                         // suite is not disabled because of the
* utf::description( "disabled suite 1")   // extra declaration at the end of the file
* utf::label( "label1" )
* utf::label( "label2" ))

BOOST_AUTO_TEST_CASE( test1, // s1/test1
* utf::enabled() * utf::description("enabled"))
{
    BOOST_TEST(true);
}

BOOST_AUTO_TEST_CASE( test2, // s1/test2
* utf::description( "defaulted") * utf::expected_failures( 1 ))
{
    BOOST_TEST(false);
}

BOOST_AUTO_TEST_CASE( test3, // s1/test3
* utf::description( "defaulted"))
{
    BOOST_TEST(false);
}
BOOST_AUTO_TEST_SUITE_END()


//// --------------------------------------------------------------------------
// Test suite 2, disabled by default, s1/test2 is explicitely enabled.
BOOST_AUTO_TEST_SUITE( s2,
* utf::disabled() 
* utf::label( "label1" ) 
* utf::expected_failures( 3 ))

BOOST_AUTO_TEST_CASE( test1, // s2/test1
* utf::description( "defaulted"))
{
    BOOST_TEST(false);
}

boost::test_tools::assertion_result do_it( utf::test_unit_id )
{
   return false;
}

BOOST_AUTO_TEST_CASE( test2, // s2/test2
* utf::enabled() 
* utf::description( "enabled w. precondition")
* utf::precondition(do_it))
{
  BOOST_TEST(false);
}

//// --------------------------------------------------------------------------
// Test suite s2/s23, disabled
BOOST_AUTO_TEST_SUITE( s23, * utf::disabled())

BOOST_AUTO_TEST_CASE( test1 ) // s2/s23/test1
{
  BOOST_TEST(false);
}

BOOST_AUTO_TEST_CASE( test2, // s2/s23/test2
* utf::timeout( 10 ))
{
  BOOST_TEST( true );
}

BOOST_AUTO_TEST_CASE( test3, // s2/s23/test3
* utf::enabled() 
* utf::depends_on( "s2/test2" ))
{
  BOOST_TEST( true );
}
BOOST_AUTO_TEST_SUITE_END() // s2/s23
BOOST_AUTO_TEST_SUITE_END() // s2



//// --------------------------------------------------------------------------
// Test suite s1 continued
BOOST_AUTO_TEST_SUITE( s1 )
BOOST_AUTO_TEST_SUITE( s14,
* utf::depends_on( "s2/s23/test3" )
* utf::description( "test suite which depends on another test suite"))

BOOST_AUTO_TEST_CASE( test1, // s1/s14/test1
* utf::depends_on( "s2" ))
{
    BOOST_TEST( "s14" == "test" );
}

BOOST_AUTO_TEST_SUITE_END() // s1/s14
BOOST_AUTO_TEST_SUITE_END() // s1
//]