#include <boost/config.hpp>

#if defined( BOOST_NO_CXX11_RVALUE_REFERENCES ) || defined( BOOST_NO_CXX11_SMART_PTR )

int main()
{
}

#else

//
//  bind_unique_ptr_test.cpp
//
//  Copyright (c) 2015 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/bind/bind.hpp>
#include <boost/core/lightweight_test.hpp>
#include <memory>

using namespace boost::placeholders;

//

void fv1( std::unique_ptr<int> p1 )
{
    BOOST_TEST( *p1 == 1 );
}

void fv2( std::unique_ptr<int> p1, std::unique_ptr<int> p2 )
{
    BOOST_TEST( *p1 == 1 );
    BOOST_TEST( *p2 == 2 );
}

void fv3( std::unique_ptr<int> p1, std::unique_ptr<int> p2, std::unique_ptr<int> p3 )
{
    BOOST_TEST( *p1 == 1 );
    BOOST_TEST( *p2 == 2 );
    BOOST_TEST( *p3 == 3 );
}

void fv4( std::unique_ptr<int> p1, std::unique_ptr<int> p2, std::unique_ptr<int> p3, std::unique_ptr<int> p4 )
{
    BOOST_TEST( *p1 == 1 );
    BOOST_TEST( *p2 == 2 );
    BOOST_TEST( *p3 == 3 );
    BOOST_TEST( *p4 == 4 );
}

void fv5( std::unique_ptr<int> p1, std::unique_ptr<int> p2, std::unique_ptr<int> p3, std::unique_ptr<int> p4, std::unique_ptr<int> p5 )
{
    BOOST_TEST( *p1 == 1 );
    BOOST_TEST( *p2 == 2 );
    BOOST_TEST( *p3 == 3 );
    BOOST_TEST( *p4 == 4 );
    BOOST_TEST( *p5 == 5 );
}

void fv6( std::unique_ptr<int> p1, std::unique_ptr<int> p2, std::unique_ptr<int> p3, std::unique_ptr<int> p4, std::unique_ptr<int> p5, std::unique_ptr<int> p6 )
{
    BOOST_TEST( *p1 == 1 );
    BOOST_TEST( *p2 == 2 );
    BOOST_TEST( *p3 == 3 );
    BOOST_TEST( *p4 == 4 );
    BOOST_TEST( *p5 == 5 );
    BOOST_TEST( *p6 == 6 );
}

void fv7( std::unique_ptr<int> p1, std::unique_ptr<int> p2, std::unique_ptr<int> p3, std::unique_ptr<int> p4, std::unique_ptr<int> p5, std::unique_ptr<int> p6, std::unique_ptr<int> p7 )
{
    BOOST_TEST( *p1 == 1 );
    BOOST_TEST( *p2 == 2 );
    BOOST_TEST( *p3 == 3 );
    BOOST_TEST( *p4 == 4 );
    BOOST_TEST( *p5 == 5 );
    BOOST_TEST( *p6 == 6 );
    BOOST_TEST( *p7 == 7 );
}

void fv8( std::unique_ptr<int> p1, std::unique_ptr<int> p2, std::unique_ptr<int> p3, std::unique_ptr<int> p4, std::unique_ptr<int> p5, std::unique_ptr<int> p6, std::unique_ptr<int> p7, std::unique_ptr<int> p8 )
{
    BOOST_TEST( *p1 == 1 );
    BOOST_TEST( *p2 == 2 );
    BOOST_TEST( *p3 == 3 );
    BOOST_TEST( *p4 == 4 );
    BOOST_TEST( *p5 == 5 );
    BOOST_TEST( *p6 == 6 );
    BOOST_TEST( *p7 == 7 );
    BOOST_TEST( *p8 == 8 );
}

void fv9( std::unique_ptr<int> p1, std::unique_ptr<int> p2, std::unique_ptr<int> p3, std::unique_ptr<int> p4, std::unique_ptr<int> p5, std::unique_ptr<int> p6, std::unique_ptr<int> p7, std::unique_ptr<int> p8, std::unique_ptr<int> p9 )
{
    BOOST_TEST( *p1 == 1 );
    BOOST_TEST( *p2 == 2 );
    BOOST_TEST( *p3 == 3 );
    BOOST_TEST( *p4 == 4 );
    BOOST_TEST( *p5 == 5 );
    BOOST_TEST( *p6 == 6 );
    BOOST_TEST( *p7 == 7 );
    BOOST_TEST( *p8 == 8 );
    BOOST_TEST( *p9 == 9 );
}

void test()
{
    {
        std::unique_ptr<int> p1( new int(1) );

        boost::bind( fv1, _1 )( std::move( p1 ) );
    }

    {
        std::unique_ptr<int> p1( new int(1) );
        std::unique_ptr<int> p2( new int(2) );

        boost::bind( fv2, _1, _2 )( std::move( p1 ), std::move( p2 ) );
    }

    {
        std::unique_ptr<int> p1( new int(1) );
        std::unique_ptr<int> p2( new int(2) );
        std::unique_ptr<int> p3( new int(3) );

        boost::bind( fv3, _1, _2, _3 )( std::move( p1 ), std::move( p2 ), std::move( p3 ) );
    }

    {
        std::unique_ptr<int> p1( new int(1) );
        std::unique_ptr<int> p2( new int(2) );
        std::unique_ptr<int> p3( new int(3) );
        std::unique_ptr<int> p4( new int(4) );

        boost::bind( fv4, _1, _2, _3, _4 )( std::move( p1 ), std::move( p2 ), std::move( p3 ), std::move( p4 ) );
    }

    {
        std::unique_ptr<int> p1( new int(1) );
        std::unique_ptr<int> p2( new int(2) );
        std::unique_ptr<int> p3( new int(3) );
        std::unique_ptr<int> p4( new int(4) );
        std::unique_ptr<int> p5( new int(5) );

        boost::bind( fv5, _1, _2, _3, _4, _5 )( std::move( p1 ), std::move( p2 ), std::move( p3 ), std::move( p4 ), std::move( p5 ) );
    }

    {
        std::unique_ptr<int> p1( new int(1) );
        std::unique_ptr<int> p2( new int(2) );
        std::unique_ptr<int> p3( new int(3) );
        std::unique_ptr<int> p4( new int(4) );
        std::unique_ptr<int> p5( new int(5) );
        std::unique_ptr<int> p6( new int(6) );

        boost::bind( fv6, _1, _2, _3, _4, _5, _6 )( std::move( p1 ), std::move( p2 ), std::move( p3 ), std::move( p4 ), std::move( p5 ), std::move( p6 ) );
    }

    {
        std::unique_ptr<int> p1( new int(1) );
        std::unique_ptr<int> p2( new int(2) );
        std::unique_ptr<int> p3( new int(3) );
        std::unique_ptr<int> p4( new int(4) );
        std::unique_ptr<int> p5( new int(5) );
        std::unique_ptr<int> p6( new int(6) );
        std::unique_ptr<int> p7( new int(7) );

        boost::bind( fv7, _1, _2, _3, _4, _5, _6, _7 )( std::move( p1 ), std::move( p2 ), std::move( p3 ), std::move( p4 ), std::move( p5 ), std::move( p6 ), std::move( p7 ) );
    }

    {
        std::unique_ptr<int> p1( new int(1) );
        std::unique_ptr<int> p2( new int(2) );
        std::unique_ptr<int> p3( new int(3) );
        std::unique_ptr<int> p4( new int(4) );
        std::unique_ptr<int> p5( new int(5) );
        std::unique_ptr<int> p6( new int(6) );
        std::unique_ptr<int> p7( new int(7) );
        std::unique_ptr<int> p8( new int(8) );

        boost::bind( fv8, _1, _2, _3, _4, _5, _6, _7, _8 )( std::move( p1 ), std::move( p2 ), std::move( p3 ), std::move( p4 ), std::move( p5 ), std::move( p6 ), std::move( p7 ), std::move( p8 ) );
    }

    {
        std::unique_ptr<int> p1( new int(1) );
        std::unique_ptr<int> p2( new int(2) );
        std::unique_ptr<int> p3( new int(3) );
        std::unique_ptr<int> p4( new int(4) );
        std::unique_ptr<int> p5( new int(5) );
        std::unique_ptr<int> p6( new int(6) );
        std::unique_ptr<int> p7( new int(7) );
        std::unique_ptr<int> p8( new int(8) );
        std::unique_ptr<int> p9( new int(9) );

        boost::bind( fv9, _1, _2, _3, _4, _5, _6, _7, _8, _9 )( std::move( p1 ), std::move( p2 ), std::move( p3 ), std::move( p4 ), std::move( p5 ), std::move( p6 ), std::move( p7 ), std::move( p8 ), std::move( p9 ) );
    }
}

int main()
{
    test();
    return boost::report_errors();
}

#endif // #if defined( BOOST_NO_CXX11_RVALUE_REFERENCES ) || defined( BOOST_NO_CXX11_SMART_PTR )
