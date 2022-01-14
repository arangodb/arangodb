/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    arg.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/arg.hpp>
#include <boost/hof/is_invocable.hpp>
#include <type_traits>
#include "test.hpp"

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::arg_c<3>(1,2,3,4,5) == 3);
    BOOST_HOF_TEST_CHECK( boost::hof::arg_c<3>(1,2,3,4,5) == 3 );
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::arg(std::integral_constant<int, 3>())(1,2,3,4,5) == 3);
    BOOST_HOF_TEST_CHECK( boost::hof::arg(std::integral_constant<int, 3>())(1,2,3,4,5) == 3 );
}

BOOST_HOF_TEST_CASE()
{
    auto at_3 = boost::hof::arg(std::integral_constant<int, 3>());
    static_assert(boost::hof::is_invocable<decltype(at_3), int, int, int>::value, "Not SFINAE-friendly");
    static_assert(!boost::hof::is_invocable<decltype(at_3), int, int>::value, "Not SFINAE-friendly");
    static_assert(!boost::hof::is_invocable<decltype(at_3), int>::value, "Not SFINAE-friendly");
}

struct foo {};

BOOST_HOF_TEST_CASE()
{
    static_assert(!boost::hof::is_invocable<decltype(boost::hof::arg), int>::value, "Not sfinae friendly");
    static_assert(!boost::hof::is_invocable<decltype(boost::hof::arg), foo>::value, "Not sfinae friendly");
}

struct copy_throws 
{
    copy_throws() {}
    copy_throws(copy_throws const&) {}
    copy_throws(copy_throws&&) noexcept {}
};
#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(boost::hof::arg_c<3>(1,2,3,4,5)), "noexcept arg");
    static_assert(noexcept(boost::hof::arg(std::integral_constant<int, 3>())(1,2,3,4,5)), "noexcept arg");
    static_assert(!noexcept(boost::hof::arg(std::integral_constant<int, 3>())(1,2,copy_throws{},4,5)), "noexcept arg");
}
#endif
