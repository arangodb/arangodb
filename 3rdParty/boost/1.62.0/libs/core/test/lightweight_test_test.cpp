//
// Test for lightweight_test.hpp
//
// Copyright (c) 2014 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/detail/lightweight_test.hpp>

struct X
{
};

#if !defined( BOOST_NO_EXCEPTIONS )
# define LWT_THROW( x ) throw x
#else
# define LWT_THROW( x ) ((void)(x))
#endif

void f( bool x )
{
    if( x )
    {
        LWT_THROW( X() );
    }
    else
    {
        LWT_THROW( 5 );
    }
}

int main()
{
    int x = 0;

    // BOOST_TEST

    BOOST_TEST( x == 0 );
    BOOST_TEST( ++x == 1 );
    BOOST_TEST( x++ == 1 );
    BOOST_TEST( x == 2? true: false );
    BOOST_TEST( x == 2? &x: 0 );

    // BOOST_TEST_EQ

    BOOST_TEST_EQ( x, 2 );
    BOOST_TEST_EQ( ++x, 3 );
    BOOST_TEST_EQ( x++, 3 );

    int y = 4;

    BOOST_TEST_EQ( ++x, ++y );
    BOOST_TEST_EQ( x++, y++ );

    // BOOST_TEST_NE

    BOOST_TEST_NE( ++x, y );
    BOOST_TEST_NE( &x, &y );

    // BOOST_TEST_THROWS

    BOOST_TEST_THROWS( throw X(), X );
    BOOST_TEST_THROWS( throw 1, int );

    BOOST_TEST_THROWS( f(true), X );
    BOOST_TEST_THROWS( f(false), int );

    return boost::report_errors();
}
