// Copyright 2018 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/shared_ptr.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config/pragma_message.hpp>

#if defined(BOOST_DISABLE_THREADS)
BOOST_PRAGMA_MESSAGE( "BOOST_DISABLE_THREADS is defined" )
#else
BOOST_PRAGMA_MESSAGE( "BOOST_DISABLE_THREADS is not defined" )
#endif

#if defined(BOOST_NO_CXX11_HDR_ATOMIC)
BOOST_PRAGMA_MESSAGE( "BOOST_NO_CXX11_HDR_ATOMIC is defined" )
#else
BOOST_PRAGMA_MESSAGE( "BOOST_NO_CXX11_HDR_ATOMIC is not defined" )
#endif

void abi_test_1( boost::shared_ptr<void> & p )
{
    BOOST_TEST_EQ( p.use_count(), 1 );

    p.reset();

    BOOST_TEST_EQ( p.use_count(), 0 );
}

boost::shared_ptr<void> abi_test_2( boost::shared_ptr<void> const & p )
{
    BOOST_TEST_EQ( p.use_count(), 1 );

    return p;
}

boost::shared_ptr<void> abi_test_3()
{
    return boost::shared_ptr<void>( static_cast<int*>( 0 ) );
}
