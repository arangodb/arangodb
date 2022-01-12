// Test for quick_exit.hpp
//
// Copyright 2018 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER)
# pragma warning(disable: 4702) // unreachable code
#endif

#include <boost/core/quick_exit.hpp>

int main()
{
    boost::quick_exit( 1 );
    return 0;
}
