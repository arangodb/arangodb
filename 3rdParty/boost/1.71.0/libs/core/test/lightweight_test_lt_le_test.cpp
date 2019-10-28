//
// Test for BOOST_TEST_LT, BOOST_TEST_LE
//
// Copyright 2014, 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/core/lightweight_test.hpp>

int main()
{
    int x = 0;

    BOOST_TEST_LT( x, 1 );
    BOOST_TEST_LT( ++x, 2 );
    BOOST_TEST_LT( x++, 3 );

    BOOST_TEST_LE( x, 2 );
    BOOST_TEST_LE( ++x, 3 );
    BOOST_TEST_LE( x++, 4 );

    int y = 3;

    BOOST_TEST_LT( ++y, ++x );
    BOOST_TEST_LT( y++, x++ );

    BOOST_TEST_LE( ++y, x );
    BOOST_TEST_LE( y++, x++ );

    return boost::report_errors();
}
