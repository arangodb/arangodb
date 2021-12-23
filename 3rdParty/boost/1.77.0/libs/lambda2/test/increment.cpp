// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/lambda2.hpp>
#include <boost/core/lightweight_test.hpp>
#include <functional>

int main()
{
    using namespace boost::lambda2;

    int x1 = 1, x2 = 2, y1 = 1, y2 = 2;

    BOOST_TEST_EQ( (_1++)(x1), y1++ );
    BOOST_TEST_EQ( (++_1)(x1), ++y1 );

    BOOST_TEST_EQ( (_1--)(x1), y1-- );
    BOOST_TEST_EQ( (--_1)(x1), --y1 );

    BOOST_TEST_EQ( (_1++ + _2++)(x1, x2), y1++ + y2++ );
    BOOST_TEST_EQ( (++_1 + ++_2)(x1, x2), ++y1 + ++y2 );

    BOOST_TEST_EQ( (_1-- + _2--)(x1, x2), y1-- + y2-- );
    BOOST_TEST_EQ( (--_1 + --_2)(x1, x2), --y1 + --y2 );

    BOOST_TEST_EQ( std::bind(_1 + _2, _1++, _2++)(x1, x2), y1++ + y2++ );
    BOOST_TEST_EQ( std::bind(_1 + _2, ++_1, ++_2)(x1, x2), ++y1 + ++y2 );

    BOOST_TEST_EQ( std::bind(_1 + _2, _1--, _2--)(x1, x2), y1-- + y2-- );
    BOOST_TEST_EQ( std::bind(_1 + _2, --_1, --_2)(x1, x2), --y1 + --y2 );

    return boost::report_errors();
}
