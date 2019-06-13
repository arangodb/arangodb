/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    partial.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/partial.hpp>
#include <boost/hof/limit.hpp>
#include "test.hpp"

static constexpr boost::hof::static_<boost::hof::partial_adaptor<binary_class> > binary_partial = {};

static constexpr boost::hof::static_<boost::hof::partial_adaptor<unary_class> > unary_partial = {};

static constexpr boost::hof::static_<boost::hof::partial_adaptor<mutable_class> > mutable_partial = {};

static constexpr boost::hof::static_<boost::hof::partial_adaptor<void_class> > void_partial = {};

static constexpr boost::hof::static_<boost::hof::partial_adaptor<mono_class> > mono_partial = {};

static constexpr boost::hof::static_<boost::hof::partial_adaptor<move_class> > move_partial = {};

constexpr boost::hof::partial_adaptor<binary_class> binary_partial_constexpr = {};

constexpr boost::hof::partial_adaptor<unary_class> unary_partial_constexpr = {};

constexpr boost::hof::partial_adaptor<void_class> void_partial_constexpr = {};

constexpr boost::hof::partial_adaptor<mono_class> mono_partial_constexpr = {};

BOOST_HOF_TEST_CASE()
{
    boost::hof::partial_adaptor<void_class>()(1);

    void_partial(1);
    void_partial()(1);
    BOOST_HOF_TEST_CHECK(3 == binary_partial(1)(2));
    BOOST_HOF_TEST_CHECK(3 == binary_partial(1, 2));
    BOOST_HOF_TEST_CHECK(3 == unary_partial()(3));
    BOOST_HOF_TEST_CHECK(3 == unary_partial(3));
    BOOST_HOF_TEST_CHECK(3 == mono_partial(2));
    BOOST_HOF_TEST_CHECK(3 == mono_partial()(2));

    int i1 = 1;
    BOOST_HOF_TEST_CHECK(3 == binary_partial(2)(i1));
    BOOST_HOF_TEST_CHECK(3 == mutable_partial(std::ref(i1))(2));
    BOOST_HOF_TEST_CHECK(3 == i1);
    int i2 = 1;
    BOOST_HOF_TEST_CHECK(3 == mutable_partial(i2, 2));
    BOOST_HOF_TEST_CHECK(3 == i2);
    
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == (move_class()(1, 2)));
    BOOST_HOF_TEST_CHECK(3 == (move_partial(1, 2)));
    BOOST_HOF_TEST_CHECK(3 == (move_partial(1)(2)));
    BOOST_HOF_TEST_CHECK(3 == (boost::hof::partial(move_class())(1)(2)));
    BOOST_HOF_TEST_CHECK(3 == (boost::hof::partial(move_class())(1, 2)));
}

BOOST_HOF_TEST_CASE()
{
    void_partial_constexpr(1);
    void_partial_constexpr()(1);
    BOOST_HOF_STATIC_TEST_CHECK(3 == binary_partial_constexpr(1)(2));
    BOOST_HOF_STATIC_TEST_CHECK(3 == binary_partial_constexpr(1, 2));
    BOOST_HOF_STATIC_TEST_CHECK(3 == unary_partial_constexpr()(3));
    BOOST_HOF_STATIC_TEST_CHECK(3 == unary_partial_constexpr(3));
    BOOST_HOF_STATIC_TEST_CHECK(3 == mono_partial_constexpr(2));
    BOOST_HOF_STATIC_TEST_CHECK(3 == mono_partial_constexpr()(2));
}
#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(boost::hof::partial(binary_class{})(1)(2)), "noexcept partial");
}
#endif
BOOST_HOF_TEST_CASE()
{
    auto f = boost::hof::partial(boost::hof::limit_c<2>(binary_class()));
    static_assert(boost::hof::is_invocable<decltype(f), int>::value, "Passing the limit is not callable");
    static_assert(boost::hof::is_invocable<decltype(f), int, int>::value, "Passing the limit is not callable");
    static_assert(!boost::hof::is_invocable<decltype(f), int, int, int>::value, "Passing the limit is not callable");
    static_assert(!boost::hof::is_invocable<decltype(f), int, int, int, int>::value, "Passing the limit is not callable");

    auto g = f(0);
    static_assert(boost::hof::is_invocable<decltype(g), int>::value, "Passing the limit is not callable");
    static_assert(!boost::hof::is_invocable<decltype(g), int, int>::value, "Passing the limit is not callable");
    static_assert(!boost::hof::is_invocable<decltype(g), int, int, int>::value, "Passing the limit is not callable");
    static_assert(!boost::hof::is_invocable<decltype(g), int, int, int, int>::value, "Passing the limit is not callable");
}

BOOST_HOF_TEST_CASE()
{
    auto f = boost::hof::partial(binary_class());
    static_assert(boost::hof::is_invocable<decltype(f), int>::value, "Passing the limit is not callable");
    static_assert(boost::hof::is_invocable<decltype(f), int, int>::value, "Passing the limit is not callable");
    static_assert(boost::hof::is_invocable<decltype(f), int, int, int>::value, "Passing the limit is not callable");
    static_assert(boost::hof::is_invocable<decltype(f), int, int, int, int>::value, "Passing the limit is not callable");

    auto g = f(0);
    static_assert(boost::hof::is_invocable<decltype(g), int>::value, "Passing the limit is not callable");
    static_assert(boost::hof::is_invocable<decltype(g), int, int>::value, "Passing the limit is not callable");
    static_assert(boost::hof::is_invocable<decltype(g), int, int, int>::value, "Passing the limit is not callable");
    static_assert(boost::hof::is_invocable<decltype(g), int, int, int, int>::value, "Passing the limit is not callable");
}
