
// Test for quick_exit.hpp
//
// Copyright 2018 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/core/quick_exit.hpp>

int main()
{
    boost::quick_exit( 0 );
    return 1;
}
