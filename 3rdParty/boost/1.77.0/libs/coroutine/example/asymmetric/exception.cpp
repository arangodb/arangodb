
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/coroutine/all.hpp>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <boost/bind.hpp>
#include <boost/throw_exception.hpp>

typedef boost::coroutines::asymmetric_coroutine< int >::pull_type pull_coro_t;
typedef boost::coroutines::asymmetric_coroutine< int >::push_type push_coro_t;

struct my_exception : public std::runtime_error
{
    my_exception( std::string const& str) :
        std::runtime_error( str)
    {}
};

void echo( push_coro_t & sink, int j)
{
    for ( int i = 0; i < j; ++i)
    {
        if ( i == 5) boost::throw_exception( my_exception("abc") );
        sink( i);
    }
}

int main( int argc, char * argv[])
{
    pull_coro_t source( boost::bind( echo, _1, 10) );
    try
    {
        while ( source)
        {
            std::cout << source.get() << std::endl;
            source();
        }
    }
    catch ( my_exception const& ex)
    { std::cout << "exception: " << ex.what() << std::endl; }

    std::cout << "\nDone" << std::endl;

    return EXIT_SUCCESS;
}
