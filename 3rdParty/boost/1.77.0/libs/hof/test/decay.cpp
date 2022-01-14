/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    decay.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/decay.hpp>
#include "test.hpp"

#define CHECK_DECAY(T) \
    STATIC_ASSERT_SAME(decltype(boost::hof::decay(std::declval<T>())), std::decay<T>::type)

BOOST_HOF_TEST_CASE()
{
    CHECK_DECAY(int);
    CHECK_DECAY(int*);
    CHECK_DECAY(int&);
    CHECK_DECAY(int&&);
    CHECK_DECAY(const int&);
    CHECK_DECAY(int[2]);
    CHECK_DECAY(int(int));
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::decay(3) == 3);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::decay(3) == 3);
}
