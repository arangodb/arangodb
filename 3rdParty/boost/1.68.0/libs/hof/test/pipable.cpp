/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    pipable.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/pipable.hpp>
#include <boost/hof/static.hpp>
#include <boost/hof/limit.hpp>
#include "test.hpp"

static constexpr boost::hof::static_<boost::hof::pipable_adaptor<binary_class> > binary_pipable = {};

static constexpr boost::hof::static_<boost::hof::pipable_adaptor<unary_class> > unary_pipable = {};

static constexpr boost::hof::static_<boost::hof::pipable_adaptor<mutable_class> > mutable_pipable = {};

static constexpr boost::hof::static_<boost::hof::pipable_adaptor<void_class> > void_pipable = {};

static constexpr boost::hof::static_<boost::hof::pipable_adaptor<mono_class> > mono_pipable = {};

static constexpr boost::hof::static_<boost::hof::pipable_adaptor<move_class> > move_pipable = {};

constexpr boost::hof::pipable_adaptor<void_class> void_pipable_constexpr = {};

constexpr boost::hof::pipable_adaptor<binary_class> binary_pipable_constexpr = {};

constexpr boost::hof::pipable_adaptor<unary_class> unary_pipable_constexpr = {};

#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(1 | binary_pipable(2)), "noexcept pipable");
    static_assert(noexcept(binary_pipable(1, 2)), "noexcept pipable");
}
#endif

BOOST_HOF_TEST_CASE()
{
    void_pipable(1);
    1 | void_pipable;
    BOOST_HOF_TEST_CHECK(3 == (1 | binary_pipable(2)));
    BOOST_HOF_TEST_CHECK(3 == (binary_pipable(1, 2)));
    BOOST_HOF_TEST_CHECK(3 == (3 | unary_pipable));
    BOOST_HOF_TEST_CHECK(3 == (3 | unary_pipable()));
    BOOST_HOF_TEST_CHECK(3 == (unary_pipable(3)));
    int i1 = 1;
    BOOST_HOF_TEST_CHECK(3 == (2 | binary_pipable(i1)));
    BOOST_HOF_TEST_CHECK(3 == (i1 | mutable_pipable(2)));
    BOOST_HOF_TEST_CHECK(3 == (i1));
    int i2 = 1;
    BOOST_HOF_TEST_CHECK(3 == (mutable_pipable(i2, 2)));
    BOOST_HOF_TEST_CHECK(3 == (i2));
    BOOST_HOF_TEST_CHECK(3 == (mono_pipable(2)));
    BOOST_HOF_TEST_CHECK(3 == (2| mono_pipable));
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == (move_class()(1, 2)));
    BOOST_HOF_TEST_CHECK(3 == (move_pipable(1, 2)));
    BOOST_HOF_TEST_CHECK(3 == (1 | move_pipable(2)));
    BOOST_HOF_TEST_CHECK(3 == (1 | boost::hof::pipable(move_class())(2)));
    BOOST_HOF_TEST_CHECK(3 == (boost::hof::pipable(move_class())(1, 2)));
}

BOOST_HOF_TEST_CASE()
{
    void_pipable_constexpr(1);
    1 | void_pipable_constexpr;
    BOOST_HOF_STATIC_TEST_CHECK(3 == (1 | binary_pipable_constexpr(2)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == (binary_pipable_constexpr(1, 2)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == (3 | unary_pipable_constexpr));
    BOOST_HOF_STATIC_TEST_CHECK(3 == (3 | unary_pipable_constexpr()));
    BOOST_HOF_STATIC_TEST_CHECK(3 == (unary_pipable_constexpr(3)));
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == boost::hof::limit_c<2>(binary_pipable_constexpr)(1, 2));
    BOOST_HOF_STATIC_TEST_CHECK(3 == boost::hof::limit_c<2>(binary_pipable_constexpr)(1, 2));
}

BOOST_HOF_TEST_CASE()
{
    auto f = boost::hof::pipable(boost::hof::limit_c<2>(binary_class()));
    static_assert(boost::hof::is_invocable<decltype(f), int, int>::value, "Passing the limit is not callable");
    static_assert(!boost::hof::is_invocable<decltype(f), int, int, int>::value, "Passing the limit is not callable");
    static_assert(!boost::hof::is_invocable<decltype(f), int, int, int, int>::value, "Passing the limit is not callable");
}
