#include <boost/coroutine/all.hpp>

#include <cstdlib>
#include <iostream>

#include <boost/bind.hpp>

#include "X.h"

typedef boost::coroutines::asymmetric_coroutine< X& >::pull_type pull_coro_t;
typedef boost::coroutines::asymmetric_coroutine< X& >::push_type push_coro_t;

void fn1( push_coro_t & sink)
{
    for ( int i = 0; i < 10; ++i)
    {
        X x( i);
        sink( x);
    }
}

void fn2( pull_coro_t & source)
{
    while ( source) {
        X & x = source.get();
        std::cout << "i = " << x.i << std::endl;
        source();
    }
}

int main( int argc, char * argv[])
{
    {
        pull_coro_t source( fn1);
        while ( source) {
            X & x = source.get();
            std::cout << "i = " << x.i << std::endl;
            source();
        }
    }
    {
        push_coro_t sink( fn2);
        for ( int i = 0; i < 10; ++i)
        {
            X x( i);
            sink( x);
        }
    }
    std::cout << "Done" << std::endl;

    return EXIT_SUCCESS;
}

