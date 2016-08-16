//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestFind
#include <boost/test/unit_test.hpp>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/lambda.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/find.hpp>
#include <boost/compute/algorithm/find_if.hpp>
#include <boost/compute/algorithm/find_if_not.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/constant_buffer_iterator.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;
namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(find_int)
{
    int data[] = { 9, 15, 1, 4, 9, 9, 4, 15, 12, 1 };
    bc::vector<int> vector(data, data + 10, queue);

    bc::vector<int>::iterator iter =
        bc::find(vector.begin(), vector.end(), 4, queue);
    BOOST_CHECK(iter == vector.begin() + 3);
    BOOST_CHECK_EQUAL(*iter, 4);

    iter = bc::find(vector.begin(), vector.end(), 12, queue);
    BOOST_CHECK(iter == vector.begin() + 8);
    BOOST_CHECK_EQUAL(*iter, 12);

    iter = bc::find(vector.begin(), vector.end(), 1, queue);
    BOOST_CHECK(iter == vector.begin() + 2);
    BOOST_CHECK_EQUAL(*iter, 1);

    iter = bc::find(vector.begin(), vector.end(), 9, queue);
    BOOST_CHECK(iter == vector.begin());
    BOOST_CHECK_EQUAL(*iter, 9);

    iter = bc::find(vector.begin(), vector.end(), 100, queue);
    BOOST_CHECK(iter == vector.end());
}

BOOST_AUTO_TEST_CASE(find_int2)
{
    using bc::int2_;

    int data[] = { 1, 2, 4, 5, 7, 8 };
    bc::vector<int2_> vector(
        reinterpret_cast<int2_ *>(data),
        reinterpret_cast<int2_ *>(data) + 3,
        queue
    );
    CHECK_RANGE_EQUAL(int2_, 3, vector, (int2_(1, 2), int2_(4, 5), int2_(7, 8)));

    bc::vector<int2_>::iterator iter =
        bc::find(vector.begin(), vector.end(), int2_(4, 5), queue);
    BOOST_CHECK(iter == vector.begin() + 1);
    BOOST_CHECK_EQUAL(*iter, int2_(4, 5));

    iter = bc::find(vector.begin(), vector.end(), int2_(10, 11), queue);
    BOOST_CHECK(iter == vector.end());
}

BOOST_AUTO_TEST_CASE(find_if_not_int)
{
    int data[] = { 2, 4, 6, 8, 1, 3, 5, 7, 9 };
    bc::vector<int> vector(data, data + 9, queue);

    bc::vector<int>::iterator iter =
        bc::find_if_not(vector.begin(), vector.end(), bc::_1 == 2, queue);
    BOOST_CHECK(iter == vector.begin() + 1);
    BOOST_CHECK_EQUAL(*iter, 4);
}

BOOST_AUTO_TEST_CASE(find_point_by_distance)
{
    using boost::compute::float2_;
    using boost::compute::lambda::_1;
    using boost::compute::lambda::distance;

    float2_ points[] = {
        float2_(0, 0), float2_(2, 2), float2_(4, 4), float2_(8, 8)
    };
    compute::vector<float2_> vec(points, points + 4, queue);

    compute::vector<float2_>::iterator iter =
        compute::find_if(vec.begin(), vec.end(), distance(_1, float2_(5, 5)) < 1.5f, queue);
    BOOST_CHECK(iter == vec.begin() + 2);

    float2_ value;
    compute::copy_n(iter, 1, &value, queue);
    BOOST_CHECK_EQUAL(value, float2_(4, 4));
}

BOOST_AUTO_TEST_SUITE_END()
