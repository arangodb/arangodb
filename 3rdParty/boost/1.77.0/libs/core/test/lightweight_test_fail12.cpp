//
// Negative test for BOOST_TEST_TRAIT_SAME
//
// Copyright 2014, 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/core/lightweight_test_trait.hpp>
#include <boost/config.hpp>

struct X
{
    typedef int type;
};

template<class T1, class T2> struct Y
{
    typedef T1 type;
};

typedef int I1;
typedef const int I2;
typedef volatile int I3;
typedef const volatile int I4;
typedef int& I5;
typedef const int& I6;
typedef volatile int& I7;
typedef const volatile int& I8;
#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
typedef int&& I9;
typedef const int&& I10;
typedef volatile int&& I11;
typedef const volatile int&& I12;
#endif

int main()
{
    BOOST_TEST_TRAIT_SAME(char[1], char[2]);
    BOOST_TEST_TRAIT_SAME(char[1], char[]);
    BOOST_TEST_TRAIT_SAME(char[1], char*);
    BOOST_TEST_TRAIT_SAME(void(), void(int));
    BOOST_TEST_TRAIT_SAME(void(), void(*)());
    BOOST_TEST_TRAIT_SAME(X, void);
    BOOST_TEST_TRAIT_SAME(X::type, void);
    BOOST_TEST_TRAIT_SAME(X, Y<void, void>);
    BOOST_TEST_TRAIT_SAME(X::type, Y<float, int>::type);
    BOOST_TEST_TRAIT_SAME(Y<int, float>, Y<int, double>);
    BOOST_TEST_TRAIT_SAME(I1, I2);
    BOOST_TEST_TRAIT_SAME(I3, I4);
    BOOST_TEST_TRAIT_SAME(I5, I6);
    BOOST_TEST_TRAIT_SAME(I7, I8);

    int expected = 14;

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)

    BOOST_TEST_TRAIT_SAME(I9, I10);
    BOOST_TEST_TRAIT_SAME(I11, I12);

    expected += 2;

#endif

    return boost::report_errors() == expected;
}
