//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestIsSorted
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/algorithm/is_sorted.hpp>
#include <boost/compute/algorithm/sort.hpp>
#include <boost/compute/algorithm/reverse.hpp>
#include <boost/compute/container/vector.hpp>

#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(is_sorted_int)
{
    compute::vector<int> vec(context);
    BOOST_CHECK(compute::is_sorted(vec.begin(), vec.end(), queue));

    vec.push_back(1, queue);
    BOOST_CHECK(compute::is_sorted(vec.begin(), vec.end(), queue));

    vec.push_back(2, queue);
    BOOST_CHECK(compute::is_sorted(vec.begin(), vec.end(), queue));

    vec.push_back(0, queue);
    BOOST_CHECK(compute::is_sorted(vec.begin(), vec.end(), queue) == false);

    vec.push_back(-2, queue);
    BOOST_CHECK(compute::is_sorted(vec.begin(), vec.end(), queue) == false);

    compute::sort(vec.begin(), vec.end(), queue);
    BOOST_CHECK(compute::is_sorted(vec.begin(), vec.end(), queue));
}

BOOST_AUTO_TEST_CASE(is_sorted_ones)
{
    compute::vector<int> vec(2048, context);
    compute::fill(vec.begin(), vec.end(), 1, queue);
    BOOST_CHECK(compute::is_sorted(vec.begin(), vec.end(), queue));
}

BOOST_AUTO_TEST_CASE(is_sorted_iota)
{
    // create vector with values from 1..1000
    compute::vector<int> vec(1000, context);
    compute::iota(vec.begin(), vec.end(), 1, queue);
    BOOST_CHECK(compute::is_sorted(vec.begin(), vec.end(), queue));

    // reverse the range
    compute::reverse(vec.begin(), vec.end(), queue);
    BOOST_CHECK(compute::is_sorted(vec.begin(), vec.end(), queue) == false);
    BOOST_CHECK(compute::is_sorted(vec.begin(), vec.end(), compute::greater<int>(), queue));

    // reverse it back
    compute::reverse(vec.begin(), vec.end(), queue);
    BOOST_CHECK(compute::is_sorted(vec.begin(), vec.end(), queue));
    BOOST_CHECK(compute::is_sorted(vec.begin(), vec.end(), compute::greater<int>(), queue) == false);
}

BOOST_AUTO_TEST_SUITE_END()
