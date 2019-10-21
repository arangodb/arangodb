
// lw_thread_test.cpp
//
// Copyright 2018 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.

#include <boost/detail/lightweight_thread.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/smart_ptr/detail/atomic_count.hpp>

boost::detail::atomic_count count( 0 );

void f()
{
    ++count;
}

int main()
{
    int const N = 4;
    boost::detail::lw_thread_t th[ N ] = {};

    for( int i = 0; i < N; ++i )
    {
        boost::detail::lw_thread_create( th[ i ], f );
    }

    for( int i = 0; i < N; ++i )
    {
        boost::detail::lw_thread_join( th[ i ] );
    }

    BOOST_TEST_EQ( count, N );

    return boost::report_errors();
}
