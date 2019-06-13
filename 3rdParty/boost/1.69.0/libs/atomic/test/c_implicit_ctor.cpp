//  Copyright (c) 2018 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

// The test verifies that atomic<T> has an implicit conversion constructor from T.
// This can only be tested in C++17 because it has mandated copy elision. Previous C++ versions
// also require atomic<> to have a copy or move constructor, which it does not.
#if __cplusplus >= 201703L

#include <boost/atomic.hpp>
#include <boost/static_assert.hpp>
#include <boost/config.hpp>
#include <type_traits>

int main(int, char *[])
{
    static_assert(std::is_convertible< int, boost::atomic< int > >::value, "boost::atomic<T> does not have an implicit constructor from T");

    boost::atomic< short > a = 10;
    (void)a;

    return 0;
}

#else // __cplusplus >= 201703L

int main(int, char *[])
{
    return 0;
}

#endif // __cplusplus >= 201703L
