//
//  shared_from_raw_test4 - based on esft_void_test.cpp
//
//  Copyright 2009, 2014 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//


#include <boost/smart_ptr/enable_shared_from_raw.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/detail/lightweight_test.hpp>

//

class X: public boost::enable_shared_from_raw
{
};

int main()
{
    boost::shared_ptr< void const volatile > pv( new X );
    boost::shared_ptr< void > pv2 = boost::const_pointer_cast< void >( pv );
    boost::shared_ptr< X > px = boost::static_pointer_cast< X >( pv2 );

    try
    {
        boost::shared_ptr< X > qx = boost::shared_from_raw( px.get() );

        BOOST_TEST( px == qx );
        BOOST_TEST( !( px < qx ) && !( qx < px ) );
    }
    catch( boost::bad_weak_ptr const& )
    {
        BOOST_ERROR( "shared_from_this( px.get() ) failed" );
    }

    boost::shared_ptr< X const volatile > px2( px );

    try
    {
        boost::shared_ptr< X const volatile > qx2 = boost::shared_from_raw( px2.get() );

        BOOST_TEST( px2 == qx2 );
        BOOST_TEST( !( px2 < qx2 ) && !( qx2 < px2 ) );
    }
    catch( boost::bad_weak_ptr const& )
    {
        BOOST_ERROR( "shared_from_this( px2.get() ) failed" );
    }

    return boost::report_errors();
}
