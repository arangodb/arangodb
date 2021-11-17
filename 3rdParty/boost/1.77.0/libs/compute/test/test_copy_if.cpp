//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestCopyIf
#include <boost/test/unit_test.hpp>

#include <boost/compute/lambda.hpp>
#include <boost/compute/algorithm/copy_if.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;
namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(copy_if_int)
{
    int data[] = { 1, 6, 3, 5, 8, 2, 4 };
    bc::vector<int> input(data, data + 7, queue);

    bc::vector<int> output(input.size(), context);
    bc::fill(output.begin(), output.end(), -1, queue);

    using ::boost::compute::_1;

    bc::vector<int>::iterator iter =
        bc::copy_if(input.begin(), input.end(),
                    output.begin(), _1 < 5, queue);
    BOOST_VERIFY(iter == output.begin() + 4);
    CHECK_RANGE_EQUAL(int, 7, output, (1, 3, 2, 4, -1, -1, -1));

    bc::fill(output.begin(), output.end(), 42, queue);
    iter =
        bc::copy_if(input.begin(), input.end(),
                    output.begin(), _1 * 2 >= 10, queue);
    BOOST_VERIFY(iter == output.begin() + 3);
    CHECK_RANGE_EQUAL(int, 7, output, (6, 5, 8, 42, 42, 42, 42));
}

BOOST_AUTO_TEST_CASE(copy_if_odd)
{
    int data[] = { 1, 2, 3, 4, 5, 1, 2, 3, 4, 5 };
    bc::vector<int> input(data, data + 10, queue);

    using ::boost::compute::_1;

    bc::vector<int> odds(input.size(), context);
    bc::vector<int>::iterator odds_end =
        bc::copy_if(input.begin(), input.end(),
                    odds.begin(), _1 % 2 == 1, queue);
    BOOST_CHECK(odds_end == odds.begin() + 6);
    CHECK_RANGE_EQUAL(int, 6, odds, (1, 3, 5, 1, 3, 5));

    bc::vector<int> evens(input.size(), context);
    bc::vector<int>::iterator evens_end =
        bc::copy_if(input.begin(), input.end(),
                    evens.begin(), _1 % 2 == 0, queue);
    BOOST_CHECK(evens_end == evens.begin() + 4);
    CHECK_RANGE_EQUAL(int, 4, evens, (2, 4, 2, 4));
}

BOOST_AUTO_TEST_CASE(clip_points_below_plane)
{
    float data[] = { 1.0f, 2.0f, 3.0f, 0.0f,
                     -1.0f, 2.0f, 3.0f, 0.0f,
                     -2.0f, -3.0f, 4.0f, 0.0f,
                     4.0f, -3.0f, 2.0f, 0.0f };
    bc::vector<bc::float4_> points(reinterpret_cast<bc::float4_ *>(data),
                                   reinterpret_cast<bc::float4_ *>(data) + 4,
                                   queue);

    // create output vector filled with (0, 0, 0, 0)
    bc::vector<bc::float4_> output(points.size(), context);
    bc::fill(output.begin(), output.end(),
             bc::float4_(0.0f, 0.0f, 0.0f, 0.0f), queue);

    // define the plane (at origin, +X normal)
    bc::float4_ plane_origin(0.0f, 0.0f, 0.0f, 0.0f);
    bc::float4_ plane_normal(1.0f, 0.0f, 0.0f, 0.0f);

    using ::boost::compute::_1;
    using ::boost::compute::lambda::dot;

    bc::vector<bc::float4_>::const_iterator iter =
        bc::copy_if(points.begin(),
                    points.end(),
                    output.begin(),
                    dot(_1 - plane_origin, plane_normal) > 0.0f,
                    queue);
    BOOST_CHECK(iter == output.begin() + 2);
}

BOOST_AUTO_TEST_CASE(copy_index_if_int)
{
    int data[] = { 1, 6, 3, 5, 8, 2, 4 };
    compute::vector<int> input(data, data + 7, queue);

    compute::vector<int> output(input.size(), context);
    compute::fill(output.begin(), output.end(), -1, queue);

    using ::boost::compute::_1;
    using ::boost::compute::detail::copy_index_if;

    compute::vector<int>::iterator iter =
        copy_index_if(input.begin(), input.end(), output.begin(),
                      _1 < 5, queue);
    BOOST_VERIFY(iter == output.begin() + 4);
    CHECK_RANGE_EQUAL(int, 7, output, (0, 2, 5, 6, -1, -1, -1));
}

BOOST_AUTO_TEST_SUITE_END()
