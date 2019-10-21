
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>

#include <boost/bind.hpp>
#include <boost/coroutine/all.hpp>

typedef boost::coroutines::symmetric_coroutine< void >  coro_t;

coro_t::call_type * c1 = 0;
coro_t::call_type * c2 = 0;

void foo( coro_t::yield_type & yield)
{
    std::cout << "foo1" << std::endl;
    yield( * c2);
    std::cout << "foo2" << std::endl;
    yield( * c2);
    std::cout << "foo3" << std::endl;
}

void bar( coro_t::yield_type & yield)
{
    std::cout << "bar1" << std::endl;
    yield( * c1);
    std::cout << "bar2" << std::endl;
    yield( * c1);
    std::cout << "bar3" << std::endl;
}

int main( int argc, char * argv[])
{
    coro_t::call_type coro1( foo);
    coro_t::call_type coro2( bar);
    c1 = & coro1;
    c2 = & coro2;
    coro1();
    std::cout << "Done" << std::endl;

    return EXIT_SUCCESS;
}
