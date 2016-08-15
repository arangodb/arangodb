//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestSortByKey
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/sort_by_key.hpp>
#include <boost/compute/algorithm/is_sorted.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

// test trivial sorting of zero element vectors
BOOST_AUTO_TEST_CASE(sort_int_0)
{
    compute::vector<int> keys(context);
    compute::vector<int> values(context);
    BOOST_CHECK_EQUAL(keys.size(), size_t(0));
    BOOST_CHECK_EQUAL(values.size(), size_t(0));
    BOOST_CHECK(compute::is_sorted(keys.begin(), keys.end()) == true);
    BOOST_CHECK(compute::is_sorted(values.begin(), values.end()) == true);
    compute::sort_by_key(keys.begin(), keys.end(), values.begin(), queue);
}

// test trivial sorting of one element vectors
BOOST_AUTO_TEST_CASE(sort_int_1)
{
    int keys_data[] = { 11 };
    int values_data[] = { 100 };

    compute::vector<int> keys(keys_data, keys_data + 1, queue);
    compute::vector<int> values(values_data, values_data + 1, queue);

    BOOST_CHECK(compute::is_sorted(keys.begin(), keys.end(), queue) == true);
    BOOST_CHECK(compute::is_sorted(values.begin(), values.end(), queue) == true);

    compute::sort_by_key(keys.begin(), keys.end(), values.begin(), queue);
}

// test trivial sorting of two element vectors
BOOST_AUTO_TEST_CASE(sort_int_2)
{
    int keys_data[] = { 4, 2 };
    int values_data[] = { 42, 24 };

    compute::vector<int> keys(keys_data, keys_data + 2, queue);
    compute::vector<int> values(values_data, values_data + 2, queue);

    BOOST_CHECK(compute::is_sorted(keys.begin(), keys.end(), queue) == false);
    BOOST_CHECK(compute::is_sorted(values.begin(), values.end(), queue) == false);

    compute::sort_by_key(keys.begin(), keys.end(), values.begin(), queue);

    BOOST_CHECK(compute::is_sorted(keys.begin(), keys.end(), queue) == true);
    BOOST_CHECK(compute::is_sorted(values.begin(), values.end(), queue) == true);
}

BOOST_AUTO_TEST_CASE(sort_char_by_int)
{
    int keys_data[] = { 6, 2, 1, 3, 4, 7, 5, 0 };
    char values_data[] = { 'g', 'c', 'b', 'd', 'e', 'h', 'f', 'a' };

    compute::vector<int> keys(keys_data, keys_data + 8, queue);
    compute::vector<char> values(values_data, values_data + 8, queue);

    compute::sort_by_key(keys.begin(), keys.end(), values.begin(), queue);

    CHECK_RANGE_EQUAL(int, 8, keys, (0, 1, 2, 3, 4, 5, 6, 7));
    CHECK_RANGE_EQUAL(char, 8, values, ('a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'));
}

BOOST_AUTO_TEST_CASE(sort_int_and_float)
{
    int n = 1024;
    std::vector<int> host_keys(n);
    std::vector<float> host_values(n);
    for(int i = 0; i < n; i++){
        host_keys[i] = n - i;
        host_values[i] = (n - i) / 2.f;
    }

    compute::vector<int> keys(host_keys.begin(), host_keys.end(), queue);
    compute::vector<float> values(host_values.begin(), host_values.end(), queue);

    BOOST_CHECK(compute::is_sorted(keys.begin(), keys.end(), queue) == false);
    BOOST_CHECK(compute::is_sorted(values.begin(), values.end(), queue) == false);

    compute::sort_by_key(keys.begin(), keys.end(), values.begin(), queue);

    BOOST_CHECK(compute::is_sorted(keys.begin(), keys.end(), queue) == true);
    BOOST_CHECK(compute::is_sorted(values.begin(), values.end(), queue) == true);
}

BOOST_AUTO_TEST_SUITE_END()
