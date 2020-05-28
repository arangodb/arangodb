/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    static.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/static.hpp>
#include "test.hpp"

// TODO: Test infix

static constexpr boost::hof::static_<binary_class> binary_static = {};

static constexpr boost::hof::static_<void_class> void_static = {};

static constexpr boost::hof::static_<mono_class> mono_static = {};


BOOST_HOF_TEST_CASE()
{
    void_static(1);
    BOOST_HOF_TEST_CHECK(3 == binary_static(1, 2));
    BOOST_HOF_TEST_CHECK(3 == mono_static(2));
}
