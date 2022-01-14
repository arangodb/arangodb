// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/lambda2.hpp>
#include <boost/core/lightweight_test.hpp>
#include <functional>

#define TEST_COMPOUND(Op) \
    BOOST_TEST_EQ( (_1 Op 1)(x1), y1 Op 1 ); \
    BOOST_TEST_EQ( (_1 Op _2)(x1, x2), y1 Op y2 ); \
    BOOST_TEST_EQ( ((_1 Op _2) Op _3)(x1, x2, x3), (y1 Op y2) Op y3 );

int main()
{
    using namespace boost::lambda2;

    int x1 = 1, x2 = 2, x3 = 3, y1 = 1, y2 = 2, y3 = 3;

    TEST_COMPOUND(+=)
    TEST_COMPOUND(-=)
    TEST_COMPOUND(*=)
    TEST_COMPOUND(/=)
    TEST_COMPOUND(%=)
    TEST_COMPOUND(&=)
    TEST_COMPOUND(|=)
    TEST_COMPOUND(^=)
    TEST_COMPOUND(<<=)
    TEST_COMPOUND(>>=)

    return boost::report_errors();
}
