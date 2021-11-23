//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestClosure
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/closure.hpp>
#include <boost/compute/function.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/algorithm/transform_reduce.hpp>
#include <boost/compute/container/array.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/counting_iterator.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(add_two)
{
    int two = 2;
    BOOST_COMPUTE_CLOSURE(int, add_two, (int x), (two),
    {
        return x + two;
    });

    int data[] = { 1, 2, 3, 4 };
    compute::vector<int> vector(data, data + 4, queue);

    compute::transform(
        vector.begin(), vector.end(), vector.begin(), add_two, queue
    );
    CHECK_RANGE_EQUAL(int, 4, vector, (3, 4, 5, 6));
}

BOOST_AUTO_TEST_CASE(add_two_and_pi)
{
    int two = 2;
    float pi = 3.14f;
    BOOST_COMPUTE_CLOSURE(float, add_two_and_pi, (float x), (two, pi),
    {
        return x + two + pi;
    });

    float data[] = { 1.9f, 2.2f, 3.4f, 4.7f };
    compute::vector<float> vector(data, data + 4, queue);

    compute::transform(
        vector.begin(), vector.end(), vector.begin(), add_two_and_pi, queue
    );

    std::vector<float> results(4);
    compute::copy(vector.begin(), vector.end(), results.begin(), queue);
    BOOST_CHECK_CLOSE(results[0], 7.04f, 1e-6);
    BOOST_CHECK_CLOSE(results[1], 7.34f, 1e-6);
    BOOST_CHECK_CLOSE(results[2], 8.54f, 1e-6);
    BOOST_CHECK_CLOSE(results[3], 9.84f, 1e-6);
}

BOOST_AUTO_TEST_CASE(add_y)
{
    // setup input and output vectors
    int data[] = { 1, 2, 3, 4 };
    compute::vector<int> input(data, data + 4, queue);
    compute::vector<int> output(4, context);

    // make closure which adds 'y' to each value
    int y = 2;
    BOOST_COMPUTE_CLOSURE(int, add_y, (int x), (y),
    {
        return x + y;
    });

    compute::transform(
        input.begin(), input.end(), output.begin(), add_y, queue
    );
    CHECK_RANGE_EQUAL(int, 4, output, (3, 4, 5, 6));

    // change y and run again
    y = 4;

    compute::transform(
        input.begin(), input.end(), output.begin(), add_y, queue
    );
    CHECK_RANGE_EQUAL(int, 4, output, (5, 6, 7, 8));
}

BOOST_AUTO_TEST_CASE(scale_add_vec)
{
    const int N = 10;
    float s = 4.5;
    compute::vector<float> a(N, context);
    compute::vector<float> b(N, context);
    a.assign(N, 1.0f, queue);
    b.assign(N, 2.0f, queue);

    BOOST_COMPUTE_CLOSURE(float, scaleAddVec, (float b, float a), (s),
    {
        return b * s + a;
    });
    compute::transform(b.begin(), b.end(), a.begin(), b.begin(), scaleAddVec, queue);
}

BOOST_AUTO_TEST_CASE(capture_vector)
{
    int data[] = { 6, 7, 8, 9 };
    compute::vector<int> vec(data, data + 4, queue);

    BOOST_COMPUTE_CLOSURE(int, get_vec, (int i), (vec),
    {
        return vec[i];
    });

    // run using a counting iterator to copy from vec to output
    compute::vector<int> output(4, context);
    compute::transform(
        compute::make_counting_iterator(0),
        compute::make_counting_iterator(4),
        output.begin(),
        get_vec,
        queue
    );
    CHECK_RANGE_EQUAL(int, 4, output, (6, 7, 8, 9));

    // fill vec with 4's and run again
    compute::fill(vec.begin(), vec.end(), 4, queue);
    compute::transform(
        compute::make_counting_iterator(0),
        compute::make_counting_iterator(4),
        output.begin(),
        get_vec,
        queue
    );
    CHECK_RANGE_EQUAL(int, 4, output, (4, 4, 4, 4));
}

BOOST_AUTO_TEST_CASE(capture_array)
{
    int data[] = { 1, 2, 3, 4 };
    compute::array<int, 4> array(context);
    compute::copy(data, data + 4, array.begin(), queue);

    BOOST_COMPUTE_CLOSURE(int, negative_array_value, (int i), (array),
    {
        return -array[i];
    });

    compute::vector<int> output(4, context);
    compute::transform(
        compute::make_counting_iterator(0),
        compute::make_counting_iterator(4),
        output.begin(),
        negative_array_value,
        queue
    );
    CHECK_RANGE_EQUAL(int, 4, output, (-1, -2, -3, -4));
}

BOOST_AUTO_TEST_CASE(triangle_area)
{
    using compute::uint4_;
    using compute::float4_;

    compute::vector<uint4_> triangle_indices(context);
    compute::vector<float4_> triangle_vertices(context);

    triangle_vertices.push_back(float4_(0, 0, 0, 1), queue);
    triangle_vertices.push_back(float4_(1, 1, 0, 1), queue);
    triangle_vertices.push_back(float4_(1, 0, 0, 1), queue);
    triangle_vertices.push_back(float4_(2, 0, 0, 1), queue);

    triangle_indices.push_back(uint4_(0, 1, 2, 0), queue);
    triangle_indices.push_back(uint4_(2, 1, 3, 0), queue);

    queue.finish();

    BOOST_COMPUTE_CLOSURE(float, triangle_area, (const uint4_ i), (triangle_vertices),
    {
        // load triangle vertices
        const float4 a = triangle_vertices[i.x];
        const float4 b = triangle_vertices[i.y];
        const float4 c = triangle_vertices[i.z];

        // return area of triangle
        return length(cross(b-a, c-a)) / 2;
    });

    // compute area of each triangle
    compute::vector<float> triangle_areas(triangle_indices.size(), context);

    compute::transform(
        triangle_indices.begin(),
        triangle_indices.end(),
        triangle_areas.begin(),
        triangle_area,
        queue
    );

    // compute total area of all triangles
    float total_area = 0;

    compute::transform_reduce(
        triangle_indices.begin(),
        triangle_indices.end(),
        &total_area,
        triangle_area,
        compute::plus<float>(),
        queue
    );
    BOOST_CHECK_CLOSE(total_area, 1.f, 1e-6);
}

BOOST_AUTO_TEST_SUITE_END()
