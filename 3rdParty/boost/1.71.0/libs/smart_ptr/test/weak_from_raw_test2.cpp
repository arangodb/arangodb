//
//  weak_from_raw_test2.cpp
//
//  Test that weak_from_raw returns consistent values
//  throughout the object lifetime
//
//  Copyright (c) 2014 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/smart_ptr/enable_shared_from_raw.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/core/lightweight_test.hpp>

class X;

static boost::weak_ptr< X > r_;

void register_( boost::weak_ptr< X > const & r )
{
    r_ = r;
}

void check_( boost::weak_ptr< X > const & r )
{
    BOOST_TEST( !( r < r_ ) && !( r_ < r ) );
}

void unregister_( boost::weak_ptr< X > const & r )
{
    BOOST_TEST( !( r < r_ ) && !( r_ < r ) );
    r_.reset();
}

class X: public boost::enable_shared_from_raw
{
public:

    X()
    {
        register_( boost::shared_from_raw( this ) );
    }

    ~X()
    {
        unregister_( boost::weak_from_raw( this ) );
    }

    void check()
    {
        check_( boost::weak_from_raw( this ) );
    }
};

int main()
{
    {
        boost::shared_ptr< X > px( new X );
        px->check();
    }

    return boost::report_errors();
}
