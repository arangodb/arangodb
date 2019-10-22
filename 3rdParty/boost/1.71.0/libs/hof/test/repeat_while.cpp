/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    repeat_while.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/repeat_while.hpp>
#include <boost/hof/reveal.hpp>
#include "test.hpp"

// TODO: Test default construction, and static initialization

struct increment_constant
{
    template<class T>
    constexpr std::integral_constant<int, T::value + 1> operator()(T) const noexcept
    {
        return std::integral_constant<int, T::value + 1>();
    }
};

struct increment
{
    template<class T>
    constexpr T operator()(T x) const noexcept
    {
        return x + 1;
    }
};

struct not_6_constant
{
    template<class T>
    constexpr std::integral_constant<bool, (T::value != 6)> 
    operator()(T) const noexcept
    {
        return std::integral_constant<bool, (T::value != 6)>();
    }
};

struct not_6
{
    template<class T>
    constexpr bool operator()(T x) const noexcept
    {
        return x != 6;
    }
};

struct not_limit
{
    template<class T>
    constexpr bool operator()(T x) const
    {
        return x != (BOOST_HOF_RECURSIVE_CONSTEXPR_DEPTH+4);
    }
};

#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(boost::hof::repeat_while(not_6())(increment())(1)), "noexcept repeat_while");
    static_assert(noexcept(boost::hof::repeat_while(not_6_constant())(increment_constant())(std::integral_constant<int, 1>())), "noexcept repeat_while");
}
#endif

BOOST_HOF_TEST_CASE()
{
    static_assert
    (
        std::is_same<
            std::integral_constant<int, 6>, 
            decltype(boost::hof::repeat_while(not_6_constant())(increment_constant())(std::integral_constant<int, 1>()))
        >::value,
        "Error"
    );

    std::integral_constant<int, 6> x = boost::hof::repeat_while(not_6_constant())(increment_constant())(std::integral_constant<int, 1>());
    boost::hof::test::unused(x);
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::repeat_while(not_6())(increment())(1) == 6);
    BOOST_HOF_TEST_CHECK(boost::hof::repeat_while(not_6())(increment())(1) == 6);
    BOOST_HOF_TEST_CHECK(boost::hof::reveal(boost::hof::repeat_while(not_6())(increment()))(1) == 6);
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::repeat_while(not_limit())(increment())(1) == BOOST_HOF_RECURSIVE_CONSTEXPR_DEPTH+4);
#if BOOST_HOF_HAS_RELAXED_CONSTEXPR
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::repeat_while(not_limit())(increment())(1) == BOOST_HOF_RECURSIVE_CONSTEXPR_DEPTH+4);
#endif
}
