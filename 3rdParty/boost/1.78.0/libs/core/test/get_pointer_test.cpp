//
// get_pointer_test.cpp
//
// Copyright 2014, 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations" // std::auto_ptr
#endif

#include <boost/get_pointer.hpp>
#include <boost/core/lightweight_test.hpp>
#include <memory>

struct X
{
};

int main()
{
    using boost::get_pointer;

    {
        X * p = new X;
        BOOST_TEST( get_pointer( p ) == p );

        delete p;
    }

    {
        X * p = 0;
        BOOST_TEST( get_pointer( p ) == 0 );
    }

#if !defined( BOOST_NO_AUTO_PTR )

    {
        std::auto_ptr< X > p( new X );
        BOOST_TEST( get_pointer( p ) == p.get() );
    }

    {
        std::auto_ptr< X > p;
        BOOST_TEST( get_pointer( p ) == 0 );
    }

#endif

#if !defined( BOOST_NO_CXX11_SMART_PTR )

    {
        std::unique_ptr< X > p( new X );
        BOOST_TEST( get_pointer( p ) == p.get() );
    }

    {
        std::unique_ptr< X > p;
        BOOST_TEST( get_pointer( p ) == 0 );
    }

    {
        std::shared_ptr< X > p( new X );
        BOOST_TEST( get_pointer( p ) == p.get() );
    }

    {
        std::shared_ptr< X > p;
        BOOST_TEST( get_pointer( p ) == 0 );
    }

#endif

    return boost::report_errors();
}
