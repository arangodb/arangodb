// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/lambda2.hpp>
#include <boost/core/lightweight_test.hpp>
#include <functional>

int main()
{
    using namespace boost::lambda2;

    int x[] = { 1, 2, 3, 4 };

    BOOST_TEST_EQ( _1[0](x), x[0] );
    BOOST_TEST_EQ( _1[_2](x, 1), x[1] );
    BOOST_TEST_EQ( (_1[_2] + _3[_4])(x, 2, x, 3), x[2] + x[3] );
    BOOST_TEST_EQ( std::bind(_1 + _2, _1[_2], _3[_4])(x, 2, x, 3), x[2] + x[3] );

    _1[0](x) = 7;
    BOOST_TEST_EQ( x[0], 7 );

    _1[_2](x, 1) = 8;
    BOOST_TEST_EQ( x[1], 8 );

    return boost::report_errors();
}
