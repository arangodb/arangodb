//
//  weak_from_raw_test5.cpp
//
//  Tests whether pointers returned from weak_from_raw
//  expire properly
//
//  Copyright 2015 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/smart_ptr/enable_shared_from_raw.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/core/lightweight_test.hpp>

class X: public boost::enable_shared_from_raw
{
public:

    explicit X( boost::weak_ptr< X > & r )
    {
        r = boost::weak_from_raw( this );
    }
};

int main()
{
    boost::weak_ptr<X> p1, p2;

    {
        boost::shared_ptr< X > px( new X( p1 ) );
        p2 = boost::weak_from_raw( px.get() );

        BOOST_TEST( !p1.expired() );
        BOOST_TEST( !p2.expired() );
    }

    BOOST_TEST( p1.expired() );
    BOOST_TEST( p2.expired() );

    return boost::report_errors();
}
