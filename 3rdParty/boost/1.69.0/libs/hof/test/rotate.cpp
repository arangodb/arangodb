/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    rotate.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/rotate.hpp>
#include <boost/hof/placeholders.hpp>
#include <boost/hof/compose.hpp>
#include <boost/hof/repeat.hpp>
#include "test.hpp"

struct head
{
    template<class T, class... Ts>
    constexpr T operator()(T x, Ts&&...) const
    BOOST_HOF_RETURNS_DEDUCE_NOEXCEPT(x)
    {
        return x;
    }
};

#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(boost::hof::rotate(head{})(1, 2, 3, 4)), "noexcept rotate");
    static_assert(noexcept(boost::hof::repeat(std::integral_constant<int, 5>{})(boost::hof::rotate)(head{})(1, 2, 3, 4, 5, 6, 7, 8, 9)), "noexcept rotate");
}
#endif

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(2 == boost::hof::rotate(head{})(1, 2, 3, 4));
    BOOST_HOF_STATIC_TEST_CHECK(2 == boost::hof::rotate(head{})(1, 2, 3, 4));
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == boost::hof::compose(boost::hof::rotate, boost::hof::rotate)(head{})(1, 2, 3, 4));
    BOOST_HOF_STATIC_TEST_CHECK(3 == boost::hof::compose(boost::hof::rotate, boost::hof::rotate)(head{})(1, 2, 3, 4));
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(6 == boost::hof::repeat(std::integral_constant<int, 5>{})(boost::hof::rotate)(head{})(1, 2, 3, 4, 5, 6, 7, 8, 9));
    BOOST_HOF_STATIC_TEST_CHECK(6 == boost::hof::repeat(std::integral_constant<int, 5>{})(boost::hof::rotate)(head{})(1, 2, 3, 4, 5, 6, 7, 8, 9));
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == boost::hof::rotate(boost::hof::_ - boost::hof::_)(2, 5));
    BOOST_HOF_STATIC_TEST_CHECK(3 == boost::hof::rotate(boost::hof::_ - boost::hof::_)(2, 5));
}

#if defined(__GNUC__) && !defined (__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ < 7
#define FINAL
#else
#define FINAL final
#endif


struct f FINAL {
    int operator()(int i, void *) const {
        return i;
    }
};

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::rotate(f())(nullptr, 2) == 2);
}
