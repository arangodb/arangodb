//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestTransformReduce
#include <boost/test/unit_test.hpp>

#include <boost/compute/lambda.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/functional.hpp>
#include <boost/compute/algorithm/transform_reduce.hpp>
#include <boost/compute/container/vector.hpp>

#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(sum_abs_int_doctest)
{
    using boost::compute::abs;
    using boost::compute::plus;

    int data[] = { 1, -2, -3, -4, 5 };
    compute::vector<int> vec(data, data + 5, queue);

//! [sum_abs_int]
int sum = 0;
boost::compute::transform_reduce(
    vec.begin(), vec.end(), &sum, abs<int>(), plus<int>(), queue
);
//! [sum_abs_int]

    BOOST_CHECK_EQUAL(sum, 15);
}

BOOST_AUTO_TEST_CASE(multiply_vector_length)
{
    float data[] = { 2.0f, 0.0f, 0.0f, 0.0f,
                     0.0f, 3.0f, 0.0f, 0.0f,
                     0.0f, 0.0f, 4.0f, 0.0f };
    compute::vector<compute::float4_> vector(
        reinterpret_cast<compute::float4_ *>(data),
        reinterpret_cast<compute::float4_ *>(data) + 3,
        queue
    );

    float product;
    compute::transform_reduce(
        vector.begin(),
        vector.end(),
        &product,
        compute::length<compute::float4_>(),
        compute::multiplies<float>(),
        queue
    );
    BOOST_CHECK_CLOSE(product, 24.0f, 1e-4f);
}

BOOST_AUTO_TEST_CASE(mean_and_std_dev)
{
    using compute::lambda::_1;
    using compute::lambda::pow;

    float data[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    compute::vector<float> vector(data, data + 10, queue);

    float sum;
    compute::reduce(
        vector.begin(),
        vector.end(),
        &sum,
        compute::plus<float>(),
        queue
    );

    float mean = sum / vector.size();
    BOOST_CHECK_CLOSE(mean, 5.5f, 1e-4);

    compute::transform_reduce(
        vector.begin(),
        vector.end(),
        &sum,
        pow(_1 - mean, 2),
        compute::plus<float>(),
        queue
    );

    float variance = sum / vector.size();
    BOOST_CHECK_CLOSE(variance, 8.25f, 1e-4);

    float std_dev = std::sqrt(variance);
    BOOST_CHECK_CLOSE(std_dev, 2.8722813232690143, 1e-4);
}

BOOST_AUTO_TEST_SUITE_END()
