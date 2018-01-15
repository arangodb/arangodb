
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>

#include <boost/range.hpp>
#include <boost/coroutine/all.hpp>

void fibonacci( boost::coroutines::asymmetric_coroutine< int >::push_type & sink)
{
    int first = 1, second = 1;
    sink( first);     
    sink( second);     
    while ( true)
    {
        int third = first + second;
        first = second;
        second = third;
        sink( third);     
    }
}

int main()
{
    boost::coroutines::asymmetric_coroutine< int >::pull_type source( fibonacci);
    boost::range_iterator<
       boost::coroutines::asymmetric_coroutine< int >::pull_type
    >::type   it( boost::begin( source) );
    for ( int i = 0; i < 10; ++i)
    {
        std::cout << * it <<  " ";
        ++it;
    }

    std::cout << "\nDone" << std::endl;

    return EXIT_SUCCESS;
}
