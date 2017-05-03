//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestAdjacentFind
#include <boost/test/unit_test.hpp>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/adjacent_find.hpp>
#include <boost/compute/algorithm/fill.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/container/vector.hpp>

#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(adjacent_find_int)
{
    int data[] = { 1, 3, 5, 5, 6, 7, 7, 8 };
    compute::vector<int> vec(data, data + 8, queue);

    compute::vector<int>::iterator iter =
        compute::adjacent_find(vec.begin(), vec.end(), queue);
    BOOST_CHECK(iter == vec.begin() + 2);
}

BOOST_AUTO_TEST_CASE(adjacent_find_int2)
{
    using compute::int2_;

    compute::vector<int2_> vec(context);
    vec.push_back(int2_(1, 2), queue);
    vec.push_back(int2_(3, 4), queue);
    vec.push_back(int2_(5, 6), queue);
    vec.push_back(int2_(7, 8), queue);
    vec.push_back(int2_(7, 8), queue);

    compute::vector<int2_>::iterator iter =
        compute::adjacent_find(vec.begin(), vec.end(), queue);
    BOOST_CHECK(iter == vec.begin() + 3);
}

BOOST_AUTO_TEST_CASE(adjacent_find_iota)
{
    compute::vector<int> vec(2048, context);
    compute::iota(vec.begin(), vec.end(), 1, queue);
    BOOST_VERIFY(
        compute::adjacent_find(vec.begin(), vec.end(), queue) == vec.end()
    );
}

BOOST_AUTO_TEST_CASE(adjacent_find_fill)
{
    compute::vector<int> vec(2048, context);
    compute::fill(vec.begin(), vec.end(), 7, queue);
    BOOST_VERIFY(
        compute::adjacent_find(vec.begin(), vec.end(), queue) == vec.begin()
    );
}

BOOST_AUTO_TEST_SUITE_END()
