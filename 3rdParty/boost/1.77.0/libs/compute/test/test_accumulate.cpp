//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestAccumulate
#include <boost/test/unit_test.hpp>

#include <numeric>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/accumulate.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/container/mapped_view.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/counting_iterator.hpp>

#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(sum_int)
{
    int data[] = { 2, 4, 6, 8 };
    boost::compute::vector<int> vector(data, data + 4, queue);
    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(vector.begin(), vector.end(), 0, queue),
        20
    );

    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(vector.begin(), vector.end(), -10, queue),
        10
    );

    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(vector.begin(), vector.end(), 5, queue),
        25
    );
}

BOOST_AUTO_TEST_CASE(product_int)
{
    int data[] = { 2, 4, 6, 8 };
    boost::compute::vector<int> vector(data, data + 4, queue);
    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(
            vector.begin(), vector.end(), 1, boost::compute::multiplies<int>(),
            queue),
        384
    );

    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(
            vector.begin(), vector.end(), -1, boost::compute::multiplies<int>(),
            queue),
        -384
    );

    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(
            vector.begin(), vector.end(), 2, boost::compute::multiplies<int>(),
            queue),
        768
    );
}

BOOST_AUTO_TEST_CASE(quotient_int)
{
    int data[] = { 2, 8, 16 };
    boost::compute::vector<int> vector(data, data + 3, queue);
    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(
            vector.begin(),
            vector.end(),
            1024,
            boost::compute::divides<int>(),
            queue
        ),
        4
    );
}

BOOST_AUTO_TEST_CASE(sum_counting_iterator)
{
    // sum 0 -> 9
    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(
            boost::compute::make_counting_iterator(0),
            boost::compute::make_counting_iterator(10),
            0,
            boost::compute::plus<int>(),
            queue
        ),
        45
    );

    // sum 0 -> 9 + 7
    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(
            boost::compute::make_counting_iterator(0),
            boost::compute::make_counting_iterator(10),
            7,
            boost::compute::plus<int>(),
            queue
        ),
        52
    );

    // sum 15 -> 24
    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(
            boost::compute::make_counting_iterator(15),
            boost::compute::make_counting_iterator(25),
            0,
            boost::compute::plus<int>(),
            queue
        ),
        195
    );

    // sum -5 -> 10
    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(
            boost::compute::make_counting_iterator(-5),
            boost::compute::make_counting_iterator(10),
            0,
            boost::compute::plus<int>(),
            queue
        ),
        30
    );

    // sum -5 -> 10 - 2
    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(
            boost::compute::make_counting_iterator(-5),
            boost::compute::make_counting_iterator(10),
            -2,
            boost::compute::plus<int>(),
            queue
        ),
        28
    );
}

BOOST_AUTO_TEST_CASE(sum_iota)
{
    // size 0
    boost::compute::vector<int> vector(0, context);

    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(vector.begin(), vector.end(), 0, queue),
        0
    );

    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(vector.begin(), vector.end(), 4, queue),
        4
    );

    // size 50
    vector.resize(50);
    boost::compute::iota(vector.begin(), vector.end(), 0, queue);

    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(vector.begin(), vector.end(), 0, queue),
        1225
    );

    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(vector.begin(), vector.end(), 11, queue),
        1236
    );

    // size 1000
    vector.resize(1000);
    boost::compute::iota(vector.begin(), vector.end(), 0, queue);

    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(vector.begin(), vector.end(), 0, queue),
        499500
    );

    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(vector.begin(), vector.end(), -45, queue),
        499455
    );

    // size 1025
    vector.resize(1025);
    boost::compute::iota(vector.begin(), vector.end(), 0, queue);

    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(vector.begin(), vector.end(), 0, queue),
        524800
    );

    BOOST_CHECK_EQUAL(
        boost::compute::accumulate(vector.begin(), vector.end(), 2, queue),
        524802
    );
}

BOOST_AUTO_TEST_CASE(min_and_max)
{
    using boost::compute::int2_;

    int data[] = { 5, 3, 1, 6, 4, 2 };
    boost::compute::vector<int> vector(data, data + 6, queue);

    BOOST_COMPUTE_FUNCTION(int2_, min_and_max, (int2_ accumulator, const int value),
    {
        return (int2)((min)(accumulator.x, value), (max)(accumulator.y, value));
    });

    int2_ result = boost::compute::accumulate(
        vector.begin(), vector.end(), int2_(100, -100), min_and_max, queue
    );
    BOOST_CHECK_EQUAL(result[0], 1);
    BOOST_CHECK_EQUAL(result[1], 6);
}

BOOST_AUTO_TEST_CASE(min_max)
{
    float data[] = { 1.2f, 5.5f, 0.1f, 9.6f, 4.2f, 6.7f, 9.0f, 3.4f };
    boost::compute::vector<float> vec(data, data + 8, queue);

    using ::boost::compute::min;
    using ::boost::compute::max;

    float min_value = boost::compute::accumulate(
        vec.begin(), vec.end(), (std::numeric_limits<float>::max)(), min<float>(), queue
    );
    BOOST_CHECK_EQUAL(min_value, 0.1f);

    float max_value = boost::compute::accumulate(
        vec.begin(), vec.end(), (std::numeric_limits<float>::min)(), max<float>(), queue
    );
    BOOST_CHECK_EQUAL(max_value, 9.6f);

    // find min with init less than any value in the array
    min_value = boost::compute::accumulate(
        vec.begin(), vec.end(), -1.f, min<float>(), queue
    );
    BOOST_CHECK_EQUAL(min_value, -1.f);

    // find max with init greater than any value in the array
    max_value = boost::compute::accumulate(
        vec.begin(), vec.end(), 10.f, max<float>(), queue
    );
    BOOST_CHECK_EQUAL(max_value, 10.f);
}

template<class T>
void ensure_std_accumulate_equality(const std::vector<T> &data,
                                    boost::compute::command_queue &queue)
{
    boost::compute::mapped_view<T> view(&data[0], data.size(), queue.get_context());

    BOOST_CHECK_EQUAL(
        std::accumulate(data.begin(), data.end(), 0),
        boost::compute::accumulate(view.begin(), view.end(), 0, queue)
    );
}

BOOST_AUTO_TEST_CASE(std_accumulate_equality)
{
    // test accumulate() with int
    int data1[] = { 1, 2, 3, 4 };
    std::vector<int> vec1(data1, data1 + 4);
    ensure_std_accumulate_equality(vec1, queue);

    vec1.resize(10000);
    std::fill(vec1.begin(), vec1.end(), 2);
    ensure_std_accumulate_equality(vec1, queue);

    // test accumulate() with float
    float data2[] = { 1.2f, 2.3f, 4.5f, 6.7f, 8.9f };
    std::vector<float> vec2(data2, data2 + 5);
    ensure_std_accumulate_equality(vec2, queue);

    vec2.resize(10000);
    std::fill(vec2.begin(), vec2.end(), 1.01f);
    ensure_std_accumulate_equality(vec2, queue);

    // test accumulate() with double
    if(device.supports_extension("cl_khr_fp64")){
        double data3[] = { 1.2, 2.3, 4.5, 6.7, 8.9 };
        std::vector<double> vec3(data3, data3 + 5);
        ensure_std_accumulate_equality(vec3, queue);

        vec3.resize(10000);
        std::fill(vec3.begin(), vec3.end(), 2.02);
        ensure_std_accumulate_equality(vec3, queue);
    }
}

BOOST_AUTO_TEST_SUITE_END()
