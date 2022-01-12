// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/lightweight_test.hpp>
#include <vector>

int main()
{
    BOOST_TEST( BOOST_TEST( true ) );
    BOOST_TEST_NOT( BOOST_TEST( false ) );

    BOOST_TEST( BOOST_TEST_NOT( false ) );
    BOOST_TEST_NOT( BOOST_TEST_NOT( true ) );

    BOOST_TEST( BOOST_TEST_EQ( 1, 1 ) );
    BOOST_TEST_NOT( BOOST_TEST_EQ( 1, 2 ) );

    BOOST_TEST( BOOST_TEST_NE( 1, 2 ) );
    BOOST_TEST_NOT( BOOST_TEST_NE( 1, 1 ) );

    BOOST_TEST( BOOST_TEST_LT( 1, 2 ) );
    BOOST_TEST_NOT( BOOST_TEST_LT( 1, 1 ) );

    BOOST_TEST( BOOST_TEST_LE( 1, 1 ) );
    BOOST_TEST_NOT( BOOST_TEST_LE( 2, 1 ) );

    BOOST_TEST( BOOST_TEST_GT( 2, 1 ) );
    BOOST_TEST_NOT( BOOST_TEST_GT( 1, 1 ) );

    BOOST_TEST( BOOST_TEST_GE( 1, 1 ) );
    BOOST_TEST_NOT( BOOST_TEST_GE( 1, 2 ) );

    BOOST_TEST( BOOST_TEST_CSTR_EQ( "1", "1" ) );
    BOOST_TEST_NOT( BOOST_TEST_CSTR_EQ( "1", "2" ) );

    BOOST_TEST( BOOST_TEST_CSTR_NE( "1", "2" ) );
    BOOST_TEST_NOT( BOOST_TEST_CSTR_NE( "1", "1" ) );

    std::vector<int> v1;
    v1.push_back( 1 );

    std::vector<int> v2;

    BOOST_TEST( BOOST_TEST_ALL_EQ(v1.begin(), v1.end(), v1.begin(), v1.end()) );
    BOOST_TEST_NOT( BOOST_TEST_ALL_EQ(v1.begin(), v1.end(), v2.begin(), v2.end()) );

    BOOST_TEST( BOOST_TEST_ALL_WITH(v1.begin(), v1.end(), v1.begin(), v1.end(), boost::detail::lw_test_eq()) );
    BOOST_TEST_NOT( BOOST_TEST_ALL_WITH(v1.begin(), v1.end(), v2.begin(), v2.end(), boost::detail::lw_test_eq()) );

    int const * p = 0;

    BOOST_TEST( p == 0 ) || BOOST_TEST_EQ( *p, 0 );
    BOOST_TEST( p != 0 ) && BOOST_TEST_EQ( *p, 0 );

    int x = 0;
    p = &x;

    BOOST_TEST( p == 0 ) || BOOST_TEST_EQ( *p, 0 );
    BOOST_TEST( p != 0 ) && BOOST_TEST_EQ( *p, 0 );

    return boost::report_errors() == 14? 0: 1;
}
