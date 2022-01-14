//
// Negative test for BOOST_TEST_THROWS
//
// Copyright (c) 2014 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/core/lightweight_test.hpp>

struct X
{
};

void f()
{
    throw 5;
}

int main()
{
    BOOST_TEST_THROWS( f(), X );

    return boost::report_errors();
}
