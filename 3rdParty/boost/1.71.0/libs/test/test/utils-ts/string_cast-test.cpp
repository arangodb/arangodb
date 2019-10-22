//  (C) Copyright Gennadiy Rozental 2001.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.
//
/// @file
/// @brief string_cast unit test
// *****************************************************************************

// Boost.Test
#define BOOST_TEST_MODULE string_cast unit test
#include <boost/test/unit_test.hpp>
#include <boost/test/utils/string_cast.hpp>

namespace utu = boost::unit_test::utils;

//____________________________________________________________________________//

struct  A {
    A(int i_) : i(i_) {}
    int i;
};

inline std::ostream&
operator<<(std::ostream& ostr, A const& a) { return ostr << "A{i=" << a.i << "}"; }

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_string_cast )
{
    BOOST_TEST( utu::string_cast( 1 ) == "1" );
    BOOST_TEST( utu::string_cast( 1.1 ) == "1.1" );
    BOOST_TEST( utu::string_cast( -1 ) == "-1" );
    BOOST_TEST( utu::string_cast( 1U ) == "1" );
    BOOST_TEST( utu::string_cast( 100000000000L ) == "100000000000" );
    BOOST_TEST( utu::string_cast( 1LL << 55 ) == "36028797018963968" );
    BOOST_TEST( utu::string_cast( 'a' ) == "a" );
    BOOST_TEST( utu::string_cast( "abc" ) == "abc" );
    BOOST_TEST( utu::string_cast( A(12) ) == "A{i=12}" );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_string_as )
{
    int ival;

    BOOST_TEST( utu::string_as<int>( "1", ival ) );
    BOOST_TEST( ival == 1 );

    BOOST_TEST( utu::string_as<int>( "  2", ival ) );
    BOOST_TEST( ival == 2 );

    BOOST_TEST( utu::string_as<int>( "+3", ival ) );
    BOOST_TEST( ival == 3 );

    BOOST_TEST( utu::string_as<int>( "-2", ival ) );
    BOOST_TEST( ival == -2 );

    double dval;

    BOOST_TEST( utu::string_as<double>( "0.32", dval ) );
    BOOST_TEST( dval == 0.32 );

    BOOST_TEST( utu::string_as<double>( "-1e-3", dval ) );
    BOOST_TEST( dval == -0.001 );

    unsigned uval;

    BOOST_TEST( utu::string_as<unsigned>( "123", uval ) );
    BOOST_TEST( uval == 123U );

    long lval;

    BOOST_TEST( utu::string_as<long>( "909090", lval ) );
    BOOST_TEST( lval == 909090 );

    long long llval;

    BOOST_TEST( utu::string_as<long long>( "1234123412341234", llval ) );
    BOOST_TEST( llval == 1234123412341234LL );

    std::string sval;

    BOOST_TEST( utu::string_as<std::string>( "abc", sval ) );
    BOOST_TEST( sval == "abc" );
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( test_string_as_validations )
{
    int ival;

    BOOST_TEST( !utu::string_as<int>( "1a", ival ) );
    BOOST_TEST( !utu::string_as<int>( "1 ", ival ) );

    double dval;

    BOOST_TEST( !utu::string_as<double>( "1e-0.1", dval ) );
    BOOST_TEST( !utu::string_as<double>( "1.001.1 ", dval ) );

    unsigned uval;

    BOOST_TEST( !utu::string_as<unsigned>( "1.1", uval ) );

    std::string sval;

    BOOST_TEST( !utu::string_as<std::string>( "a b", sval ) );
}

// EOF
