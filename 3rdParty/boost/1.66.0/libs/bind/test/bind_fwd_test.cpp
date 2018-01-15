#include <boost/config.hpp>

//
//  bind_fwd_test.cpp - forwarding test
//
//  Copyright (c) 2015 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/bind.hpp>
#include <boost/detail/lightweight_test.hpp>

//

void fv1( int & a )
{
    a = 1;
}

void fv2( int & a, int & b )
{
    a = 1;
    b = 2;
}

void fv3( int & a, int & b, int & c )
{
    a = 1;
    b = 2;
    c = 3;
}

void fv4( int & a, int & b, int & c, int & d )
{
    a = 1;
    b = 2;
    c = 3;
    d = 4;
}

void fv5( int & a, int & b, int & c, int & d, int & e )
{
    a = 1;
    b = 2;
    c = 3;
    d = 4;
    e = 5;
}

void fv6( int & a, int & b, int & c, int & d, int & e, int & f )
{
    a = 1;
    b = 2;
    c = 3;
    d = 4;
    e = 5;
    f = 6;
}

void fv7( int & a, int & b, int & c, int & d, int & e, int & f, int & g )
{
    a = 1;
    b = 2;
    c = 3;
    d = 4;
    e = 5;
    f = 6;
    g = 7;
}

void fv8( int & a, int & b, int & c, int & d, int & e, int & f, int & g, int & h )
{
    a = 1;
    b = 2;
    c = 3;
    d = 4;
    e = 5;
    f = 6;
    g = 7;
    h = 8;
}

void fv9( int & a, int & b, int & c, int & d, int & e, int & f, int & g, int & h, int & i )
{
    a = 1;
    b = 2;
    c = 3;
    d = 4;
    e = 5;
    f = 6;
    g = 7;
    h = 8;
    i = 9;
}

void test()
{
    {
        int a = 0;

        boost::bind( fv1, _1 )( a );

        BOOST_TEST( a == 1 );
    }

    {
        int a = 0;
        int b = 0;

        boost::bind( fv2, _1, _2 )( a, b );

        BOOST_TEST( a == 1 );
        BOOST_TEST( b == 2 );
    }

    {
        int a = 0;
        int b = 0;
        int c = 0;

        boost::bind( fv3, _1, _2, _3 )( a, b, c );

        BOOST_TEST( a == 1 );
        BOOST_TEST( b == 2 );
        BOOST_TEST( c == 3 );
    }

    {
        int a = 0;
        int b = 0;
        int c = 0;
        int d = 0;

        boost::bind( fv4, _1, _2, _3, _4 )( a, b, c, d );

        BOOST_TEST( a == 1 );
        BOOST_TEST( b == 2 );
        BOOST_TEST( c == 3 );
        BOOST_TEST( d == 4 );
    }

    {
        int a = 0;
        int b = 0;
        int c = 0;
        int d = 0;
        int e = 0;

        boost::bind( fv5, _1, _2, _3, _4, _5 )( a, b, c, d, e );

        BOOST_TEST( a == 1 );
        BOOST_TEST( b == 2 );
        BOOST_TEST( c == 3 );
        BOOST_TEST( d == 4 );
        BOOST_TEST( e == 5 );
    }

    {
        int a = 0;
        int b = 0;
        int c = 0;
        int d = 0;
        int e = 0;
        int f = 0;

        boost::bind( fv6, _1, _2, _3, _4, _5, _6 )( a, b, c, d, e, f );

        BOOST_TEST( a == 1 );
        BOOST_TEST( b == 2 );
        BOOST_TEST( c == 3 );
        BOOST_TEST( d == 4 );
        BOOST_TEST( e == 5 );
        BOOST_TEST( f == 6 );
    }

    {
        int a = 0;
        int b = 0;
        int c = 0;
        int d = 0;
        int e = 0;
        int f = 0;
        int g = 0;

        boost::bind( fv7, _1, _2, _3, _4, _5, _6, _7 )( a, b, c, d, e, f, g );

        BOOST_TEST( a == 1 );
        BOOST_TEST( b == 2 );
        BOOST_TEST( c == 3 );
        BOOST_TEST( d == 4 );
        BOOST_TEST( e == 5 );
        BOOST_TEST( f == 6 );
        BOOST_TEST( g == 7 );
    }

    {
        int a = 0;
        int b = 0;
        int c = 0;
        int d = 0;
        int e = 0;
        int f = 0;
        int g = 0;
        int h = 0;

        boost::bind( fv8, _1, _2, _3, _4, _5, _6, _7, _8 )( a, b, c, d, e, f, g, h );

        BOOST_TEST( a == 1 );
        BOOST_TEST( b == 2 );
        BOOST_TEST( c == 3 );
        BOOST_TEST( d == 4 );
        BOOST_TEST( e == 5 );
        BOOST_TEST( f == 6 );
        BOOST_TEST( g == 7 );
        BOOST_TEST( h == 8 );
    }

    {
        int a = 0;
        int b = 0;
        int c = 0;
        int d = 0;
        int e = 0;
        int f = 0;
        int g = 0;
        int h = 0;
        int i = 0;

        boost::bind( fv9, _1, _2, _3, _4, _5, _6, _7, _8, _9 )( a, b, c, d, e, f, g, h, i );

        BOOST_TEST( a == 1 );
        BOOST_TEST( b == 2 );
        BOOST_TEST( c == 3 );
        BOOST_TEST( d == 4 );
        BOOST_TEST( e == 5 );
        BOOST_TEST( f == 6 );
        BOOST_TEST( g == 7 );
        BOOST_TEST( h == 8 );
        BOOST_TEST( i == 9 );
    }
}

int main()
{
    test();
    return boost::report_errors();
}
