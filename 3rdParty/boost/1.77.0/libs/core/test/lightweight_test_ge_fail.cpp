//
// Negative test for BOOST_TEST_GE
//
// Copyright 2017 Kohei Takahashi
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/core/lightweight_test.hpp>

int main()
{
    int x = 0;

    BOOST_TEST_GE( x, 1 );

    return boost::report_errors();
}
