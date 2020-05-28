/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    identity.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/identity.hpp>
#include <boost/hof/is_invocable.hpp>
#include <boost/hof/detail/move.hpp>
#include "test.hpp"

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::identity(10) == 10);
    BOOST_HOF_TEST_CHECK(boost::hof::identity(10) == 10);
}

BOOST_HOF_TEST_CASE()
{
    int i = 3;
    BOOST_HOF_TEST_CHECK(boost::hof::identity(i) == 3);
    BOOST_HOF_TEST_CHECK(&boost::hof::identity(i) == &i);
    static_assert(std::is_lvalue_reference<decltype(boost::hof::identity(i))>::value, "Not lvalue");
    static_assert(!std::is_lvalue_reference<decltype(boost::hof::identity(3))>::value, "Not rvalue");
}

BOOST_HOF_TEST_CASE()
{
    auto ls = boost::hof::identity({1, 2, 3, 4});
    std::vector<int> v{1, 2, 3, 4};
    BOOST_HOF_TEST_CHECK(std::equal(ls.begin(), ls.end(), v.begin()));
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(std::vector<int>(boost::hof::identity({1, 2, 3})) == std::vector<int>{1, 2, 3});
}

BOOST_HOF_TEST_CASE()
{
    static_assert(boost::hof::is_invocable<decltype(boost::hof::identity), int>::value, "Identiy callable");
    static_assert(!boost::hof::is_invocable<decltype(boost::hof::identity), int, int>::value, "Identiy not callable");
    static_assert(!boost::hof::is_invocable<decltype(boost::hof::identity)>::value, "Identiy not callable");
}

BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(boost::hof::identity({1, 2, 3})), "Noexcept identity");
    static_assert(noexcept(boost::hof::identity(1)), "Noexcept identity");
    int i = 5;
    static_assert(noexcept(boost::hof::identity(i)), "Noexcept identity");
}

struct copy_throws 
{
    copy_throws() {}
    copy_throws(copy_throws const&) {}
    copy_throws(copy_throws&&) noexcept {}
};

BOOST_HOF_TEST_CASE()
{
    copy_throws ct{};
    static_assert(noexcept(boost::hof::identity(ct)), "Noexcept identity");
    static_assert(noexcept(boost::hof::identity(boost::hof::move(ct))), "Noexcept identity");
    static_assert(!noexcept(boost::hof::identity(copy_throws{})), "Noexcept identity");
}

