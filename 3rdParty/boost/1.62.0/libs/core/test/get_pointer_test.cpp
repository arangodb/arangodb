//
// get_pointer_test.cpp
//
// Copyright 2014 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

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
        std::auto_ptr< X > p( new X );
        BOOST_TEST( get_pointer( p ) == p.get() );
    }

#if !defined( BOOST_NO_CXX11_SMART_PTR )

    {
        std::unique_ptr< X > p( new X );
        BOOST_TEST( get_pointer( p ) == p.get() );
    }

    {
        std::shared_ptr< X > p( new X );
        BOOST_TEST( get_pointer( p ) == p.get() );
    }

#endif

    return boost::report_errors();
}
