//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestSortByTransform
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/copy_n.hpp>
#include <boost/compute/algorithm/is_sorted.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/experimental/sort_by_transform.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(sort_int_by_abs)
{
    int data[] = { 1, -2, 4, -3, 0, 5, -8, -9 };
    compute::vector<int> vector(data, data + 8, queue);

    compute::experimental::sort_by_transform(
        vector.begin(),
        vector.end(),
        compute::abs<int>(),
        compute::less<int>(),
        queue
    );

    CHECK_RANGE_EQUAL(int, 8, vector, (0, 1, -2, -3, 4, 5, -8, -9));
}

BOOST_AUTO_TEST_CASE(sort_vectors_by_length)
{
    using compute::float4_;

    float data[] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 1.0f, 0.0f,
        3.0f, 2.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f
    };

    compute::vector<float4_> vector(4, context);
    compute::copy_n(
        reinterpret_cast<float4_ *>(data), 4, vector.begin(), queue
    );

    compute::experimental::sort_by_transform(
        vector.begin(),
        vector.end(),
        compute::length<float4_>(),
        compute::less<float>(),
        queue
    );

    std::vector<float4_> host_vector(4);
    compute::copy(
        vector.begin(), vector.end(), host_vector.begin(), queue
    );
    BOOST_CHECK_EQUAL(host_vector[0], float4_(0.0f, 0.0f, 0.5f, 0.0f));
    BOOST_CHECK_EQUAL(host_vector[1], float4_(1.0f, 0.0f, 0.0f, 0.0f));
    BOOST_CHECK_EQUAL(host_vector[2], float4_(0.0f, 1.0f, 1.0f, 0.0f));
    BOOST_CHECK_EQUAL(host_vector[3], float4_(3.0f, 2.0f, 1.0f, 0.0f));
}

BOOST_AUTO_TEST_CASE(sort_vectors_by_component)
{
    using compute::float4_;

    float data[] = {
        1.0f, 2.0f, 3.0f, 0.0f,
        9.0f, 8.0f, 7.0f, 0.0f,
        4.0f, 5.0f, 6.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f
    };

    compute::vector<float4_> vector(4, context);
    compute::copy_n(
        reinterpret_cast<float4_ *>(data), 4, vector.begin(), queue
    );

    // sort by y-component
    compute::experimental::sort_by_transform(
        vector.begin(),
        vector.end(),
        compute::get<1>(),
        compute::less<float>(),
        queue
    );

    std::vector<float4_> host_vector(4);
    compute::copy(
        vector.begin(), vector.end(), host_vector.begin(), queue
    );
    BOOST_CHECK_EQUAL(host_vector[0], float4_(0.0f, 0.0f, 0.0f, 0.0f));
    BOOST_CHECK_EQUAL(host_vector[1], float4_(1.0f, 2.0f, 3.0f, 0.0f));
    BOOST_CHECK_EQUAL(host_vector[2], float4_(4.0f, 5.0f, 6.0f, 0.0f));
    BOOST_CHECK_EQUAL(host_vector[3], float4_(9.0f, 8.0f, 7.0f, 0.0f));
}

BOOST_AUTO_TEST_SUITE_END()
