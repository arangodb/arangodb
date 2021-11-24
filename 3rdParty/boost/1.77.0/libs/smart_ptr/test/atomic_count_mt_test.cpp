// Copyright 2018, 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt)

#include <boost/smart_ptr/detail/atomic_count.hpp>
#include <boost/smart_ptr/detail/lightweight_thread.hpp>
#include <boost/bind/bind.hpp>
#include <boost/core/lightweight_test.hpp>

static boost::detail::atomic_count count( 0 );

void f( int n )
{
    for( int i = 0; i < n; ++i )
    {
        ++count;
    }
}

int main()
{
    int const N = 100000; // iterations
    int const M = 8;      // threads

    boost::detail::lw_thread_t th[ M ] = {};

    for( int i = 0; i < M; ++i )
    {
        boost::detail::lw_thread_create( th[ i ], boost::bind( f, N ) );
    }

    for( int i = 0; i < M; ++i )
    {
        boost::detail::lw_thread_join( th[ i ] );
    }

    BOOST_TEST_EQ( count, N * M );

    return boost::report_errors();
}
