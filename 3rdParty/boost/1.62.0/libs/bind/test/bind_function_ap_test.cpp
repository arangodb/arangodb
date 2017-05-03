#include <boost/config.hpp>

//
//  bind_function_ap_test.cpp - regression test
//
//  Copyright (c) 2015 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#if defined( __GNUC__ ) && ( __GNUC__ * 100 + __GNUC_MINOR__ >= 406 )
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined( __clang__ ) && defined( __has_warning )
# if __has_warning( "-Wdeprecated-declarations" )
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
# endif
#endif

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <memory>

//

void fv1( std::auto_ptr<int> p1 )
{
    BOOST_TEST( *p1 == 1 );
}

void fv2( std::auto_ptr<int> p1, std::auto_ptr<int> p2 )
{
    BOOST_TEST( *p1 == 1 );
    BOOST_TEST( *p2 == 2 );
}

void fv3( std::auto_ptr<int> p1, std::auto_ptr<int> p2, std::auto_ptr<int> p3 )
{
    BOOST_TEST( *p1 == 1 );
    BOOST_TEST( *p2 == 2 );
    BOOST_TEST( *p3 == 3 );
}

void fv4( std::auto_ptr<int> p1, std::auto_ptr<int> p2, std::auto_ptr<int> p3, std::auto_ptr<int> p4 )
{
    BOOST_TEST( *p1 == 1 );
    BOOST_TEST( *p2 == 2 );
    BOOST_TEST( *p3 == 3 );
    BOOST_TEST( *p4 == 4 );
}

void fv5( std::auto_ptr<int> p1, std::auto_ptr<int> p2, std::auto_ptr<int> p3, std::auto_ptr<int> p4, std::auto_ptr<int> p5 )
{
    BOOST_TEST( *p1 == 1 );
    BOOST_TEST( *p2 == 2 );
    BOOST_TEST( *p3 == 3 );
    BOOST_TEST( *p4 == 4 );
    BOOST_TEST( *p5 == 5 );
}

void fv6( std::auto_ptr<int> p1, std::auto_ptr<int> p2, std::auto_ptr<int> p3, std::auto_ptr<int> p4, std::auto_ptr<int> p5, std::auto_ptr<int> p6 )
{
    BOOST_TEST( *p1 == 1 );
    BOOST_TEST( *p2 == 2 );
    BOOST_TEST( *p3 == 3 );
    BOOST_TEST( *p4 == 4 );
    BOOST_TEST( *p5 == 5 );
    BOOST_TEST( *p6 == 6 );
}

void fv7( std::auto_ptr<int> p1, std::auto_ptr<int> p2, std::auto_ptr<int> p3, std::auto_ptr<int> p4, std::auto_ptr<int> p5, std::auto_ptr<int> p6, std::auto_ptr<int> p7 )
{
    BOOST_TEST( *p1 == 1 );
    BOOST_TEST( *p2 == 2 );
    BOOST_TEST( *p3 == 3 );
    BOOST_TEST( *p4 == 4 );
    BOOST_TEST( *p5 == 5 );
    BOOST_TEST( *p6 == 6 );
    BOOST_TEST( *p7 == 7 );
}

void fv8( std::auto_ptr<int> p1, std::auto_ptr<int> p2, std::auto_ptr<int> p3, std::auto_ptr<int> p4, std::auto_ptr<int> p5, std::auto_ptr<int> p6, std::auto_ptr<int> p7, std::auto_ptr<int> p8 )
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

void fv9( std::auto_ptr<int> p1, std::auto_ptr<int> p2, std::auto_ptr<int> p3, std::auto_ptr<int> p4, std::auto_ptr<int> p5, std::auto_ptr<int> p6, std::auto_ptr<int> p7, std::auto_ptr<int> p8, std::auto_ptr<int> p9 )
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
        boost::function<void(std::auto_ptr<int>)> fw1 = boost::bind( fv1, _1 );

        std::auto_ptr<int> p1( new int(1) );

        fw1( p1 );
    }

    {
        boost::function<void(std::auto_ptr<int>, std::auto_ptr<int>)> fw2 = boost::bind( fv2, _1, _2 );

        std::auto_ptr<int> p1( new int(1) );
        std::auto_ptr<int> p2( new int(2) );

        fw2( p1, p2 );
    }

    {
        boost::function<void(std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>)> fw3 = boost::bind( fv3, _1, _2, _3 );

        std::auto_ptr<int> p1( new int(1) );
        std::auto_ptr<int> p2( new int(2) );
        std::auto_ptr<int> p3( new int(3) );

        fw3( p1, p2, p3 );
    }

    {
        boost::function<void(std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>)> fw4 = boost::bind( fv4, _1, _2, _3, _4 );

        std::auto_ptr<int> p1( new int(1) );
        std::auto_ptr<int> p2( new int(2) );
        std::auto_ptr<int> p3( new int(3) );
        std::auto_ptr<int> p4( new int(4) );

        fw4( p1, p2, p3, p4 );
    }

    {
        boost::function<void(std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>)> fw5 = boost::bind( fv5, _1, _2, _3, _4, _5 );

        std::auto_ptr<int> p1( new int(1) );
        std::auto_ptr<int> p2( new int(2) );
        std::auto_ptr<int> p3( new int(3) );
        std::auto_ptr<int> p4( new int(4) );
        std::auto_ptr<int> p5( new int(5) );

        fw5( p1, p2, p3, p4, p5 );
    }

    {
        boost::function<void(std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>)> fw6 = boost::bind( fv6, _1, _2, _3, _4, _5, _6 );

        std::auto_ptr<int> p1( new int(1) );
        std::auto_ptr<int> p2( new int(2) );
        std::auto_ptr<int> p3( new int(3) );
        std::auto_ptr<int> p4( new int(4) );
        std::auto_ptr<int> p5( new int(5) );
        std::auto_ptr<int> p6( new int(6) );

        fw6( p1, p2, p3, p4, p5, p6 );
    }

    {
        boost::function<void(std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>)> fw7 = boost::bind( fv7, _1, _2, _3, _4, _5, _6, _7 );

        std::auto_ptr<int> p1( new int(1) );
        std::auto_ptr<int> p2( new int(2) );
        std::auto_ptr<int> p3( new int(3) );
        std::auto_ptr<int> p4( new int(4) );
        std::auto_ptr<int> p5( new int(5) );
        std::auto_ptr<int> p6( new int(6) );
        std::auto_ptr<int> p7( new int(7) );

        fw7( p1, p2, p3, p4, p5, p6, p7 );
    }

    {
        boost::function<void(std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>)> fw8 = boost::bind( fv8, _1, _2, _3, _4, _5, _6, _7, _8 );

        std::auto_ptr<int> p1( new int(1) );
        std::auto_ptr<int> p2( new int(2) );
        std::auto_ptr<int> p3( new int(3) );
        std::auto_ptr<int> p4( new int(4) );
        std::auto_ptr<int> p5( new int(5) );
        std::auto_ptr<int> p6( new int(6) );
        std::auto_ptr<int> p7( new int(7) );
        std::auto_ptr<int> p8( new int(8) );

        fw8( p1, p2, p3, p4, p5, p6, p7, p8 );
    }

    {
        boost::function<void(std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>, std::auto_ptr<int>)> fw9 = boost::bind( fv9, _1, _2, _3, _4, _5, _6, _7, _8, _9 );

        std::auto_ptr<int> p1( new int(1) );
        std::auto_ptr<int> p2( new int(2) );
        std::auto_ptr<int> p3( new int(3) );
        std::auto_ptr<int> p4( new int(4) );
        std::auto_ptr<int> p5( new int(5) );
        std::auto_ptr<int> p6( new int(6) );
        std::auto_ptr<int> p7( new int(7) );
        std::auto_ptr<int> p8( new int(8) );
        std::auto_ptr<int> p9( new int(9) );

        fw9( p1, p2, p3, p4, p5, p6, p7, p8, p9 );
    }
}

int main()
{
    test();
    return boost::report_errors();
}
