/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    flip.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/flip.hpp>
#include <boost/hof/placeholders.hpp>
#include "test.hpp"

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == boost::hof::flip(boost::hof::_ - boost::hof::_)(2, 5));
    BOOST_HOF_STATIC_TEST_CHECK(3 == boost::hof::flip(boost::hof::_ - boost::hof::_)(2, 5));
}

BOOST_HOF_TEST_CASE()
{
    typedef std::integral_constant<int, 1> one;
    typedef std::integral_constant<int, 2> two;
    typedef std::integral_constant<int, 3> three;
    BOOST_HOF_TEST_CHECK(1 == boost::hof::arg(one{})(1, 2, 3, 4));
    BOOST_HOF_STATIC_TEST_CHECK(1 == boost::hof::arg(one{})(1, 2, 3, 4));
    BOOST_HOF_TEST_CHECK(2 == boost::hof::flip(boost::hof::arg(one{}))(1, 2, 3, 4));
    BOOST_HOF_STATIC_TEST_CHECK(2 == boost::hof::flip(boost::hof::arg(one{}))(1, 2, 3, 4));

    BOOST_HOF_TEST_CHECK(2 == boost::hof::arg(two{})(1, 2, 3, 4));
    BOOST_HOF_STATIC_TEST_CHECK(2 == boost::hof::arg(two{})(1, 2, 3, 4));
    BOOST_HOF_TEST_CHECK(1 == boost::hof::flip(boost::hof::arg(two{}))(1, 2, 3, 4));
    BOOST_HOF_STATIC_TEST_CHECK(1 == boost::hof::flip(boost::hof::arg(two{}))(1, 2, 3, 4));

    BOOST_HOF_TEST_CHECK(3 == boost::hof::arg(three{})(1, 2, 3, 4));
    BOOST_HOF_STATIC_TEST_CHECK(3 == boost::hof::arg(three{})(1, 2, 3, 4));
    BOOST_HOF_TEST_CHECK(3 == boost::hof::flip(boost::hof::arg(three{}))(1, 2, 3, 4));
    BOOST_HOF_STATIC_TEST_CHECK(3 == boost::hof::flip(boost::hof::arg(three{}))(1, 2, 3, 4));
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

#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
BOOST_HOF_TEST_CASE()
{
    static_assert(boost::hof::flip(boost::hof::_ - boost::hof::_)(2, 5), "noexcept flip");
}
#endif

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::flip(f())(nullptr, 2) == 2);
}
