//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestStack
#include <boost/test/unit_test.hpp>

#include <boost/compute/container/stack.hpp>

#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(size)
{
    bc::stack<int> stack;
    BOOST_CHECK_EQUAL(stack.size(), size_t(0));

    stack.push(1);
    stack.push(2);
    stack.push(3);
    BOOST_CHECK_EQUAL(stack.size(), size_t(3));
}

BOOST_AUTO_TEST_CASE(push_and_pop)
{
    bc::stack<int> stack;
    stack.push(1);
    stack.push(2);
    stack.push(3);

    BOOST_CHECK_EQUAL(stack.top(), 3);
    BOOST_CHECK_EQUAL(stack.size(), size_t(3));
    stack.pop();
    BOOST_CHECK_EQUAL(stack.top(), 2);
    BOOST_CHECK_EQUAL(stack.size(), size_t(2));
    stack.pop();
    BOOST_CHECK_EQUAL(stack.top(), 1);
    BOOST_CHECK_EQUAL(stack.size(), size_t(1));
    stack.pop();
    BOOST_CHECK_EQUAL(stack.size(), size_t(0));
}

BOOST_AUTO_TEST_SUITE_END()
