//  (C) Copyright Gennadiy Rozental 2003-2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
//  File        : $RCSfile$
//
//  Version     : $Revision$
//
//  Description : unit test for class properties facility
// ***************************************************************************

// Boost.Test
#define BOOST_TEST_MODULE Boost.Test algorithms test
#include <boost/test/unit_test.hpp>
#include <boost/test/utils/class_properties.hpp>
#include <boost/test/utils/basic_cstring/basic_cstring.hpp>
#include <boost/test/utils/algorithm.hpp>
namespace utf = boost::unit_test;
namespace utu = boost::unit_test::utils;
using utf::const_string;

// STL
#include <cctype>
#include <functional>

# ifdef BOOST_NO_STDC_NAMESPACE
namespace std { using ::toupper; }
# endif

#ifdef BOOST_NO_CXX11_DECLTYPE
#define TEST_SURROUND_EXPRESSION(x) (x)
#else
#define TEST_SURROUND_EXPRESSION(x) x
#endif

//____________________________________________________________________________//

bool predicate( char c1, char c2 ) { return (std::toupper)( c1 ) == (std::toupper)( c2 ); }

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_mismatch )
{
    const_string cs1( "test_string" );
    const_string cs2( "test_stream" );

    BOOST_TEST( TEST_SURROUND_EXPRESSION(utu::mismatch( cs1.begin(), cs1.end(), cs2.begin(), cs2.end() ).first - cs1.begin()) == 8 );

    cs2 = "trest";
    BOOST_TEST( TEST_SURROUND_EXPRESSION(utu::mismatch( cs1.begin(), cs1.end(), cs2.begin(), cs2.end() ).first - cs1.begin()) == 1 );

    cs2 = "test_string_klmn";
    BOOST_TEST( TEST_SURROUND_EXPRESSION(utu::mismatch( cs1.begin(), cs1.end(), cs2.begin(), cs2.end() ).first - cs1.begin()) == 11 );

    cs2 = "TeSt_liNk";
    BOOST_TEST(
        TEST_SURROUND_EXPRESSION(utu::mismatch( cs1.begin(), cs1.end(), cs2.begin(), cs2.end(), std::ptr_fun( predicate ) ).first - cs1.begin()) == 5 );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_find_first_not_of )
{
    const_string cs( "test_string" );
    const_string another( "tes" );

    BOOST_TEST( TEST_SURROUND_EXPRESSION(utu::find_first_not_of( cs.begin(), cs.end(), another.begin(), another.end() ) - cs.begin()) == 4 );

    another = "T_sE";
    BOOST_TEST(
        TEST_SURROUND_EXPRESSION(utu::find_first_not_of( cs.begin(), cs.end(), another.begin(), another.end(), std::ptr_fun( predicate ) ) - cs.begin()) == 7 );

    another = "tes_ring";
    BOOST_TEST( utu::find_last_not_of( cs.begin(), cs.end(), another.begin(), another.end() ) == cs.end() );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_find_last_of )
{
    const_string cs( "test_string" );
    const_string another( "tes" );

    BOOST_TEST( TEST_SURROUND_EXPRESSION(utu::find_last_of( cs.begin(), cs.end(), another.begin(), another.end() ) - cs.begin()) == 6 );

    another = "_Se";
    BOOST_TEST( TEST_SURROUND_EXPRESSION(utu::find_last_of( cs.begin(), cs.end(), another.begin(), another.end(), std::ptr_fun( predicate ) ) - cs.begin()) == 5 );

    another = "qw";
    BOOST_TEST( utu::find_last_of( cs.begin(), cs.end(), another.begin(), another.end() ) == cs.end() );

    cs = "qerty";
    BOOST_TEST( TEST_SURROUND_EXPRESSION(utu::find_last_of( cs.begin(), cs.end(), another.begin(), another.end() ) - cs.begin()) == 0 );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_find_last_not_of )
{
    const_string cs( "test_string" );
    const_string another( "string" );

    BOOST_TEST( TEST_SURROUND_EXPRESSION(utu::find_last_not_of( cs.begin(), cs.end(), another.begin(), another.end() ) - cs.begin()) == 4 );

    another = "_SeG";
    BOOST_TEST( TEST_SURROUND_EXPRESSION(utu::find_last_not_of( cs.begin(), cs.end(), another.begin(), another.end(), std::ptr_fun( predicate ) ) - cs.begin()) == 9 );

    another = "e_string";
    BOOST_TEST( utu::find_last_not_of( cs.begin(), cs.end(), another.begin(), another.end() ) == cs.end() );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_replace_all )
{
    {
      std::string cs( "cstring: cstring is a const string that can be transformed to an std::string" );

      const std::string to_look_for[] = {"cstring", "const"};
      const std::string to_replace[] = { "const_string", "constant" };

      BOOST_TEST( utu::replace_all_occurrences_of(
                    cs,
                    to_look_for, to_look_for + sizeof(to_look_for)/sizeof(to_look_for[0]),
                    to_replace, to_replace + sizeof(to_replace)/sizeof(to_replace[0]))
                    ==
                    "constant_string: constant_string is a constant string that can be transformed to an std::string"
              );
    }

    {
      std::string cs( "some\\file\\with\\wrong:.path" );

      const std::string to_look_for[] = {"\\", ":"};
      const std::string to_replace[] = { "/", "_" };

      BOOST_TEST( utu::replace_all_occurrences_of(
                    cs,
                    to_look_for, to_look_for + sizeof(to_look_for)/sizeof(to_look_for[0]),
                    to_replace, to_replace + sizeof(to_replace)/sizeof(to_replace[0]))
                    ==
                    "some/file/with/wrong_.path"
              );
    }
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_replace_with_wildcards )
{
    {
      std::string cs( "this string contains x='374'u elements of size y='24'u and y='75'z" );

      const std::string to_look_for[] = {"x='*'u", "y='*'u"};
      const std::string to_replace[] = { "x='27'q", "k='*0'p" };

      BOOST_TEST( utu::replace_all_occurrences_with_wildcards(
                    cs,
                    to_look_for, to_look_for + sizeof(to_look_for)/sizeof(to_look_for[0]),
                    to_replace, to_replace + sizeof(to_replace)/sizeof(to_replace[0]))
                    ==
                    "this string contains x='27'q elements of size k='240'p and y='75'z"
              );
    }
}

//____________________________________________________________________________//

// EOF
