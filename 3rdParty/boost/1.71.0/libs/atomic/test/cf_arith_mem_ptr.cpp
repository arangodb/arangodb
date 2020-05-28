//  Copyright (c) 2017 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/atomic.hpp>

struct foo
{
    int n;
};

int main(int, char *[])
{
    // The test verifies that atomic<> does not provide arithmetic operations on member pointers
    typedef int (foo::*mem_ptr);
    boost::atomic< mem_ptr > a;
    a.fetch_add(1);

    return 1;
}
