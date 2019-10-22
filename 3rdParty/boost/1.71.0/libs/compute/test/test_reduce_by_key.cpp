//---------------------------------------------------------------------------//
// Copyright (c) 2015 Jakub Szuppe <j.szuppe@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestReduceByKey
#include <boost/test/unit_test.hpp>

#include <boost/compute/lambda.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/functional.hpp>
#include <boost/compute/algorithm/inclusive_scan.hpp>
#include <boost/compute/algorithm/reduce_by_key.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(reduce_by_key_int)
{
//! [reduce_by_key_int]
// setup keys and values
int keys[] = { 0, 2, -3, -3, -3, -3, -3, 4 };
int data[] = { 1, 1, 1, 1, 1, 2, 5, 1 };

boost::compute::vector<int> keys_input(keys, keys + 8, queue);
boost::compute::vector<int> values_input(data, data + 8, queue);

boost::compute::vector<int> keys_output(8, context);
boost::compute::vector<int> values_output(8, context);
// reduce by key
boost::compute::reduce_by_key(keys_input.begin(), keys_input.end(), values_input.begin(),
                              keys_output.begin(), values_output.begin(), queue);
// keys_output = { 0, 2, -3, 4 }
// values_output = { 1, 1, 10, 1 }
//! [reduce_by_key_int]
    CHECK_RANGE_EQUAL(int, 4, keys_output,   (0, 2, -3,  4));
    CHECK_RANGE_EQUAL(int, 4, values_output, (1, 1, 10, 1));
}

BOOST_AUTO_TEST_CASE(reduce_by_key_int_long_vector)
{
    size_t size = 1024;
    bc::vector<int> keys_input(size, int(0), queue);
    bc::vector<int> values_input(size, int(1), queue);

    bc::vector<int> keys_output(size, context);
    bc::vector<int> values_output(size, context);

    bc::reduce_by_key(keys_input.begin(), keys_input.end(), values_input.begin(),
                      keys_output.begin(), values_output.begin(), queue);

    CHECK_RANGE_EQUAL(int, 1, keys_output,   (0));
    CHECK_RANGE_EQUAL(int, 1, values_output, (static_cast<int>(size)));

    keys_input[137] = 1;
    keys_input[677] = 1;
    keys_input[1001] = 1;
    bc::inclusive_scan(keys_input.begin(), keys_input.end(), keys_input.begin(), queue);

    bc::reduce_by_key(keys_input.begin(), keys_input.end(), values_input.begin(),
                      keys_output.begin(), values_output.begin(), queue);

    CHECK_RANGE_EQUAL(int, 4, keys_output,   (0, 1, 2, 3));
    CHECK_RANGE_EQUAL(int, 4, values_output, (137, 540, 324, 23));
}

BOOST_AUTO_TEST_CASE(reduce_by_key_empty_vector)
{
    bc::vector<int> keys_input(context);
    bc::vector<int> values_input(context);

    bc::vector<int> keys_output(context);
    bc::vector<int> values_output(context);

    bc::reduce_by_key(keys_input.begin(), keys_input.end(), values_input.begin(),
                      keys_output.begin(), values_output.begin(), queue);

    BOOST_CHECK(keys_output.empty());
    BOOST_CHECK(values_output.empty());
}

BOOST_AUTO_TEST_CASE(reduce_by_key_int_one_key_value)
{
    int keys[] = { 22 };
    int data[] = { -9 };

    bc::vector<int> keys_input(keys, keys + 1, queue);
    bc::vector<int> values_input(data, data + 1, queue);

    bc::vector<int> keys_output(1, context);
    bc::vector<int> values_output(1, context);

    bc::reduce_by_key(keys_input.begin(), keys_input.end(), values_input.begin(),
                      keys_output.begin(), values_output.begin(), queue);

    CHECK_RANGE_EQUAL(int, 1, keys_output,   (22));
    CHECK_RANGE_EQUAL(int, 1, values_output, (-9));
}

BOOST_AUTO_TEST_CASE(reduce_by_key_int_min_max)
{
    int keys[] = { 0, 2, 2, 3, 3, 3, 3, 3, 4 };
    int data[] = { 1, 2, 1, -3, 1, 4, 2, 5, 77 };

    bc::vector<int> keys_input(keys, keys + 9, queue);
    bc::vector<int> values_input(data, data + 9, queue);

    bc::vector<int> keys_output(9, context);
    bc::vector<int> values_output(9, context);

    bc::reduce_by_key(keys_input.begin(), keys_input.end(), values_input.begin(),
                      keys_output.begin(), values_output.begin(), bc::min<int>(),
                      bc::equal_to<int>(), queue);

    CHECK_RANGE_EQUAL(int, 4, keys_output,   (0, 2, 3,  4));
    CHECK_RANGE_EQUAL(int, 4, values_output, (1, 1, -3, 77));

    bc::reduce_by_key(keys_input.begin(), keys_input.end(), values_input.begin(),
                      keys_output.begin(), values_output.begin(), bc::max<int>(),
                      bc::equal_to<int>(), queue);

    CHECK_RANGE_EQUAL(int, 4, keys_output,   (0, 2, 3,  4));
    CHECK_RANGE_EQUAL(int, 4, values_output, (1, 2, 5, 77));
}

BOOST_AUTO_TEST_CASE(reduce_by_key_float_max)
{
    int keys[] = { 0, 2, 2, 3, 3, 3, 3, 3, 4 };
    float data[] = { 1.0, 2.0, -1.5, -3.0, 1.0, -0.24, 2, 5, 77.1 };

    bc::vector<int> keys_input(keys, keys + 9, queue);
    bc::vector<float> values_input(data, data + 9, queue);

    bc::vector<int> keys_output(9, context);
    bc::vector<float> values_output(9, context);

    bc::reduce_by_key(keys_input.begin(), keys_input.end(), values_input.begin(),
                      keys_output.begin(), values_output.begin(), bc::max<float>(),
                      queue);

    CHECK_RANGE_EQUAL(int, 4, keys_output, (0, 2, 3, 4));
    BOOST_CHECK_CLOSE(float(values_output[0]), 1.0f, 1e-4f);
    BOOST_CHECK_CLOSE(float(values_output[1]), 2.0f, 1e-4f);
    BOOST_CHECK_CLOSE(float(values_output[2]), 5.0f, 1e-4f);
    BOOST_CHECK_CLOSE(float(values_output[3]), 77.1f, 1e-4f);
}

BOOST_AUTO_TEST_CASE(reduce_by_key_int2)
{
    using bc::int2_;

    int keys[] = { 0, 2, 3, 3, 3, 3, 4, 4 };
    int2_ data[] = {
        int2_(0, 1), int2_(-3, 2), int2_(0, 1), int2_(0, 1),
        int2_(-3, 0), int2_(0, 0), int2_(-3, 2), int2_(-7, -2)
    };

    bc::vector<int> keys_input(keys, keys + 8, queue);
    bc::vector<int2_> values_input(data, data + 8, queue);

    bc::vector<int> keys_output(8, context);
    bc::vector<int2_> values_output(8, context);

    bc::reduce_by_key(keys_input.begin(), keys_input.end(), values_input.begin(),
                      keys_output.begin(), values_output.begin(), queue);

    CHECK_RANGE_EQUAL(int, 4, keys_output, (0, 2, 3, 4));
    CHECK_RANGE_EQUAL(int2_, 4, values_output,
        (int2_(0, 1), int2_(-3, 2), int2_(-3, 2), int2_(-10, 0)));
}

BOOST_AUTO_TEST_CASE(reduce_by_key_int2_long_vector)
{
    using bc::int2_;

    size_t size = 1024;
    bc::vector<int> keys_input(size, int(0), queue);
    bc::vector<int2_> values_input(size, int2_(1, -1), queue);

    bc::vector<int> keys_output(size, context);
    bc::vector<int2_> values_output(size, context);

    bc::reduce_by_key(keys_input.begin(), keys_input.end(), values_input.begin(),
                      keys_output.begin(), values_output.begin(), queue);

    CHECK_RANGE_EQUAL(int, 1, keys_output,   (0));
    CHECK_RANGE_EQUAL(int2_, 1, values_output, (int2_(int(size), -int(size))));

    keys_input[137] = 1;
    keys_input[677] = 1;
    keys_input[1001] = 1;
    bc::inclusive_scan(keys_input.begin(), keys_input.end(), keys_input.begin(), queue);

    bc::reduce_by_key(keys_input.begin(), keys_input.end(), values_input.begin(),
                      keys_output.begin(), values_output.begin(), queue);

    CHECK_RANGE_EQUAL(int, 4, keys_output,   (0, 1, 2, 3));
    CHECK_RANGE_EQUAL(int2_, 4, values_output,
        (int2_(137, -137), int2_(540, -540), int2_(324, -324), int2_(23, -23)));
}

BOOST_AUTO_TEST_SUITE_END()
