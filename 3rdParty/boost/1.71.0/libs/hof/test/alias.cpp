/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    alias.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/alias.hpp>
#include "test.hpp"

struct foo
{
    int i;
    foo(int i_) : i(i_)
    {}
};

BOOST_HOF_TEST_CASE()
{
    boost::hof::alias<int> ai = 5;
    BOOST_HOF_TEST_CHECK(boost::hof::alias_value(ai) == 5);
    boost::hof::alias_inherit<foo> af = foo{5};
    BOOST_HOF_TEST_CHECK(boost::hof::alias_value(af).i == 5);
}

