/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    indirect.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/indirect.hpp>
#include "test.hpp"

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == boost::hof::indirect(std::unique_ptr<binary_class>(new binary_class()))(1, 2));
    BOOST_HOF_TEST_CHECK(3 == boost::hof::reveal(boost::hof::indirect(std::unique_ptr<binary_class>(new binary_class())))(1, 2));

    binary_class f;

    BOOST_HOF_TEST_CHECK(3 == boost::hof::indirect(&f)(1, 2));
    BOOST_HOF_TEST_CHECK(3 == boost::hof::reveal(boost::hof::indirect(&f))(1, 2));
}
#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
BOOST_HOF_TEST_CASE()
{
    binary_class f;
    static_assert(noexcept(boost::hof::indirect(&f)(1, 2)), "noexcept indirect");
}
#endif

struct mutable_function
{
    mutable_function() : value(0) {}
    void operator()(int a) { value += a; }
    int value;
};

BOOST_HOF_TEST_CASE()
{
    auto mf = mutable_function{};
    boost::hof::indirect(&mf)(15);
    boost::hof::indirect(&mf)(2);
    BOOST_HOF_TEST_CHECK(mf.value == 17);
}


BOOST_HOF_TEST_CASE()
{
    auto mf = std::make_shared<mutable_function>();
    boost::hof::indirect(mf)(15);
    boost::hof::indirect(mf)(2);
    BOOST_HOF_TEST_CHECK(mf->value == 17);
}


