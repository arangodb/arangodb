/*
Copyright 2018 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/config.hpp>
#if !defined(BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION)
#include <boost/core/empty_value.hpp>
#include <boost/core/lightweight_test.hpp>

struct T1 { };

struct S1
    : boost::empty_value<T1, 0, true> { };

struct T2 {
    int value;
};

struct S2
    : boost::empty_value<T1, 0, true>
    , boost::empty_value<T2, 1, true> { };

struct S3
    : boost::empty_value<T1, 0, false>
    , boost::empty_value<T2, 1, true> { };

struct T3 { };

struct S4
    : boost::empty_value<T1, 0, true>
    , boost::empty_value<T3, 1, true> { };

struct S5
    : boost::empty_value<T1, 0, false>
    , boost::empty_value<T3, 1, false> { };

struct S6
    : boost::empty_value<T1, 0, true>
    , boost::empty_value<T2, 1, true>
    , boost::empty_value<T3, 2, true> { };

int main()
{
    BOOST_TEST(sizeof(S1) == sizeof(T1));
    BOOST_TEST(sizeof(S2) == sizeof(T2));
    BOOST_TEST(sizeof(S3) > sizeof(T2));
    BOOST_TEST(sizeof(S4) == sizeof(T1));
    BOOST_TEST(sizeof(S5) > sizeof(T1));
    BOOST_TEST(sizeof(S6) == sizeof(T2));
    return boost::report_errors();
}
#else
int main()
{
    return 0;
}
#endif
