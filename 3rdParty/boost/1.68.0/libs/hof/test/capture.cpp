/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    capture.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/capture.hpp>
#include <boost/hof/identity.hpp>
#include "test.hpp"

BOOST_HOF_TEST_CASE()
{    
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::capture_basic(1, 2)(binary_class())() == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::capture_basic(1, 2)(binary_class())() == 3);

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::capture_basic(1)(binary_class())(2) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::capture_basic(1)(binary_class())(2) == 3);

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::capture_basic()(binary_class())(1, 2) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::capture_basic()(binary_class())(1, 2) == 3);

    static const int one = 1;
    static const int two = 2;
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::capture_forward(one, two)(binary_class())() == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::capture_forward(one, two)(binary_class())() == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::capture_forward(1, 2)(binary_class())() == 3);

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::capture_forward(one)(binary_class())(two) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::capture_forward(1)(binary_class())(2) == 3);

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::capture_forward()(binary_class())(one, two) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::capture_forward()(binary_class())(one, two) == 3);

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::capture(1, 2)(binary_class())() == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::capture(1, 2)(binary_class())() == 3);

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::capture(1)(binary_class())(2) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::capture(1)(binary_class())(2) == 3);

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::capture()(binary_class())(1, 2) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::capture()(binary_class())(1, 2) == 3);
}

struct add_member
{
    int i;

    add_member(int ip) : i(ip)
    {}

    int add(int j) const
    {
        return i + j;
    }
};

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::capture_basic(add_member(1), 2)(&add_member::add)() == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::capture_basic(add_member(1))(&add_member::add)(2) == 3);
}

BOOST_HOF_TEST_CASE()
{
    auto id = boost::hof::identity;
    auto f = boost::hof::capture(boost::hof::identity)(boost::hof::identity);
    static_assert(BOOST_HOF_IS_DEFAULT_CONSTRUCTIBLE(decltype(id)), "Id not default constructible");
    static_assert(BOOST_HOF_IS_DEFAULT_CONSTRUCTIBLE(decltype(f)), "Not default constructible");
    f();
}
#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(boost::hof::capture(boost::hof::identity)(boost::hof::identity)()), "noexcept capture");
    static_assert(noexcept(boost::hof::capture_basic(boost::hof::identity)(boost::hof::identity)()), "noexcept capture");
    static_assert(noexcept(boost::hof::capture_forward(boost::hof::identity)(boost::hof::identity)()), "noexcept capture");
}
#endif
BOOST_HOF_TEST_CASE()
{
    auto f = boost::hof::capture_basic(boost::hof::identity)(boost::hof::identity);
    f();
}

BOOST_HOF_TEST_CASE()
{
    auto f = boost::hof::capture_forward(boost::hof::identity)(boost::hof::identity);
    f();
}

BOOST_HOF_TEST_CASE()
{
    auto f = boost::hof::capture(boost::hof::identity)(add_member{1});
    static_assert(!boost::hof::is_invocable<decltype(f), int>::value, "Not sfinae friendly");
}

