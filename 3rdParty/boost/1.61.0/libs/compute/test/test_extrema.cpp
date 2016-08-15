//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestExtrema
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/algorithm/fill.hpp>
#include <boost/compute/algorithm/max_element.hpp>
#include <boost/compute/algorithm/min_element.hpp>
#include <boost/compute/algorithm/minmax_element.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/transform_iterator.hpp>

#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(int_min_max)
{
    boost::compute::vector<int> vector(size_t(4096), int(0), queue);
    boost::compute::iota(vector.begin(), (vector.begin() + 512), 1, queue);
    boost::compute::fill((vector.end() - 512), vector.end(), 513, queue);

    boost::compute::vector<int>::iterator min_iter =
        boost::compute::min_element(vector.begin(), vector.end(), queue);
    BOOST_CHECK(min_iter == vector.begin() + 512);
    BOOST_CHECK_EQUAL((vector.begin() + 512).read(queue), 0);
    BOOST_CHECK_EQUAL(min_iter.read(queue), 0);

    boost::compute::vector<int>::iterator max_iter =
        boost::compute::max_element(vector.begin(), vector.end(), queue);
    BOOST_CHECK(max_iter == vector.end() - 512);
    BOOST_CHECK_EQUAL((vector.end() - 512).read(queue), 513);
    BOOST_CHECK_EQUAL(max_iter.read(queue), 513);
}

BOOST_AUTO_TEST_CASE(int2_min_max_custom_comparision_function)
{
    using boost::compute::int2_;

    boost::compute::vector<int2_> vector(context);
    vector.push_back(int2_(1, 10), queue);
    vector.push_back(int2_(2, -100), queue);
    vector.push_back(int2_(3, 30), queue);
    vector.push_back(int2_(4, 20), queue);
    vector.push_back(int2_(5, 5), queue);
    vector.push_back(int2_(6, -80), queue);
    vector.push_back(int2_(7, 21), queue);
    vector.push_back(int2_(8, -5), queue);

    BOOST_COMPUTE_FUNCTION(bool, compare_second, (const int2_ a, const int2_ b),
    {
        return a.y < b.y;
    });

    boost::compute::vector<int2_>::iterator min_iter =
        boost::compute::min_element(
            vector.begin(), vector.end(), compare_second, queue
         );
    BOOST_CHECK(min_iter == vector.begin() + 1);
    BOOST_CHECK_EQUAL(*min_iter, int2_(2, -100));

    boost::compute::vector<int2_>::iterator max_iter =
        boost::compute::max_element(
            vector.begin(), vector.end(), compare_second, queue
        );
    BOOST_CHECK(max_iter == vector.begin() + 2);
    BOOST_CHECK_EQUAL(*max_iter, int2_(3, 30));
}

BOOST_AUTO_TEST_CASE(iota_min_max)
{
    boost::compute::vector<int> vector(5000, context);

    // fill with 0 -> 4999
    boost::compute::iota(vector.begin(), vector.end(), 0, queue);

    boost::compute::vector<int>::iterator min_iter =
        boost::compute::min_element(vector.begin(), vector.end(), queue);
    BOOST_CHECK(min_iter == vector.begin());
    BOOST_CHECK_EQUAL(*min_iter, 0);

    boost::compute::vector<int>::iterator max_iter =
        boost::compute::max_element(vector.begin(), vector.end(), queue);
    BOOST_CHECK(max_iter == vector.end() - 1);
    BOOST_CHECK_EQUAL(*max_iter, 4999);

    min_iter =
        boost::compute::min_element(
            vector.begin() + 1000,
            vector.end() - 1000,
            queue
        );
    BOOST_CHECK(min_iter == vector.begin() + 1000);
    BOOST_CHECK_EQUAL(*min_iter, 1000);

    max_iter =
        boost::compute::max_element(
            vector.begin() + 1000,
            vector.end() - 1000,
            queue
        );
    BOOST_CHECK(max_iter == vector.begin() + 3999);
    BOOST_CHECK_EQUAL(*max_iter, 3999);

    // fill with -2500 -> 2499
    boost::compute::iota(vector.begin(), vector.end(), -2500, queue);
    min_iter =
        boost::compute::min_element(vector.begin(), vector.end(), queue);
    BOOST_CHECK(min_iter == vector.begin());
    BOOST_CHECK_EQUAL(*min_iter, -2500);

    max_iter =
        boost::compute::max_element(vector.begin(), vector.end(), queue);
    BOOST_CHECK(max_iter == vector.end() - 1);
    BOOST_CHECK_EQUAL(*max_iter, 2499);
}

// uses max_element() and length() to find the longest 2d vector
BOOST_AUTO_TEST_CASE(max_vector_length)
{
    float data[] = { -1.5f, 3.2f,
                     10.0f, 0.0f,
                     -4.2f, 2.0f,
                     0.0f, 0.5f,
                     1.9f, 1.9f };
    boost::compute::vector<boost::compute::float2_> vector(
        reinterpret_cast<boost::compute::float2_ *>(data),
        reinterpret_cast<boost::compute::float2_ *>(data) + 5,
        queue
    );

    // find length of the longest vector
    typedef boost::compute::transform_iterator<
                boost::compute::vector<boost::compute::float2_>::iterator,
                boost::compute::length<boost::compute::float2_>
            > length_transform_iter;

    length_transform_iter max_iter =
        boost::compute::max_element(
            boost::compute::make_transform_iterator(
                vector.begin(),
                boost::compute::length<boost::compute::float2_>()
            ),
            boost::compute::make_transform_iterator(
                vector.end(),
                boost::compute::length<boost::compute::float2_>()
            ),
            queue
        );
    BOOST_CHECK(
        max_iter == boost::compute::make_transform_iterator(
                        vector.begin() + 1,
                        boost::compute::length<boost::compute::float2_>()
                    )
    );
    BOOST_CHECK(max_iter.base() == vector.begin() + 1);
    BOOST_CHECK_EQUAL(*max_iter, float(10.0));

    // find length of the shortest vector
    length_transform_iter min_iter =
        boost::compute::min_element(
            boost::compute::make_transform_iterator(
                vector.begin(),
                boost::compute::length<boost::compute::float2_>()
            ),
            boost::compute::make_transform_iterator(
                vector.end(),
                boost::compute::length<boost::compute::float2_>()
            ),
            queue
        );
    BOOST_CHECK(
        min_iter == boost::compute::make_transform_iterator(
                        vector.begin() + 3,
                        boost::compute::length<boost::compute::float2_>()
                    )
    );
    BOOST_CHECK(min_iter.base() == vector.begin() + 3);
    BOOST_CHECK_EQUAL(*min_iter, float(0.5));
}

// uses max_element() and popcount() to find the value with the most 1 bits
BOOST_AUTO_TEST_CASE(max_bits_set)
{
    using boost::compute::uint_;

    uint_ data[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    boost::compute::vector<uint_> vector(data, data + 10, queue);

    boost::compute::vector<uint_>::iterator iter =
        boost::compute::max_element(
            boost::compute::make_transform_iterator(
                vector.begin(),
                boost::compute::popcount<uint_>()
            ),
            boost::compute::make_transform_iterator(
                vector.end(),
                boost::compute::popcount<uint_>()
            ),
            queue
        ).base();

    BOOST_CHECK(iter == vector.begin() + 7);
    BOOST_CHECK_EQUAL(uint_(*iter), uint_(7));
}

BOOST_AUTO_TEST_SUITE_END()
