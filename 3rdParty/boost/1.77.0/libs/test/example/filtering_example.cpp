//  (C) Copyright Gennadiy Rozental 2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at 
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

#define BOOST_TEST_MODULE filtering test
#include <boost/test/unit_test.hpp>
namespace bt=boost::unit_test;

const std::string test1v("test1");
const std::string test2v("test2");
const std::string test3v("test3");

//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE( s1,
* bt::disabled()
* bt::description( "initially disabled suite 1")
* bt::label( "label1" )
* bt::label( "label2" ))

BOOST_AUTO_TEST_CASE( test1,
* bt::enabled()
* bt::description("initially enabled case s1/t1"))
{
    BOOST_TEST( "s1" == test1v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test2,
* bt::description( "initially defaulted case s1/t2")
* bt::expected_failures( 1 ))
{
    BOOST_TEST( "s1" == test2v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test3,
* bt::description( "initially defaulted case s1/t3"))
{
    BOOST_TEST( "s1" == test3v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( s2,
* bt::disabled()
* bt::label( "label1" )
* bt::expected_failures( 3 ))

BOOST_AUTO_TEST_CASE( test1,
* bt::description( "initially defaulted case s2/t1"))
{
    BOOST_TEST( "s2" == test1v );
}

//____________________________________________________________________________//

boost::test_tools::assertion_result
do_it( bt::test_unit_id )
{
   return false;
}

BOOST_AUTO_TEST_CASE( test2,
* bt::enabled()
* bt::description( "initially enabled case s2/t2")
* bt::precondition(do_it))
{
    BOOST_TEST( "s2" == test2v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test3,
* bt::description( "initially defaulted case s2/t3"))
{
    BOOST_TEST( "s2" == test3v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( s3,
* bt::disabled())

BOOST_AUTO_TEST_CASE( test1 )
{
    BOOST_TEST( "s3" == test1v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test2,
* bt::timeout( 10 ))
{
    BOOST_TEST( "s3" == test2v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test3,
* bt::enabled())
{
    BOOST_TEST( "s3" == test3v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE( s14,
* bt::depends_on( "s3/s15" )
* bt::description( "test suite which depends on another test suite"))

BOOST_AUTO_TEST_CASE( test1,
* bt::depends_on( "s2" ))
{
    BOOST_TEST( "s14" == test1v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( s15 )

BOOST_AUTO_TEST_CASE( test1 )
{
    BOOST_TEST( "s15" == test1v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( s4 )

BOOST_AUTO_TEST_CASE( test1,
* bt::disabled()
* bt::label( "label2" ))
{
    BOOST_TEST( "s4" == test1v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test2,
* bt::depends_on( "s4/test1" )
* bt::description( "test case which depends on disabled s4/t1"))
{
    BOOST_TEST( "s4" == test2v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test3,
* bt::depends_on( "s4/test2" )
* bt::description( "test case which depends on enabled s4/t2, but indirectly on disabled s4/t1"))
{
    BOOST_TEST( "s4" == test3v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE_END()

#if 0
BOOST_AUTO_TEST_SUITE( s5 )

BOOST_AUTO_TEST_CASE( test1,
* bt::depends_on( "s5/test3" ))
{
    BOOST_TEST( "s5" == test1v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test2,
* bt::depends_on( "s5/test1" ))
{
    BOOST_TEST( "s5" == test2v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test3,
* bt::depends_on( "s5/test2" ))
{
    BOOST_TEST( "s5" == test3v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE_END()
#endif

BOOST_AUTO_TEST_SUITE( s6 )

BOOST_AUTO_TEST_CASE( test1,
* bt::depends_on( "s6/test3" )
* bt::description( "test case which depends on enabled s6/t3"))
{
    BOOST_TEST( "s6" == test1v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test2,
* bt::depends_on( "s6/test1" )
* bt::description( "test case which depends on enabled s6/t1"))
{
    BOOST_TEST( "s6" == test2v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test3 )
{
    BOOST_TEST( "s6" == test3v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE( s9,
* bt::description( "test suite with all test cases disabled"))

BOOST_AUTO_TEST_CASE( test1,
* bt::disabled()
* bt::label( "label1" ))
{
    BOOST_TEST( "s9" == test1v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test2,
* bt::disabled())
{
    BOOST_TEST( "s9" == test2v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

#if 0
BOOST_AUTO_TEST_SUITE( s7 )

BOOST_AUTO_TEST_CASE( test1,
* bt::depends_on( "s8/test1" ))
{
    BOOST_TEST( "s7" == test1v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test2,
* bt::depends_on( "s7/test1" ))
{
    BOOST_TEST( "s7" == test2v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( s8 )

BOOST_AUTO_TEST_CASE( test1,
* bt::depends_on( "s8/test2" ))
{
    BOOST_TEST( "s8" == test1v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test2,
* bt::depends_on( "s7/test2" ))
{
    BOOST_TEST( "s8" == test2v );
}

//____________________________________________________________________________//
`
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( s10 )

BOOST_AUTO_TEST_CASE( test1,
* bt::depends_on( "s11" ))
{
    BOOST_TEST( "s10" == test1v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test2,
* bt::depends_on( "s10/test1" ))
{
    BOOST_TEST( "s10" == test2v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( s11 )

BOOST_AUTO_TEST_CASE( test1,
* bt::depends_on( "s11/test2" ))
{
    BOOST_TEST( "s11" == test1v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test2,
* bt::depends_on( "s10" ))
{
    BOOST_TEST( "s11" == test2v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE_END()

#endif

BOOST_AUTO_TEST_SUITE( s12 )

BOOST_AUTO_TEST_CASE( test1,
* bt::depends_on( "s13" )
* bt::description( "test test case which depends on test suite with all test cases skipped"))
{
    BOOST_TEST( "s12" == test1v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( s13 )

BOOST_AUTO_TEST_CASE( test1,
* bt::depends_on( "s13/test2" ))
{
    BOOST_TEST( "s13" == test1v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test2,
* bt::disabled())
{
    BOOST_TEST( "s13" == test2v );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_SUITE_END()

// EOF
