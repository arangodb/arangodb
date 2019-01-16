/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    result.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/result.hpp>
#include <boost/hof/static.hpp>
#include "test.hpp"


static constexpr boost::hof::result_adaptor<int, unary_class> unary_int = {};

BOOST_HOF_TEST_CASE()
{
    STATIC_ASSERT_SAME(decltype(unary_int(false)), int);
    BOOST_HOF_TEST_CHECK(unary_int(false) == 0);
    BOOST_HOF_STATIC_TEST_CHECK(unary_int(false) == 0);
}

static constexpr boost::hof::result_adaptor<void, unary_class> unary_void = {};

BOOST_HOF_TEST_CASE()
{
    STATIC_ASSERT_SAME(decltype(unary_void(false)), void);
}
