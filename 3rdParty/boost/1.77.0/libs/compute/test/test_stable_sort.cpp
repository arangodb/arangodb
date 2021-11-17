//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestStableSort
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/function.hpp>
#include <boost/compute/algorithm/stable_sort.hpp>
#include <boost/compute/algorithm/is_sorted.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(sort_int_vector)
{
    int data[] = { -4, 152, -5000, 963, 75321, -456, 0, 1112 };
    compute::vector<int> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));
    BOOST_CHECK(compute::is_sorted(vector.begin(), vector.end(), queue) == false);

    compute::stable_sort(vector.begin(), vector.end(), queue);
    BOOST_CHECK(compute::is_sorted(vector.begin(), vector.end(), queue) == true);
    CHECK_RANGE_EQUAL(int, 8, vector, (-5000, -456, -4, 0, 152, 963, 1112, 75321));

    // sort reversed
    compute::stable_sort(vector.begin(), vector.end(), compute::greater<int>(), queue);
    CHECK_RANGE_EQUAL(int, 8, vector, (75321, 1112, 963, 152, 0, -4, -456, -5000));
}

BOOST_AUTO_TEST_CASE(sort_int2)
{
    using compute::int2_;

    // device vector of int2's
    compute::vector<int2_> vec(context);
    vec.push_back(int2_(2, 1), queue);
    vec.push_back(int2_(2, 2), queue);
    vec.push_back(int2_(1, 2), queue);
    vec.push_back(int2_(1, 1), queue);

    // function comparing the first component of each int2
    BOOST_COMPUTE_FUNCTION(bool, compare_first, (int2_ a, int2_ b),
    {
        return a.x < b.x;
    });

    // ensure vector is not sorted
    BOOST_CHECK(compute::is_sorted(vec.begin(), vec.end(), compare_first, queue) == false);

    // sort elements based on their first component
    compute::stable_sort(vec.begin(), vec.end(), compare_first, queue);

    // ensure vector is now sorted
    BOOST_CHECK(compute::is_sorted(vec.begin(), vec.end(), compare_first, queue) == true);

    // check sorted vector order
    std::vector<int2_> result(vec.size());
    compute::copy(vec.begin(), vec.end(), result.begin(), queue);
    BOOST_CHECK_EQUAL(result[0], int2_(1, 2));
    BOOST_CHECK_EQUAL(result[1], int2_(1, 1));
    BOOST_CHECK_EQUAL(result[2], int2_(2, 1));
    BOOST_CHECK_EQUAL(result[3], int2_(2, 2));

    // function comparing the second component of each int2
    BOOST_COMPUTE_FUNCTION(bool, compare_second, (int2_ a, int2_ b),
    {
        return a.y < b.y;
    });

    // sort elements based on their second component
    compute::stable_sort(vec.begin(), vec.end(), compare_second, queue);

    // check sorted vector order
    compute::copy(vec.begin(), vec.end(), result.begin(), queue);
    BOOST_CHECK_EQUAL(result[0], int2_(1, 1));
    BOOST_CHECK_EQUAL(result[1], int2_(2, 1));
    BOOST_CHECK_EQUAL(result[2], int2_(1, 2));
    BOOST_CHECK_EQUAL(result[3], int2_(2, 2));
}

BOOST_AUTO_TEST_SUITE_END()
