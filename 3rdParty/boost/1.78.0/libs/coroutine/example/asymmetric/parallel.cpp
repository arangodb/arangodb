
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/coroutine/all.hpp>

#include <cstdlib>
#include <iostream>

#include <boost/bind.hpp>

void first( boost::coroutines::asymmetric_coroutine< void >::push_type & sink)
{
    std::cout << "started first! ";
    for ( int i = 0; i < 10; ++i)
    {
        sink();
        std::cout << "a" << i;
    }
}

void second( boost::coroutines::asymmetric_coroutine< void >::push_type & sink)
{
    std::cout << "started second! ";
    for ( int i = 0; i < 10; ++i)
    {
        sink();
        std::cout << "b" << i;
    }
}

int main( int argc, char * argv[])
{
    {
        boost::coroutines::asymmetric_coroutine< void >::pull_type source1( boost::bind( first, _1) );
        boost::coroutines::asymmetric_coroutine< void >::pull_type source2( boost::bind( second, _1) );
        while ( source1 && source2) {
            source1();
            std::cout << " ";
            source2();
            std::cout << " ";
        }
    }

    std::cout << "\nDone" << std::endl;

    return EXIT_SUCCESS;
}
