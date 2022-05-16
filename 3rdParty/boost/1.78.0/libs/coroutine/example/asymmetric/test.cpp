#include <boost/coroutine/all.hpp>

#include <boost/bind.hpp>

#include "X.h"

typedef boost::coroutines::asymmetric_coroutine< X& >::pull_type pull_coro_t;
typedef boost::coroutines::asymmetric_coroutine< X& >::push_type push_coro_t;

void foo1( push_coro_t & sink)
{
    for ( int i = 0; i < 10; ++i)
    {
        X x( i);
        sink( x);
    }
}

void foo2( pull_coro_t & source)
{
    while ( source) {
        X & x = source.get();
        source();
    }
}

void bar()
{
    {
        pull_coro_t source( foo1);
        while ( source) {
            X & x = source.get();
            source();
        }
    }
    {
        push_coro_t sink( foo2);
        for ( int i = 0; i < 10; ++i)
        {
            X x( i);
            sink( x);
        }
    }
}

