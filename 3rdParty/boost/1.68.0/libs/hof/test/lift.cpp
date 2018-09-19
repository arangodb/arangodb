/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    lift.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include "test.hpp"
#include <boost/hof/lift.hpp>
#include <boost/hof/function.hpp>
#include <boost/hof/detail/move.hpp>
#include <tuple>
#include <algorithm>

template<class T, class U>
constexpr T sum(T x, U y) BOOST_HOF_RETURNS_DEDUCE_NOEXCEPT(x+y)
{
    return x + y;
}

BOOST_HOF_LIFT_CLASS(max_f, std::max);
BOOST_HOF_LIFT_CLASS(sum_f, sum);

#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(sum_f()(1, 2)), "noexcept lift");
    static_assert(!noexcept(sum_f()(std::string(), std::string())), "noexcept lift");
}
#endif

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(max_f()(3, 4) == std::max(3, 4));

    BOOST_HOF_TEST_CHECK(sum_f()(1, 2) == 3);
    BOOST_HOF_STATIC_TEST_CHECK(sum_f()(1, 2) == 3);
}

#if BOOST_HOF_HAS_GENERIC_LAMBDA
BOOST_HOF_TEST_CASE()
{
    auto my_max = BOOST_HOF_LIFT(std::max);
    BOOST_HOF_TEST_CHECK(my_max(3, 4) == std::max(3, 4));
    
    BOOST_HOF_TEST_CHECK(BOOST_HOF_LIFT(std::max)(3, 4) == std::max(3, 4));
    BOOST_HOF_TEST_CHECK(BOOST_HOF_LIFT(sum)(1, 2) == 3);
}

BOOST_HOF_STATIC_FUNCTION(psum) = BOOST_HOF_LIFT(sum);
BOOST_HOF_STATIC_FUNCTION(pmax) = BOOST_HOF_LIFT(std::max);

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(pmax(3, 4) == std::max(3, 4));

    BOOST_HOF_TEST_CHECK(psum(1, 2) == 3);
}
#endif
