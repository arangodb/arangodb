// Copyright 2018, 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt)

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/smart_ptr/detail/lightweight_thread.hpp>
#include <boost/bind/bind.hpp>
#include <boost/core/lightweight_test.hpp>

static boost::shared_ptr<int> sp( new int );
static boost::weak_ptr<int> wp( sp );

void f1( int n )
{
    for( int i = 0; i < n; ++i )
    {
        boost::weak_ptr<int> p1( wp );

        BOOST_TEST( !wp.expired() );
        BOOST_TEST( wp.lock() != 0 );
    }
}

void f2( int n )
{
    for( int i = 0; i < n; ++i )
    {
        boost::weak_ptr<int> p1( wp );

        BOOST_TEST( wp.expired() );
        BOOST_TEST( wp.lock() == 0 );
    }
}

int main()
{
    int const N = 100000; // iterations
    int const M = 8;      // threads

    boost::detail::lw_thread_t th[ M ] = {};

    for( int i = 0; i < M; ++i )
    {
        boost::detail::lw_thread_create( th[ i ], boost::bind( f1, N ) );
    }

    for( int i = 0; i < M; ++i )
    {
        boost::detail::lw_thread_join( th[ i ] );
    }

    BOOST_TEST_EQ( sp.use_count(), 1 );
    BOOST_TEST_EQ( wp.use_count(), 1 );

    sp.reset();

    BOOST_TEST_EQ( sp.use_count(), 0 );
    BOOST_TEST_EQ( wp.use_count(), 0 );

    for( int i = 0; i < M; ++i )
    {
        boost::detail::lw_thread_create( th[ i ], boost::bind( f2, N ) );
    }

    for( int i = 0; i < M; ++i )
    {
        boost::detail::lw_thread_join( th[ i ] );
    }

    wp.reset();

    return boost::report_errors();
}
