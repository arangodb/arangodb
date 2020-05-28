#include <boost/config.hpp>

//
//  bind_nested_rv_test.cpp
//
//  Copyright (c) 2016 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/function.hpp>
#include <boost/detail/lightweight_test.hpp>

//

bool f1( boost::shared_ptr<int> p1 )
{
    BOOST_TEST( p1 != 0 && *p1 == 1 );
    return true;
}

bool f2( boost::shared_ptr<int> p1, boost::shared_ptr<int> p2 )
{
    BOOST_TEST( p1 != 0 && *p1 == 1 );
    BOOST_TEST( p2 != 0 && *p2 == 2 );
    return true;
}

bool f3( boost::shared_ptr<int> p1, boost::shared_ptr<int> p2, boost::shared_ptr<int> p3 )
{
    BOOST_TEST( p1 != 0 && *p1 == 1 );
    BOOST_TEST( p2 != 0 && *p2 == 2 );
    BOOST_TEST( p3 != 0 && *p3 == 3 );
    return true;
}

bool f4( boost::shared_ptr<int> p1, boost::shared_ptr<int> p2, boost::shared_ptr<int> p3, boost::shared_ptr<int> p4 )
{
    BOOST_TEST( p1 != 0 && *p1 == 1 );
    BOOST_TEST( p2 != 0 && *p2 == 2 );
    BOOST_TEST( p3 != 0 && *p3 == 3 );
    BOOST_TEST( p4 != 0 && *p4 == 4 );
    return true;
}

bool f5( boost::shared_ptr<int> p1, boost::shared_ptr<int> p2, boost::shared_ptr<int> p3, boost::shared_ptr<int> p4, boost::shared_ptr<int> p5 )
{
    BOOST_TEST( p1 != 0 && *p1 == 1 );
    BOOST_TEST( p2 != 0 && *p2 == 2 );
    BOOST_TEST( p3 != 0 && *p3 == 3 );
    BOOST_TEST( p4 != 0 && *p4 == 4 );
    BOOST_TEST( p5 != 0 && *p5 == 5 );
    return true;
}

bool f6( boost::shared_ptr<int> p1, boost::shared_ptr<int> p2, boost::shared_ptr<int> p3, boost::shared_ptr<int> p4, boost::shared_ptr<int> p5, boost::shared_ptr<int> p6 )
{
    BOOST_TEST( p1 != 0 && *p1 == 1 );
    BOOST_TEST( p2 != 0 && *p2 == 2 );
    BOOST_TEST( p3 != 0 && *p3 == 3 );
    BOOST_TEST( p4 != 0 && *p4 == 4 );
    BOOST_TEST( p5 != 0 && *p5 == 5 );
    BOOST_TEST( p6 != 0 && *p6 == 6 );
    return true;
}

bool f7( boost::shared_ptr<int> p1, boost::shared_ptr<int> p2, boost::shared_ptr<int> p3, boost::shared_ptr<int> p4, boost::shared_ptr<int> p5, boost::shared_ptr<int> p6, boost::shared_ptr<int> p7 )
{
    BOOST_TEST( p1 != 0 && *p1 == 1 );
    BOOST_TEST( p2 != 0 && *p2 == 2 );
    BOOST_TEST( p3 != 0 && *p3 == 3 );
    BOOST_TEST( p4 != 0 && *p4 == 4 );
    BOOST_TEST( p5 != 0 && *p5 == 5 );
    BOOST_TEST( p6 != 0 && *p6 == 6 );
    BOOST_TEST( p7 != 0 && *p7 == 7 );
    return true;
}

bool f8( boost::shared_ptr<int> p1, boost::shared_ptr<int> p2, boost::shared_ptr<int> p3, boost::shared_ptr<int> p4, boost::shared_ptr<int> p5, boost::shared_ptr<int> p6, boost::shared_ptr<int> p7, boost::shared_ptr<int> p8 )
{
    BOOST_TEST( p1 != 0 && *p1 == 1 );
    BOOST_TEST( p2 != 0 && *p2 == 2 );
    BOOST_TEST( p3 != 0 && *p3 == 3 );
    BOOST_TEST( p4 != 0 && *p4 == 4 );
    BOOST_TEST( p5 != 0 && *p5 == 5 );
    BOOST_TEST( p6 != 0 && *p6 == 6 );
    BOOST_TEST( p7 != 0 && *p7 == 7 );
    BOOST_TEST( p8 != 0 && *p8 == 8 );
    return true;
}

bool f9( boost::shared_ptr<int> p1, boost::shared_ptr<int> p2, boost::shared_ptr<int> p3, boost::shared_ptr<int> p4, boost::shared_ptr<int> p5, boost::shared_ptr<int> p6, boost::shared_ptr<int> p7, boost::shared_ptr<int> p8, boost::shared_ptr<int> p9 )
{
    BOOST_TEST( p1 != 0 && *p1 == 1 );
    BOOST_TEST( p2 != 0 && *p2 == 2 );
    BOOST_TEST( p3 != 0 && *p3 == 3 );
    BOOST_TEST( p4 != 0 && *p4 == 4 );
    BOOST_TEST( p5 != 0 && *p5 == 5 );
    BOOST_TEST( p6 != 0 && *p6 == 6 );
    BOOST_TEST( p7 != 0 && *p7 == 7 );
    BOOST_TEST( p8 != 0 && *p8 == 8 );
    BOOST_TEST( p9 != 0 && *p9 == 9 );
    return true;
}

void test()
{
    {
        boost::function<bool(boost::shared_ptr<int>)> f( f1 );

        ( boost::bind( f, _1 ) && boost::bind( f1, _1 ) )( boost::make_shared<int>( 1 ) );
    }

    {
        boost::function<bool(boost::shared_ptr<int>, boost::shared_ptr<int>)> f( f2 );

        ( boost::bind( f, _1, _2 ) && boost::bind( f2, _1, _2 ) )( boost::make_shared<int>( 1 ), boost::make_shared<int>( 2 ) );
    }

    {
        boost::function<bool(boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>)> f( f3 );

        ( boost::bind( f, _1, _2, _3 ) && boost::bind( f3, _1, _2, _3 ) )( boost::make_shared<int>( 1 ), boost::make_shared<int>( 2 ), boost::make_shared<int>( 3 ) );
    }

    {
        boost::function<bool(boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>)> f( f3 );

        ( boost::bind( f, _1, _2, _3 ) && boost::bind( f3, _1, _2, _3 ) )( boost::make_shared<int>( 1 ), boost::make_shared<int>( 2 ), boost::make_shared<int>( 3 ) );
    }

    {
        boost::function<bool(boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>)> f( f4 );

        ( boost::bind( f, _1, _2, _3, _4 ) && boost::bind( f4, _1, _2, _3, _4 ) )( boost::make_shared<int>( 1 ), boost::make_shared<int>( 2 ), boost::make_shared<int>( 3 ), boost::make_shared<int>( 4 ) );
    }

    {
        boost::function<bool(boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>)> f( f5 );

        ( boost::bind( f, _1, _2, _3, _4, _5 ) && boost::bind( f5, _1, _2, _3, _4, _5 ) )( boost::make_shared<int>( 1 ), boost::make_shared<int>( 2 ), boost::make_shared<int>( 3 ), boost::make_shared<int>( 4 ), boost::make_shared<int>( 5 ) );
    }

    {
        boost::function<bool(boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>)> f( f6 );

        ( boost::bind( f, _1, _2, _3, _4, _5, _6 ) && boost::bind( f6, _1, _2, _3, _4, _5, _6 ) )( boost::make_shared<int>( 1 ), boost::make_shared<int>( 2 ), boost::make_shared<int>( 3 ), boost::make_shared<int>( 4 ), boost::make_shared<int>( 5 ), boost::make_shared<int>( 6 ) );
    }

    {
        boost::function<bool(boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>)> f( f7 );

        ( boost::bind( f, _1, _2, _3, _4, _5, _6, _7 ) && boost::bind( f7, _1, _2, _3, _4, _5, _6, _7 ) )( boost::make_shared<int>( 1 ), boost::make_shared<int>( 2 ), boost::make_shared<int>( 3 ), boost::make_shared<int>( 4 ), boost::make_shared<int>( 5 ), boost::make_shared<int>( 6 ), boost::make_shared<int>( 7 ) );
    }

    {
        boost::function<bool(boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>)> f( f8 );

        ( boost::bind( f, _1, _2, _3, _4, _5, _6, _7, _8 ) && boost::bind( f8, _1, _2, _3, _4, _5, _6, _7, _8 ) )( boost::make_shared<int>( 1 ), boost::make_shared<int>( 2 ), boost::make_shared<int>( 3 ), boost::make_shared<int>( 4 ), boost::make_shared<int>( 5 ), boost::make_shared<int>( 6 ), boost::make_shared<int>( 7 ), boost::make_shared<int>( 8 ) );
    }

    {
        boost::function<bool(boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>, boost::shared_ptr<int>)> f( f9 );

        ( boost::bind( f, _1, _2, _3, _4, _5, _6, _7, _8, _9 ) && boost::bind( f9, _1, _2, _3, _4, _5, _6, _7, _8, _9 ) )( boost::make_shared<int>( 1 ), boost::make_shared<int>( 2 ), boost::make_shared<int>( 3 ), boost::make_shared<int>( 4 ), boost::make_shared<int>( 5 ), boost::make_shared<int>( 6 ), boost::make_shared<int>( 7 ), boost::make_shared<int>( 8 ), boost::make_shared<int>( 9 ) );
    }
}

int main()
{
    test();
    return boost::report_errors();
}
