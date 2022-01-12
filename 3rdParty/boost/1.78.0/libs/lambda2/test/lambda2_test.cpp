// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER) && _MSC_VER == 1900
# pragma warning(disable: 4552) // '<<': operator has no effect; expected operator with side-effect
#endif

#include <boost/lambda2.hpp>
#include <boost/core/lightweight_test.hpp>

#define TEST_UNARY(Op) \
    BOOST_TEST_EQ( (Op _1)( 0 ), Op 0 ); \
    BOOST_TEST_EQ( (Op _1)( 1 ), Op 1 ); \
    BOOST_TEST_EQ( (Op _1)( 5 ), Op 5 ); \
    BOOST_TEST_EQ( (Op (Op _1))( 5 ), Op (Op 5) ); \
    BOOST_TEST_EQ( ((Op _1) + (Op _2))( 1, 2 ), (Op 1) + (Op 2) ); \
    BOOST_TEST_EQ( ((Op _1) && (Op _2))( 1, 2 ), (Op 1) && (Op 2) );

#define TEST_BINARY(Op) \
    BOOST_TEST_EQ( (_1 Op _2)( 0, 1 ), 0 Op 1 ); \
    BOOST_TEST_EQ( (_1 Op _2)( 3, 5 ), 3 Op 5 ); \
    BOOST_TEST_EQ( (_1 Op 2)( 1, 2 ), 1 Op 2 ); \
    BOOST_TEST_EQ( (1 Op _2)( 1, 2 ), 1 Op 2 ); \
    BOOST_TEST_EQ( ((_1 Op _2) + (_3 Op _4))( 1, 2, 3, 4 ), (1 Op 2) + (3 Op 4) );

int main()
{
    using namespace boost::lambda2;

    TEST_BINARY(+)
    TEST_BINARY(-)
    TEST_BINARY(*)
    TEST_BINARY(/)
    TEST_BINARY(%)
    TEST_UNARY(-)

    TEST_BINARY(==)
    TEST_BINARY(!=)
    TEST_BINARY(>)
    TEST_BINARY(<)
    TEST_BINARY(>=)
    TEST_BINARY(<=)

    TEST_BINARY(&&)
    TEST_BINARY(||)
    TEST_UNARY(!)

    TEST_BINARY(&)
    TEST_BINARY(|)
    TEST_BINARY(^)
    TEST_UNARY(~)

#if defined(_MSC_VER) && _MSC_VER == 1900 && !defined(_DEBUG)

    // prevents crash in TEST_BINARY(/)
    // no idea why

    BOOST_TEST_EQ( (_1 / _2)( 0, 1 ), 0 / 1 );

#endif

    TEST_BINARY(<<)
    TEST_BINARY(>>)

    TEST_UNARY(+)

    return boost::report_errors();
}
