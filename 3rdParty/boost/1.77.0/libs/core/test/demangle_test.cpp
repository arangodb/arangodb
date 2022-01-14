//
// Trivial test for core::demangle
//
// Copyright (c) 2014 Peter Dimov
// Copyright (c) 2014 Andrey Semashev
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/core/demangle.hpp>
#include <typeinfo>
#include <iostream>

template<class T1, class T2> struct Y1
{
};

void test_demangle()
{
    typedef Y1<int, long> T;
    std::cout << boost::core::demangle( typeid( T ).name() ) << std::endl;
}

void test_demangle_alloc()
{
    typedef Y1<int, long> T;
    const char* p = boost::core::demangle_alloc( typeid( T ).name() );
    if (p)
    {
        std::cout << p << std::endl;
        boost::core::demangle_free(p);
    }
    else
    {
        std::cout << "[demangling failed]" << std::endl;
    }
}

void test_scoped_demangled_name()
{
    typedef Y1<int, long> T;
    boost::core::scoped_demangled_name demangled_name( typeid( T ).name() );
    const char* p = demangled_name.get();
    if (p)
    {
        std::cout << p << std::endl;
    }
    else
    {
        std::cout << "[demangling failed]" << std::endl;
    }
}

int main()
{
    test_demangle();
    test_demangle_alloc();
    test_scoped_demangled_name();
    return 0;
}
