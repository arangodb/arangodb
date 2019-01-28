/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    mutable.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/mutable.hpp>
#include <boost/hof/lazy.hpp>
#include <boost/hof/detail/move.hpp>
#include <memory>
#include "test.hpp"

struct mutable_fun
{
    int x;
    mutable_fun() noexcept
    : x(1)
    {}

    int operator()(int i) noexcept
    {
        x+=i;
        return x;
    }
};

#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(boost::hof::mutable_(mutable_fun())(3)), "noexcept mutable_");
}
#endif

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::mutable_(mutable_fun())(3) == 4);
}

BOOST_HOF_TEST_CASE()
{
    auto mut_fun = mutable_fun();
    auto by_5 = boost::hof::lazy(boost::hof::mutable_(std::ref(mut_fun)))(5);
    BOOST_HOF_TEST_CHECK(by_5() == 6);
    BOOST_HOF_TEST_CHECK(by_5() == 11);
}

struct mutable_move_fun
{
    std::unique_ptr<int> x;
    mutable_move_fun() : x(new int(1))
    {}

    int operator()(int i)
    {
        *x+=i;
        return *x;
    }
};

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::mutable_(mutable_move_fun())(3) == 4);
}

BOOST_HOF_TEST_CASE()
{
    auto mut_fun = mutable_move_fun();
    auto by_5 = boost::hof::lazy(boost::hof::mutable_(boost::hof::move(mut_fun)))(5);
    BOOST_HOF_TEST_CHECK(by_5() == 6);
    BOOST_HOF_TEST_CHECK(by_5() == 11);
}

