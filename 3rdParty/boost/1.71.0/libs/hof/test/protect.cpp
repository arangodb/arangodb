/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    protect.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/protect.hpp>
#include <boost/hof/lazy.hpp>
#include <boost/hof/placeholders.hpp>
#include <memory>
#include "test.hpp"

#include <boost/hof/function.hpp>

int f(int x)
{
    return x;
}

int& g(int& x)
{
    return x;
}

template<class T>
const T& constify(const T& arg)
{
    return arg;
}

BOOST_HOF_TEST_CASE()
{
    int i[9] = {0,1,2,3,4,5,6,7,8};

    // non-const

    // test nullary
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(1))() == 1);

    // test lvalues

    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1))(i[0]) == &i[0]);

    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1))(i[0], i[1]) == &i[0]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_2))(i[0], i[1]) == &i[1]);

    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1))(i[0], i[1], i[2]) == &i[0]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_2))(i[0], i[1], i[2]) == &i[1]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_3))(i[0], i[1], i[2]) == &i[2]);

    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1))(i[0], i[1], i[2], i[3]) == &i[0]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_2))(i[0], i[1], i[2], i[3]) == &i[1]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_3))(i[0], i[1], i[2], i[3]) == &i[2]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_4))(i[0], i[1], i[2], i[3]) == &i[3]);

    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1))(i[0], i[1], i[2], i[3], i[4]) == &i[0]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_2))(i[0], i[1], i[2], i[3], i[4]) == &i[1]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_3))(i[0], i[1], i[2], i[3], i[4]) == &i[2]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_4))(i[0], i[1], i[2], i[3], i[4]) == &i[3]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_5))(i[0], i[1], i[2], i[3], i[4]) == &i[4]);

    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1))(i[0], i[1], i[2], i[3], i[4], i[5]) == &i[0]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_2))(i[0], i[1], i[2], i[3], i[4], i[5]) == &i[1]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_3))(i[0], i[1], i[2], i[3], i[4], i[5]) == &i[2]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_4))(i[0], i[1], i[2], i[3], i[4], i[5]) == &i[3]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_5))(i[0], i[1], i[2], i[3], i[4], i[5]) == &i[4]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_6))(i[0], i[1], i[2], i[3], i[4], i[5]) == &i[5]);

    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1))(i[0], i[1], i[2], i[3], i[4], i[5], i[6]) == &i[0]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_2))(i[0], i[1], i[2], i[3], i[4], i[5], i[6]) == &i[1]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_3))(i[0], i[1], i[2], i[3], i[4], i[5], i[6]) == &i[2]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_4))(i[0], i[1], i[2], i[3], i[4], i[5], i[6]) == &i[3]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_5))(i[0], i[1], i[2], i[3], i[4], i[5], i[6]) == &i[4]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_6))(i[0], i[1], i[2], i[3], i[4], i[5], i[6]) == &i[5]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_7))(i[0], i[1], i[2], i[3], i[4], i[5], i[6]) == &i[6]);

    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7]) == &i[0]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_2))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7]) == &i[1]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_3))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7]) == &i[2]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_4))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7]) == &i[3]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_5))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7]) == &i[4]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_6))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7]) == &i[5]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_7))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7]) == &i[6]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_8))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7]) == &i[7]);

    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[0]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_2))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[1]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_3))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[2]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_4))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[3]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_5))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[4]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_6))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[5]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_7))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[6]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_8))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[7]);
    BOOST_HOF_TEST_CHECK(&boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_9))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[8]);

    // test rvalues

    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1))(0) == 0);

    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1))(0, 1) == 0);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2))(0, 1) == 1);

    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1))(0, 1, 2) == 0);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2))(0, 1, 2) == 1);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_3))(0, 1, 2) == 2);

    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1))(0, 1, 2, 3) == 0);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2))(0, 1, 2, 3) == 1);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_3))(0, 1, 2, 3) == 2);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_4))(0, 1, 2, 3) == 3);

    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1))(0, 1, 2, 3, 4) == 0);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2))(0, 1, 2, 3, 4) == 1);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_3))(0, 1, 2, 3, 4) == 2);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_4))(0, 1, 2, 3, 4) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_5))(0, 1, 2, 3, 4) == 4);

    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1))(0, 1, 2, 3, 4, 5) == 0);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2))(0, 1, 2, 3, 4, 5) == 1);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_3))(0, 1, 2, 3, 4, 5) == 2);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_4))(0, 1, 2, 3, 4, 5) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_5))(0, 1, 2, 3, 4, 5) == 4);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_6))(0, 1, 2, 3, 4, 5) == 5);

    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1))(0, 1, 2, 3, 4, 5, 6) == 0);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2))(0, 1, 2, 3, 4, 5, 6) == 1);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_3))(0, 1, 2, 3, 4, 5, 6) == 2);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_4))(0, 1, 2, 3, 4, 5, 6) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_5))(0, 1, 2, 3, 4, 5, 6) == 4);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_6))(0, 1, 2, 3, 4, 5, 6) == 5);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_7))(0, 1, 2, 3, 4, 5, 6) == 6);

    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1))(0, 1, 2, 3, 4, 5, 6, 7) == 0);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2))(0, 1, 2, 3, 4, 5, 6, 7) == 1);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_3))(0, 1, 2, 3, 4, 5, 6, 7) == 2);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_4))(0, 1, 2, 3, 4, 5, 6, 7) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_5))(0, 1, 2, 3, 4, 5, 6, 7) == 4);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_6))(0, 1, 2, 3, 4, 5, 6, 7) == 5);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_7))(0, 1, 2, 3, 4, 5, 6, 7) == 6);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_8))(0, 1, 2, 3, 4, 5, 6, 7) == 7);

    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 0);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 1);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_3))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 2);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_4))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_5))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 4);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_6))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 5);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_7))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 6);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_8))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 7);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_9))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 8);

    // test mixed perfect forwarding
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1))(i[0], 1) == 0);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2))(i[0], 1) == 1);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1))(0, i[1]) == 0);
    BOOST_HOF_TEST_CHECK(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2))(0, i[1]) == 1);

    // const

    // test nullary
    BOOST_HOF_TEST_CHECK(constify(constify(boost::hof::protect(boost::hof::lazy(f)(1))))() == 1);

    // test lvalues
    BOOST_HOF_TEST_CHECK(&constify(constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1))))(i[0]) == &i[0]);

    BOOST_HOF_TEST_CHECK(&constify(constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1))))(i[0], i[1]) == &i[0]);
    BOOST_HOF_TEST_CHECK(&constify(constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_2))))(i[0], i[1]) == &i[1]);

    BOOST_HOF_TEST_CHECK(&constify(constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1))))(i[0], i[1], i[2]) == &i[0]);
    BOOST_HOF_TEST_CHECK(&constify(constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_2))))(i[0], i[1], i[2]) == &i[1]);
    BOOST_HOF_TEST_CHECK(&constify(constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_3))))(i[0], i[1], i[2]) == &i[2]);

    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1)))(i[0], i[1], i[2], i[3]) == &i[0]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_2)))(i[0], i[1], i[2], i[3]) == &i[1]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_3)))(i[0], i[1], i[2], i[3]) == &i[2]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_4)))(i[0], i[1], i[2], i[3]) == &i[3]);

    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1)))(i[0], i[1], i[2], i[3], i[4]) == &i[0]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_2)))(i[0], i[1], i[2], i[3], i[4]) == &i[1]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_3)))(i[0], i[1], i[2], i[3], i[4]) == &i[2]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_4)))(i[0], i[1], i[2], i[3], i[4]) == &i[3]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_5)))(i[0], i[1], i[2], i[3], i[4]) == &i[4]);

    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1)))(i[0], i[1], i[2], i[3], i[4], i[5]) == &i[0]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_2)))(i[0], i[1], i[2], i[3], i[4], i[5]) == &i[1]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_3)))(i[0], i[1], i[2], i[3], i[4], i[5]) == &i[2]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_4)))(i[0], i[1], i[2], i[3], i[4], i[5]) == &i[3]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_5)))(i[0], i[1], i[2], i[3], i[4], i[5]) == &i[4]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_6)))(i[0], i[1], i[2], i[3], i[4], i[5]) == &i[5]);

    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6]) == &i[0]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_2)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6]) == &i[1]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_3)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6]) == &i[2]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_4)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6]) == &i[3]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_5)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6]) == &i[4]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_6)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6]) == &i[5]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_7)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6]) == &i[6]);

    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7]) == &i[0]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_2)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7]) == &i[1]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_3)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7]) == &i[2]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_4)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7]) == &i[3]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_5)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7]) == &i[4]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_6)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7]) == &i[5]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_7)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7]) == &i[6]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_8)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7]) == &i[7]);

    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_1)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[0]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_2)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[1]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_3)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[2]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_4)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[3]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_5)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[4]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_6)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[5]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_7)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[6]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_8)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[7]);
    BOOST_HOF_TEST_CHECK(&constify(boost::hof::protect(boost::hof::lazy(g)(std::placeholders::_9)))(i[0], i[1], i[2], i[3], i[4], i[5], i[6], i[7], i[8]) == &i[8]);

    // test rvalues

    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1)))(0) == 0);

    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1)))(0, 1) == 0);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2)))(0, 1) == 1);

    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1)))(0, 1, 2) == 0);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2)))(0, 1, 2) == 1);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_3)))(0, 1, 2) == 2);

    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1)))(0, 1, 2, 3) == 0);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2)))(0, 1, 2, 3) == 1);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_3)))(0, 1, 2, 3) == 2);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_4)))(0, 1, 2, 3) == 3);

    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1)))(0, 1, 2, 3, 4) == 0);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2)))(0, 1, 2, 3, 4) == 1);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_3)))(0, 1, 2, 3, 4) == 2);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_4)))(0, 1, 2, 3, 4) == 3);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_5)))(0, 1, 2, 3, 4) == 4);

    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1)))(0, 1, 2, 3, 4, 5) == 0);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2)))(0, 1, 2, 3, 4, 5) == 1);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_3)))(0, 1, 2, 3, 4, 5) == 2);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_4)))(0, 1, 2, 3, 4, 5) == 3);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_5)))(0, 1, 2, 3, 4, 5) == 4);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_6)))(0, 1, 2, 3, 4, 5) == 5);

    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1)))(0, 1, 2, 3, 4, 5, 6) == 0);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2)))(0, 1, 2, 3, 4, 5, 6) == 1);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_3)))(0, 1, 2, 3, 4, 5, 6) == 2);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_4)))(0, 1, 2, 3, 4, 5, 6) == 3);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_5)))(0, 1, 2, 3, 4, 5, 6) == 4);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_6)))(0, 1, 2, 3, 4, 5, 6) == 5);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_7)))(0, 1, 2, 3, 4, 5, 6) == 6);

    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1)))(0, 1, 2, 3, 4, 5, 6, 7) == 0);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2)))(0, 1, 2, 3, 4, 5, 6, 7) == 1);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_3)))(0, 1, 2, 3, 4, 5, 6, 7) == 2);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_4)))(0, 1, 2, 3, 4, 5, 6, 7) == 3);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_5)))(0, 1, 2, 3, 4, 5, 6, 7) == 4);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_6)))(0, 1, 2, 3, 4, 5, 6, 7) == 5);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_7)))(0, 1, 2, 3, 4, 5, 6, 7) == 6);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_8)))(0, 1, 2, 3, 4, 5, 6, 7) == 7);

    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1)))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 0);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2)))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 1);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_3)))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 2);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_4)))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 3);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_5)))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 4);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_6)))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 5);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_7)))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 6);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_8)))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 7);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_9)))(0, 1, 2, 3, 4, 5, 6, 7, 8) == 8);

    // test mixed perfect forwarding
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1)))(i[0], 1) == 0);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2)))(i[0], 1) == 1);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_1)))(0, i[1]) == 0);
    BOOST_HOF_TEST_CHECK(constify(boost::hof::protect(boost::hof::lazy(f)(std::placeholders::_2)))(0, i[1]) == 1);
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::lazy(boost::hof::apply)(boost::hof::protect(boost::hof::lazy(boost::hof::identity)(boost::hof::_1)), boost::hof::_1)(17) == 17);
    BOOST_HOF_TEST_CHECK(boost::hof::lazy(boost::hof::apply)(boost::hof::protect(boost::hof::lazy(boost::hof::identity)(boost::hof::_1)), 17)() == 17);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::lazy(boost::hof::apply)(boost::hof::protect(boost::hof::lazy(boost::hof::identity)(boost::hof::_1)), boost::hof::_1)(17) == 17);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::lazy(boost::hof::apply)(boost::hof::protect(boost::hof::lazy(boost::hof::identity)(boost::hof::_1)), 17)() == 17);
}

namespace test1 {

int id(int x)
{
    return x;
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::lazy(boost::hof::apply)(boost::hof::protect(boost::hof::lazy(id)(std::placeholders::_1)), std::placeholders::_1)(17) == 17);
    BOOST_HOF_TEST_CHECK(boost::hof::lazy(boost::hof::apply)(boost::hof::protect(boost::hof::lazy(id)(std::placeholders::_1)), 17)() == 17);
}

}
