//
// quick.cpp - a quick test for boost/bind.hpp
//
// Copyright 2017 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/bind.hpp>
#include <boost/core/lightweight_test.hpp>

int f( int a, int b, int c )
{
    return a + 10 * b + 100 * c;
}

int main()
{
    int const i = 1;

    BOOST_TEST_EQ( boost::bind( f, _1, 2, 3 )( i ), 321 );

    return boost::report_errors();
}
