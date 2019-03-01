//
// Test that implicit_cast requires a template argument
//
// Copyright 2014 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/implicit_cast.hpp>

int main()
{
    int x = boost::implicit_cast( 1 );
    (void)x;

    return 0;
}
