#include <boost/config.hpp>

//
//  bind_fwd2_test.cpp - forwarding test for 2 arguments
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

int fv1( int const & a )
{
    return a;
}

void fv2_1( int & a, int const & b )
{
    a = b;
}

void fv2_2( int const & a, int & b )
{
    b = a;
}

int fv2_3( int const & a, int const & b )
{
    return a+b;
}

void test()
{
    {
        int const a = 1;
        int r = boost::bind( fv1, _1 )( a );
        BOOST_TEST( r == 1 );
    }

    {
        int r = boost::bind( fv1, _1 )( 1 );
        BOOST_TEST( r == 1 );
    }

    {
        int a = 1;
        int const b = 2;

        boost::bind( fv2_1, _1, _2 )( a, b );

        BOOST_TEST( a == 2 );
    }

    {
        int a = 1;

        boost::bind( fv2_1, _1, _2 )( a, 2 );

        BOOST_TEST( a == 2 );
    }

    {
        int const a = 1;
        int b = 2;

        boost::bind( fv2_2, _1, _2 )( a, b );

        BOOST_TEST( b == 1 );
    }

    {
        int b = 2;

        boost::bind( fv2_2, _1, _2 )( 1, b );

        BOOST_TEST( b == 1 );
    }

    {
        int const a = 1;
        int const b = 2;

        int r = boost::bind( fv2_3, _1, _2 )( a, b );

        BOOST_TEST( r == 3 );
    }

    {
        int const a = 1;

        int r = boost::bind( fv2_3, _1, _2 )( a, 2 );

        BOOST_TEST( r == 3 );
    }

    {
        int const b = 2;

        int r = boost::bind( fv2_3, _1, _2 )( 1, b );

        BOOST_TEST( r == 3 );
    }

    {
        int r = boost::bind( fv2_3, _1, _2 )( 1, 2 );

        BOOST_TEST( r == 3 );
    }
}

int main()
{
    test();
    return boost::report_errors();
}
