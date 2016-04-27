//  (C) Copyright Gennadiy Rozental 2005-2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at 
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

// Boost.Test
#define BOOST_TEST_MODULE Unit_test_example_04 
#include <boost/test/unit_test.hpp>
namespace bt=boost::unit_test;

//____________________________________________________________________________//

struct suite_fixture {
    suite_fixture()     { BOOST_TEST_MESSAGE( "Running some test suite setup" ); }
    ~suite_fixture()    { BOOST_TEST_MESSAGE( "Running some test suite teardown" ); }
};
struct suite_fixture2 {
    suite_fixture2()    { BOOST_TEST_MESSAGE( "Running some more test suite setup" ); }
    ~suite_fixture2()   { BOOST_TEST_MESSAGE( "Running some more test suite teardown" ); }
};

// automatically registered test cases could be organized in test suites
BOOST_AUTO_TEST_SUITE( my_suite1, 
* bt::fixture<suite_fixture>()
* bt::fixture<suite_fixture2>() )

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(my_test1,1);

void some_setup()
{
    BOOST_TEST_MESSAGE( "Running some extra setup" );
}

auto& d =
* bt::label( "L1" )
* bt::label( "L2" )
* bt::description( "test case 1 description" )
* bt::fixture( &some_setup );
BOOST_AUTO_TEST_CASE( my_test1 )
{
    BOOST_TEST( 2 == 1 );
}

//____________________________________________________________________________//

// this test case belongs to suite1 test suite
BOOST_AUTO_TEST_CASE( my_test2, * bt::description( "test case 2 description" ) )
{
    int i = 0;

    BOOST_TEST( i == 2 );

    BOOST_TEST( i == 0 );
}

BOOST_AUTO_TEST_SUITE_END()

//____________________________________________________________________________//

// this test case belongs to master test suite
BOOST_AUTO_TEST_CASE( my_test3 )
{
    int i = 0;

    BOOST_TEST( i == 0 );
}

//____________________________________________________________________________//

BOOST_TEST_DECORATOR(
* bt::label( "L3" )
* bt::description( "suite description" )
)
BOOST_AUTO_TEST_SUITE( my_suite2 )

// this test case belongs to suite2 test suite
BOOST_AUTO_TEST_CASE( my_test4, * bt::depends_on( "my_suite2/internal_suite/my_test5" ) )
{
    int i = 0;

    BOOST_CHECK_EQUAL( i, 1 );
}

BOOST_AUTO_TEST_SUITE( internal_suite )

// this test case belongs to my_suite2:internal_suite test suite

BOOST_TEST_DECORATOR(
* bt::timeout( 100 )
)
BOOST_AUTO_TEST_CASE( my_test5, * bt::expected_failures( 1 ) )
{
    int i = 0;

    BOOST_CHECK_EQUAL( i, 1 );
}

BOOST_AUTO_TEST_CASE( my_test6, *bt::disabled() )
{
}

BOOST_AUTO_TEST_CASE( this_should_also_be_disabled, 
* bt::depends_on( "my_suite2/internal_suite/disabled_suite/my_test7" ) )
{
}

BOOST_AUTO_TEST_SUITE( disabled_suite, * bt::disabled() )

BOOST_AUTO_TEST_CASE( my_test7 ) {}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

//____________________________________________________________________________//

// EOF
