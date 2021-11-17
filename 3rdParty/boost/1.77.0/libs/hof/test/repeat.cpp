/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    repeat.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/repeat.hpp>
#include <limits>
#include "test.hpp"

// TODO: Add tests for multiple parameters

struct increment
{
    template<class T>
    constexpr T operator()(T x) const noexcept
    {
        return x + 1;
    }
};

#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(boost::hof::repeat(std::integral_constant<int, 5>())(increment())(1)), "noexcept repeat");
    static_assert(noexcept(boost::hof::repeat(5)(increment())(1)), "noexcept repeat");
}
#endif

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::repeat(std::integral_constant<int, 5>())(increment())(1) == 6);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::repeat(std::integral_constant<int, 5>())(increment())(1) == 6);
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::repeat(5)(increment())(1) == 6);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::repeat(5)(increment())(1) == 6);
}

BOOST_HOF_TEST_CASE()
{
    int i = 5;
    BOOST_HOF_TEST_CHECK(boost::hof::repeat(i)(increment())(1) == 6);
}

BOOST_HOF_TEST_CASE()
{
    static const int i = 5;
    BOOST_HOF_TEST_CHECK(boost::hof::repeat(i)(increment())(1) == 6);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::repeat(i)(increment())(1) == 6);
}

// BOOST_HOF_TEST_CASE()
// {
//     BOOST_HOF_TEST_CHECK(boost::hof::repeat(std::numeric_limits<int>::max()/4)(increment())(0) == std::numeric_limits<int>::max()/4);
// }

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::repeat(BOOST_HOF_RECURSIVE_CONSTEXPR_DEPTH+4)(increment())(0) == BOOST_HOF_RECURSIVE_CONSTEXPR_DEPTH+4);
#if BOOST_HOF_HAS_RELAXED_CONSTEXPR
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::repeat(BOOST_HOF_RECURSIVE_CONSTEXPR_DEPTH+4)(increment())(0) == BOOST_HOF_RECURSIVE_CONSTEXPR_DEPTH+4);
#endif
}
