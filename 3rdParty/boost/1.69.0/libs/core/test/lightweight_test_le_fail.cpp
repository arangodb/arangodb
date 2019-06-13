//
// Negative test for BOOST_TEST_LE
//
// Copyright 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/core/lightweight_test.hpp>

int main()
{
    int x = 1;

    BOOST_TEST_LE( x, 0 );

    return boost::report_errors();
}
