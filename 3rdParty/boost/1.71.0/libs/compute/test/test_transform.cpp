//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestTransform
#include <boost/test/unit_test.hpp>

#include <boost/compute/lambda.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/function.hpp>
#include <boost/compute/functional.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/counting_iterator.hpp>
#include <boost/compute/functional/field.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;
namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(transform_int_abs)
{
    int data[] = { 1, -2, -3, -4, 5 };
    bc::vector<int> vector(data, data + 5, queue);
    CHECK_RANGE_EQUAL(int, 5, vector, (1, -2, -3, -4, 5));

    bc::transform(vector.begin(),
                  vector.end(),
                  vector.begin(),
                  bc::abs<int>(),
                  queue);
    CHECK_RANGE_EQUAL(int, 5, vector, (1, 2, 3, 4, 5));
}

BOOST_AUTO_TEST_CASE(transform_float_sqrt)
{
    float data[] = { 1.0f, 4.0f, 9.0f, 16.0f };
    bc::vector<float> vector(data, data + 4, queue);
    CHECK_RANGE_EQUAL(float, 4, vector, (1.0f, 4.0f, 9.0f, 16.0f));

    bc::transform(vector.begin(),
                  vector.end(),
                  vector.begin(),
                  bc::sqrt<float>(),
                  queue);
    queue.finish();
    BOOST_CHECK_CLOSE(float(vector[0]), 1.0f, 1e-4f);
    BOOST_CHECK_CLOSE(float(vector[1]), 2.0f, 1e-4f);
    BOOST_CHECK_CLOSE(float(vector[2]), 3.0f, 1e-4f);
    BOOST_CHECK_CLOSE(float(vector[3]), 4.0f, 1e-4f);
}

BOOST_AUTO_TEST_CASE(transform_float_clamp)
{
    float data[] = { 10.f, 20.f, 30.f, 40.f, 50.f };
    bc::vector<float> vector(data, data + 5, queue);
    CHECK_RANGE_EQUAL(float, 5, vector, (10.0f, 20.0f, 30.0f, 40.0f, 50.0f));

    bc::transform(vector.begin(),
                  vector.end(),
                  vector.begin(),
                  clamp(bc::_1, 15.f, 45.f),
                  queue);
    CHECK_RANGE_EQUAL(float, 5, vector, (15.0f, 20.0f, 30.0f, 40.0f, 45.0f));
}

BOOST_AUTO_TEST_CASE(transform_add_int)
{
    int data1[] = { 1, 2, 3, 4 };
    bc::vector<int> input1(data1, data1 + 4, queue);

    int data2[] = { 10, 20, 30, 40 };
    bc::vector<int> input2(data2, data2 + 4, queue);

    bc::vector<int> output(4, context);
    bc::transform(input1.begin(),
                  input1.end(),
                  input2.begin(),
                  output.begin(),
                  bc::plus<int>(),
                  queue);
    CHECK_RANGE_EQUAL(int, 4, output, (11, 22, 33, 44));

    bc::transform(input1.begin(),
                  input1.end(),
                  input2.begin(),
                  output.begin(),
                  bc::multiplies<int>(),
                  queue);
    CHECK_RANGE_EQUAL(int, 4, output, (10, 40, 90, 160));
}

BOOST_AUTO_TEST_CASE(transform_pow4)
{
    float data[] = { 1.0f, 2.0f, 3.0f, 4.0f };
    bc::vector<float> vector(data, data + 4, queue);
    CHECK_RANGE_EQUAL(float, 4, vector, (1.0f, 2.0f, 3.0f, 4.0f));

    bc::vector<float> result(4, context);
    bc::transform(vector.begin(),
                  vector.end(),
                  result.begin(),
                  pown(bc::_1, 4),
                  queue);
    queue.finish();
    BOOST_CHECK_CLOSE(float(result[0]), 1.0f, 1e-4f);
    BOOST_CHECK_CLOSE(float(result[1]), 16.0f, 1e-4f);
    BOOST_CHECK_CLOSE(float(result[2]), 81.0f, 1e-4f);
    BOOST_CHECK_CLOSE(float(result[3]), 256.0f, 1e-4f);
}

BOOST_AUTO_TEST_CASE(transform_custom_function)
{
    float data[] = { 9.0f, 7.0f, 5.0f, 3.0f };
    bc::vector<float> vector(data, data + 4, queue);

    BOOST_COMPUTE_FUNCTION(float, pow3add4, (float x),
    {
        return pow(x, 3.0f) + 4.0f;
    });

    bc::vector<float> result(4, context);
    bc::transform(vector.begin(),
                  vector.end(),
                  result.begin(),
                  pow3add4,
                  queue);
    queue.finish();
    BOOST_CHECK_CLOSE(float(result[0]), 733.0f, 1e-4f);
    BOOST_CHECK_CLOSE(float(result[1]), 347.0f, 1e-4f);
    BOOST_CHECK_CLOSE(float(result[2]), 129.0f, 1e-4f);
    BOOST_CHECK_CLOSE(float(result[3]), 31.0f, 1e-4f);
}

BOOST_AUTO_TEST_CASE(extract_vector_component)
{
    using bc::int2_;

    int data[] = { 1, 2,
                   3, 4,
                   5, 6,
                   7, 8 };
    bc::vector<int2_> vector(
        reinterpret_cast<int2_ *>(data),
        reinterpret_cast<int2_ *>(data) + 4,
        queue
    );
    CHECK_RANGE_EQUAL(
        int2_, 4, vector,
        (int2_(1, 2), int2_(3, 4), int2_(5, 6), int2_(7, 8))
    );

    bc::vector<int> x_components(4, context);
    bc::transform(vector.begin(),
                  vector.end(),
                  x_components.begin(),
                  bc::get<0>(),
                  queue);
    CHECK_RANGE_EQUAL(int, 4, x_components, (1, 3, 5, 7));

    bc::vector<int> y_components(4, context);
    bc::transform(vector.begin(),
                  vector.end(),
                  y_components.begin(),
                  bc::get<1>(),
                  queue);
    CHECK_RANGE_EQUAL(int, 4, y_components, (2, 4, 6, 8));
}

BOOST_AUTO_TEST_CASE(transform_pinned_vector)
{
    int data[] = { 2, -3, 4, -5, 6, -7 };
    std::vector<int> vector(data, data + 6);

    bc::buffer buffer(context,
                      vector.size() * sizeof(int),
                      bc::buffer::read_write | bc::buffer::use_host_ptr,
                      &vector[0]);

    bc::transform(bc::make_buffer_iterator<int>(buffer, 0),
                  bc::make_buffer_iterator<int>(buffer, 6),
                  bc::make_buffer_iterator<int>(buffer, 0),
                  bc::abs<int>(),
                  queue);

    void *ptr = queue.enqueue_map_buffer(buffer,
                                         bc::command_queue::map_read,
                                         0,
                                         buffer.size());
    BOOST_VERIFY(ptr == &vector[0]);
    BOOST_CHECK_EQUAL(vector[0], 2);
    BOOST_CHECK_EQUAL(vector[1], 3);
    BOOST_CHECK_EQUAL(vector[2], 4);
    BOOST_CHECK_EQUAL(vector[3], 5);
    BOOST_CHECK_EQUAL(vector[4], 6);
    BOOST_CHECK_EQUAL(vector[5], 7);
    queue.enqueue_unmap_buffer(buffer, ptr);
}

BOOST_AUTO_TEST_CASE(transform_popcount)
{
    using boost::compute::uint_;

    uint_ data[] = { 0, 1, 2, 3, 4, 45, 127, 5000, 789, 15963 };
    bc::vector<uint_> input(data, data + 10, queue);
    bc::vector<uint_> output(input.size(), context);

    bc::transform(
        input.begin(),
        input.end(),
        output.begin(),
        bc::popcount<uint_>(),
        queue
    );
    CHECK_RANGE_EQUAL(uint_, 10, output, (0, 1, 1, 2, 1, 4, 7, 5, 5, 10));
}

// generates the first 25 fibonacci numbers in parallel using the
// rounding-based fibonacci formula
BOOST_AUTO_TEST_CASE(generate_fibonacci_sequence)
{
    using boost::compute::uint_;

    boost::compute::vector<uint_> sequence(25, context);

    BOOST_COMPUTE_FUNCTION(uint_, nth_fibonacci, (const uint_ n),
    {
        const float golden_ratio = (1.f + sqrt(5.f)) / 2.f;
        return floor(pown(golden_ratio, n) / sqrt(5.f) + 0.5f);
    });

    boost::compute::transform(
        boost::compute::make_counting_iterator(uint_(0)),
        boost::compute::make_counting_iterator(uint_(sequence.size())),
        sequence.begin(),
        nth_fibonacci,
        queue
    );
    CHECK_RANGE_EQUAL(
        uint_, 25, sequence,
        (0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610,
         987, 1597, 2584, 4181, 6765, 10946, 17711, 28657, 46368)
    );
}

BOOST_AUTO_TEST_CASE(field)
{
    using compute::uint2_;
    using compute::uint4_;
    using compute::field;

    unsigned int data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    compute::vector<uint4_> input(
        reinterpret_cast<uint4_ *>(data),
        reinterpret_cast<uint4_ *>(data) + 2,
        queue
    );
    compute::vector<uint2_> output(input.size(), context);

    compute::transform(
        input.begin(),
        input.end(),
        output.begin(),
        compute::field<uint2_>("xz"),
        queue
    );

    queue.finish();

    BOOST_CHECK_EQUAL(uint2_(output[0]), uint2_(1, 3));
    BOOST_CHECK_EQUAL(uint2_(output[1]), uint2_(5, 7));
}

BOOST_AUTO_TEST_CASE(transform_abs_doctest)
{
//! [transform_abs]
int data[] = { -1, -2, -3, -4 };
boost::compute::vector<int> vec(data, data + 4, queue);

using boost::compute::abs;

// calculate the absolute value for each element in-place
boost::compute::transform(
    vec.begin(), vec.end(), vec.begin(), abs<int>(), queue
);

// vec == { 1, 2, 3, 4 }
//! [transform_abs]

    CHECK_RANGE_EQUAL(int, 4, vec, (1, 2, 3, 4));
}

BOOST_AUTO_TEST_CASE(abs_if_odd)
{
    // return absolute value only for odd values
    BOOST_COMPUTE_FUNCTION(int, abs_if_odd, (int x),
    {
        if(x & 1){
            return abs(x);
        }
        else {
            return x;
        }
    });

    int data[] = { -2, -3, -4, -5, -6, -7, -8, -9 };
    compute::vector<int> vector(data, data + 8, queue);

    compute::transform(
        vector.begin(), vector.end(), vector.begin(), abs_if_odd, queue
    );

    CHECK_RANGE_EQUAL(int, 8, vector, (-2, +3, -4, +5, -6, +7, -8, +9));
}

BOOST_AUTO_TEST_SUITE_END()
