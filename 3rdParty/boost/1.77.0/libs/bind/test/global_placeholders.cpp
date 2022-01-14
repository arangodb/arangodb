// Copyright 2017, 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/bind.hpp>
#include <boost/core/lightweight_test.hpp>

//

int f( int a, int b, int c )
{
    return a + 10 * b + 100 * c;
}

int main()
{
    int const i = 1;

    BOOST_TEST_EQ( boost::bind( f, _1, 2, _2 )( i, 3 ), 321 );

    return boost::report_errors();
}
