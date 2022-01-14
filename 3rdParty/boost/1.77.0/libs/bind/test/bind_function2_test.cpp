#include <boost/config.hpp>

//
//  bind_function2_test.cpp - regression test
//
//  Copyright (c) 2015 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/bind/bind.hpp>
#include <boost/function.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::placeholders;

//

void fv1( int & a )
{
    a = 17041;
}

void fv2( int & a, int b )
{
    a = b;
}

void fv3( int & a, int b, int c )
{
    a = b + c;
}

void fv4( int & a, int b, int c, int d )
{
    a = b + c + d;
}

void fv5( int & a, int b, int c, int d, int e )
{
    a = b + c + d + e;
}

void fv6( int & a, int b, int c, int d, int e, int f )
{
    a = b + c + d + e + f;
}

void fv7( int & a, int b, int c, int d, int e, int f, int g )
{
    a = b + c + d + e + f + g;
}

void fv8( int & a, int b, int c, int d, int e, int f, int g, int h )
{
    a = b + c + d + e + f + g + h;
}

void fv9( int & a, int b, int c, int d, int e, int f, int g, int h, int i )
{
    a = b + c + d + e + f + g + h + i;
}

void function_test()
{
    int x = 0;

    {
        boost::function<void(int&)> fw1 = boost::bind( fv1, _1 );
        fw1( x ); BOOST_TEST( x == 17041 );
    }

    {
        boost::function<void(int&, int)> fw2 = boost::bind( fv2, _1, _2 );
        fw2( x, 1 ); BOOST_TEST( x == 1 );
    }

    {
        boost::function<void(int&, int, int)> fw3 = boost::bind( fv3, _1, _2, _3 );
        fw3( x, 1, 2 ); BOOST_TEST( x == 1+2 );
    }

    {
        boost::function<void(int&, int, int, int)> fw4 = boost::bind( fv4, _1, _2, _3, _4 );
        fw4( x, 1, 2, 3 ); BOOST_TEST( x == 1+2+3 );
    }

    {
        boost::function<void(int&, int, int, int, int)> fw5 = boost::bind( fv5, _1, _2, _3, _4, _5 );
        fw5( x, 1, 2, 3, 4 ); BOOST_TEST( x == 1+2+3+4 );
    }

    {
        boost::function<void(int&, int, int, int, int, int)> fw6 = boost::bind( fv6, _1, _2, _3, _4, _5, _6 );
        fw6( x, 1, 2, 3, 4, 5 ); BOOST_TEST( x == 1+2+3+4+5 );
    }

    {
        boost::function<void(int&, int, int, int, int, int, int)> fw7 = boost::bind( fv7, _1, _2, _3, _4, _5, _6, _7 );
        fw7( x, 1, 2, 3, 4, 5, 6 ); BOOST_TEST( x == 1+2+3+4+5+6 );
    }

    {
        boost::function<void(int&, int, int, int, int, int, int, int)> fw8 = boost::bind( fv8, _1, _2, _3, _4, _5, _6, _7, _8 );
        fw8( x, 1, 2, 3, 4, 5, 6, 7 ); BOOST_TEST( x == 1+2+3+4+5+6+7 );
    }

    {
        boost::function<void(int&, int, int, int, int, int, int, int, int)> fw9 = boost::bind( fv9, _1, _2, _3, _4, _5, _6, _7, _8, _9 );
        fw9( x, 1, 2, 3, 4, 5, 6, 7, 8 ); BOOST_TEST( x == 1+2+3+4+5+6+7+8 );
    }
}

int main()
{
    function_test();
    return boost::report_errors();
}
