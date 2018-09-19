/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    limit.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/limit.hpp>
#include <boost/hof/is_invocable.hpp>
#include <boost/hof/pack.hpp>
#include "test.hpp"

BOOST_HOF_TEST_CASE()
{
    auto f = boost::hof::limit(std::integral_constant<int, 2>())(binary_class());
    BOOST_HOF_TEST_CHECK(f(1, 2) == 3);
    static_assert(boost::hof::function_param_limit<decltype(f)>::value == 2, "Function limit is 2");
}

BOOST_HOF_TEST_CASE()
{
    auto f = boost::hof::limit_c<2>(binary_class());
    BOOST_HOF_TEST_CHECK(f(1, 2) == 3);
    static_assert(boost::hof::function_param_limit<decltype(f)>::value == 2, "Function limit is 2");
}

BOOST_HOF_TEST_CASE()
{
    auto f = boost::hof::limit_c<2>(boost::hof::always(3));
    BOOST_HOF_TEST_CHECK(f(1, 2) == 3);
    BOOST_HOF_TEST_CHECK(f(1) == 3);
    BOOST_HOF_TEST_CHECK(f() == 3);
    static_assert(boost::hof::function_param_limit<decltype(f)>::value == 2, "Function limit is 2");
    static_assert(boost::hof::is_invocable<decltype(f), int>::value, "Invocable");
    static_assert(boost::hof::is_invocable<decltype(f), int, int>::value, "Invocable");
    static_assert(!boost::hof::is_invocable<decltype(f), int, int, int>::value, "Not Invocable");
}

BOOST_HOF_TEST_CASE()
{
    static_assert(!boost::hof::is_invocable<decltype(boost::hof::limit), int>::value, "Not integral constant");
}

BOOST_HOF_TEST_CASE()
{
    static_assert(boost::hof::function_param_limit<decltype(boost::hof::pack())>::value == 0, "Failed limit on pack");
    static_assert(boost::hof::function_param_limit<decltype(boost::hof::pack(1))>::value == 1, "Failed limit on pack");
    static_assert(boost::hof::function_param_limit<decltype(boost::hof::pack(1, 2))>::value == 2, "Failed limit on pack");
}

