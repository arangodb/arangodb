/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    tap.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/tap.hpp>
#include "test.hpp"

struct sum_f
{
    template<class T, class U>
    constexpr T operator()(T x, U y) const
    {
        return x+y;
    }
};

static constexpr boost::hof::pipable_adaptor<sum_f> sum = {};
// TODO: Test constexpr
BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == (1 | sum(2)));
    BOOST_HOF_TEST_CHECK(5 == (1 | sum(2) | boost::hof::tap([](int i) { BOOST_HOF_TEST_CHECK(3 == i); }) | sum(2)));
}
