/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    tuple_transform.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/proj.hpp>
#include <boost/hof/construct.hpp>
#include <boost/hof/unpack.hpp>
#include <boost/hof/function.hpp>
#include <boost/hof/placeholders.hpp>
#include <boost/hof/compose.hpp>
// Disable static checks for gcc 4.7
#ifndef BOOST_HOF_HAS_STATIC_TEST_CHECK
#if (defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ < 8)
#define BOOST_HOF_HAS_STATIC_TEST_CHECK 0
#endif
#endif
#include "test.hpp"

struct tuple_transform_f
{
    template<class Sequence, class F>
    constexpr auto operator()(Sequence&& s, F f) const BOOST_HOF_RETURNS
    (
        boost::hof::unpack(boost::hof::proj(f, boost::hof::construct<std::tuple>()))(boost::hof::forward<Sequence>(s))
    );
};

struct pack_transform_f
{
    template<class Sequence, class F>
    constexpr auto operator()(Sequence&& s, F f) const BOOST_HOF_RETURNS
    (
        boost::hof::unpack(boost::hof::proj(f, boost::hof::pack()))(boost::hof::forward<Sequence>(s))
    );
};

BOOST_HOF_STATIC_FUNCTION(tuple_transform) = tuple_transform_f{};
// BOOST_HOF_STATIC_FUNCTION(pack_transform) = pack_transform_f{};

#if !BOOST_HOF_HAS_CONSTEXPR_TUPLE
#define TUPLE_TRANSFORM_STATIC_CHECK(...)
#else
#define TUPLE_TRANSFORM_STATIC_CHECK BOOST_HOF_STATIC_TEST_CHECK

#endif

BOOST_HOF_TEST_CASE()
{
    auto t = std::make_tuple(1, 2);
    auto r = tuple_transform(t, [](int i) { return i*i; });
    BOOST_HOF_TEST_CHECK(r == std::make_tuple(1, 4));
}

BOOST_HOF_TEST_CASE()
{
    TUPLE_TRANSFORM_STATIC_CHECK(tuple_transform(std::make_tuple(1, 2), boost::hof::_1 * boost::hof::_1) == std::make_tuple(1, 4));
}

#define TUPLE_TRANSFORM_CHECK_ID(x) \
BOOST_HOF_TEST_CHECK(tuple_transform(x, boost::hof::identity) == x); \
TUPLE_TRANSFORM_STATIC_CHECK(tuple_transform(x, boost::hof::identity) == x);

BOOST_HOF_TEST_CASE()
{
    TUPLE_TRANSFORM_CHECK_ID(std::make_tuple(1, 2));
    TUPLE_TRANSFORM_CHECK_ID(std::make_tuple(1));
    TUPLE_TRANSFORM_CHECK_ID(std::make_tuple());
}

BOOST_HOF_TEST_CASE()
{
    auto x = tuple_transform(std::make_tuple(std::unique_ptr<int>(new int(3))), boost::hof::identity);
    auto y = std::make_tuple(std::unique_ptr<int>(new int(3)));
    BOOST_HOF_TEST_CHECK(x != y);
    BOOST_HOF_TEST_CHECK(tuple_transform(x, *boost::hof::_1) == tuple_transform(y, *boost::hof::_1));
}

#define TUPLE_TRANSFORM_CHECK_COMPOSE(x, f, g) \
BOOST_HOF_TEST_CHECK(tuple_transform(x, boost::hof::compose(f, g)) == tuple_transform(tuple_transform(x, g), f)); \
TUPLE_TRANSFORM_STATIC_CHECK(tuple_transform(x, boost::hof::compose(f, g)) == tuple_transform(tuple_transform(x, g), f));

BOOST_HOF_TEST_CASE()
{
    TUPLE_TRANSFORM_CHECK_COMPOSE(std::make_tuple(1, 2, 3, 4), boost::hof::_1 * boost::hof::_1, boost::hof::_1 + boost::hof::_1);
    TUPLE_TRANSFORM_CHECK_COMPOSE(std::make_tuple(1, 2, 3), boost::hof::_1 * boost::hof::_1, boost::hof::_1 + boost::hof::_1);
    TUPLE_TRANSFORM_CHECK_COMPOSE(std::make_tuple(1), boost::hof::_1 * boost::hof::_1, boost::hof::_1 + boost::hof::_1);
    TUPLE_TRANSFORM_CHECK_COMPOSE(std::make_tuple(), boost::hof::_1 * boost::hof::_1, boost::hof::_1 + boost::hof::_1);
}
