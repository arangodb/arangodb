//
// Test for BOOST_TEST_GT, BOOST_TEST_GE
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

    BOOST_TEST_GT( x, -1 );
    BOOST_TEST_GT( ++x, 0 );
    BOOST_TEST_GT( x++, 0 );

    BOOST_TEST_GE( x, 2 );
    BOOST_TEST_GE( ++x, 3 );
    BOOST_TEST_GE( x++, 3 );

    int y = 5;

    BOOST_TEST_GT( ++y, ++x );
    BOOST_TEST_GT( y++, x++ );

    BOOST_TEST_GE( ++y, x );
    BOOST_TEST_GE( y++, x++ );

    return boost::report_errors();
}
