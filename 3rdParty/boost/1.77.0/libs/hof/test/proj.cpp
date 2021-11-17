/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    proj.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/proj.hpp>
#include <boost/hof/placeholders.hpp>
#include <boost/hof/mutable.hpp>
#include "test.hpp"

#include <memory>

struct foo
{
    constexpr foo(int xp) : x(xp)
    {}
    int x;
};

struct select_x
{
    template<class T>
    constexpr auto operator()(T&& x) const BOOST_HOF_RETURNS(x.x);
};

BOOST_HOF_TEST_CASE()
{
#ifndef _MSC_VER
    constexpr 
#endif
    auto add = boost::hof::_ + boost::hof::_;
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::proj(select_x(), add)(foo(1), foo(2)) == 3);
    // Using mutable_ as a workaround on libc++, since mem_fn does not meet the
    // requirements of a FunctionObject
    BOOST_HOF_TEST_CHECK(boost::hof::proj(boost::hof::mutable_(std::mem_fn(&foo::x)), add)(foo(1), foo(2)) == 3);
    static_assert(boost::hof::detail::is_default_constructible<decltype(boost::hof::proj(select_x(), add))>::value, "Not default constructible");
}

BOOST_HOF_TEST_CASE()
{
#ifndef _MSC_VER
    constexpr 
#endif
    auto add = boost::hof::_ + boost::hof::_;
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::proj(select_x(), add)(foo(1), foo(2)) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::proj(&foo::x, add)(foo(1), foo(2)) == 3);
    static_assert(boost::hof::detail::is_default_constructible<decltype(boost::hof::proj(select_x(), add))>::value, "Not default constructible");
}

BOOST_HOF_TEST_CASE()
{
    auto indirect_add = boost::hof::proj(*boost::hof::_, boost::hof::_ + boost::hof::_);
    BOOST_HOF_TEST_CHECK(indirect_add(std::unique_ptr<int>(new int(1)), std::unique_ptr<int>(new int(2))) == 3);
    static_assert(boost::hof::detail::is_default_constructible<decltype(indirect_add)>::value, "Not default constructible");
}

struct select_x_1
{
    std::unique_ptr<int> i;
    select_x_1() : i(new int(2))
    {}
    template<class T>
    auto operator()(T&& x) const BOOST_HOF_RETURNS(x.x * (*i));
};

struct sum_1
{
    std::unique_ptr<int> i;
    sum_1() : i(new int(1))
    {}
    template<class T, class U>
    auto operator()(T&& x, U&& y) const BOOST_HOF_RETURNS(x + y + *i);
};

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::proj(select_x_1(), sum_1())(foo(1), foo(2)) == 7);
}

BOOST_HOF_TEST_CASE()
{
    std::string s;
    auto f = [&](const std::string& x)
    {
        s += x;
    };
    boost::hof::proj(f)("hello", "-", "world");
    BOOST_HOF_TEST_CHECK(s == "hello-world");
    boost::hof::proj(f)();
    BOOST_HOF_TEST_CHECK(s == "hello-world");
}

BOOST_HOF_TEST_CASE()
{
    std::string s;
    auto f = [&](const std::string& x)
    {
        s += x;
        return s;
    };
    auto last = [](const std::string& x, const std::string& y, const std::string& z)
    {
        BOOST_HOF_TEST_CHECK(x == "hello");
        BOOST_HOF_TEST_CHECK(y == "hello-");
        BOOST_HOF_TEST_CHECK(z == "hello-world");
        return z;
    };
    BOOST_HOF_TEST_CHECK(boost::hof::proj(f, last)("hello", "-", "world") == "hello-world");
}

template<bool B>
struct bool_
{
    static const bool value = B;
};

struct constexpr_check
{
    template<class T>
    constexpr int operator()(T) const
    {
        static_assert(T::value, "Failed");
        return 0;
    }
};

struct constexpr_check_each
{
    template<class T>
    constexpr bool operator()(T x) const
    {
        return boost::hof::proj(constexpr_check())(x, x), true;
    }
};

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_STATIC_TEST_CHECK(constexpr_check_each()(bool_<true>()));
}

BOOST_HOF_TEST_CASE()
{
    boost::hof::proj(boost::hof::identity, boost::hof::identity)(0);
}

struct bar {};

BOOST_HOF_TEST_CASE()
{
    auto f = boost::hof::proj(bar{});
    static_assert(!boost::hof::is_invocable<decltype(f), int>::value, "Not sfinae friendly");
    static_assert(!boost::hof::is_invocable<decltype(f), int, int>::value, "Not sfinae friendly");
    static_assert(!boost::hof::is_invocable<decltype(f), int, int, int>::value, "Not sfinae friendly");
}

BOOST_HOF_TEST_CASE()
{
    auto f = boost::hof::proj(bar{}, bar{});
    static_assert(!boost::hof::is_invocable<decltype(f), int>::value, "Not sfinae friendly");
    static_assert(!boost::hof::is_invocable<decltype(f), int, int>::value, "Not sfinae friendly");
    static_assert(!boost::hof::is_invocable<decltype(f), int, int, int>::value, "Not sfinae friendly");
    static_assert(!boost::hof::is_invocable<decltype(f)>::value, "Not sfinae friendly");
}
