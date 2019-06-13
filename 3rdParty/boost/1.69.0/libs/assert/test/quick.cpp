//
// quick.cpp - a quick test for boost/assert.hpp
//
// Copyright 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/assert.hpp>

int main()
{
    int x = 1;
    BOOST_ASSERT( x == 1 );
}
