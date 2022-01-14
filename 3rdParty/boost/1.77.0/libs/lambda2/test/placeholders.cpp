// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/lambda2.hpp>
#include <boost/core/lightweight_test.hpp>
#include <functional>

int f( int x )
{
    return x;
}

int main()
{
    using namespace boost::lambda2;

    BOOST_TEST_EQ( std::bind(f, _1)( 1 ), 1 );
    BOOST_TEST_EQ( std::bind(f, _2)( 1, 2 ), 2 );
    BOOST_TEST_EQ( std::bind(f, _3)( 1, 2, 3 ), 3 );
    BOOST_TEST_EQ( std::bind(f, _4)( 1, 2, 3, 4 ), 4 );
    BOOST_TEST_EQ( std::bind(f, _5)( 1, 2, 3, 4, 5 ), 5 );
    BOOST_TEST_EQ( std::bind(f, _6)( 1, 2, 3, 4, 5, 6 ), 6 );
    BOOST_TEST_EQ( std::bind(f, _7)( 1, 2, 3, 4, 5, 6, 7 ), 7 );
    BOOST_TEST_EQ( std::bind(f, _8)( 1, 2, 3, 4, 5, 6, 7, 8 ), 8 );
    BOOST_TEST_EQ( std::bind(f, _9)( 1, 2, 3, 4, 5, 6, 7, 8, 9 ), 9 );

    BOOST_TEST_EQ( _1( 1 ), 1 );
    BOOST_TEST_EQ( _2( 1, 2 ), 2 );
    BOOST_TEST_EQ( _3( 1, 2, 3 ), 3 );
    BOOST_TEST_EQ( _4( 1, 2, 3, 4 ), 4 );
    BOOST_TEST_EQ( _5( 1, 2, 3, 4, 5 ), 5 );
    BOOST_TEST_EQ( _6( 1, 2, 3, 4, 5, 6 ), 6 );
    BOOST_TEST_EQ( _7( 1, 2, 3, 4, 5, 6, 7 ), 7 );
    BOOST_TEST_EQ( _8( 1, 2, 3, 4, 5, 6, 7, 8 ), 8 );
    BOOST_TEST_EQ( _9( 1, 2, 3, 4, 5, 6, 7, 8, 9 ), 9 );

    BOOST_TEST_EQ( _1( 1, 2, 3, 4, 5, 6, 7, 8, 9 ), 1 );
    BOOST_TEST_EQ( _2( 1, 2, 3, 4, 5, 6, 7, 8, 9 ), 2 );
    BOOST_TEST_EQ( _3( 1, 2, 3, 4, 5, 6, 7, 8, 9 ), 3 );
    BOOST_TEST_EQ( _4( 1, 2, 3, 4, 5, 6, 7, 8, 9 ), 4 );
    BOOST_TEST_EQ( _5( 1, 2, 3, 4, 5, 6, 7, 8, 9 ), 5 );
    BOOST_TEST_EQ( _6( 1, 2, 3, 4, 5, 6, 7, 8, 9 ), 6 );
    BOOST_TEST_EQ( _7( 1, 2, 3, 4, 5, 6, 7, 8, 9 ), 7 );
    BOOST_TEST_EQ( _8( 1, 2, 3, 4, 5, 6, 7, 8, 9 ), 8 );

    return boost::report_errors();
}
