//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestMerge
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/types/pair.hpp>
#include <boost/compute/algorithm/copy_n.hpp>
#include <boost/compute/algorithm/merge.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/lambda.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(simple_merge_int)
{
    int data1[] = { 1, 3, 5, 7 };
    int data2[] = { 2, 4, 6, 8 };

    boost::compute::vector<int> v1(4, context);
    boost::compute::vector<int> v2(4, context);
    boost::compute::vector<int> v3(8, context);

    boost::compute::copy_n(data1, 4, v1.begin(), queue);
    boost::compute::copy_n(data2, 4, v2.begin(), queue);
    boost::compute::fill(v3.begin(), v3.end(), 0, queue);

    // merge v1 with v2 into v3
    boost::compute::merge(
        v1.begin(), v1.end(),
        v2.begin(), v2.end(),
        v3.begin(),
        queue
    );
    CHECK_RANGE_EQUAL(int, 8, v3, (1, 2, 3, 4, 5, 6, 7, 8));

    // merge v2 with v1 into v3
    boost::compute::merge(
        v2.begin(), v2.end(),
        v1.begin(), v1.end(),
        v3.begin(),
        queue
    );
    CHECK_RANGE_EQUAL(int, 8, v3, (1, 2, 3, 4, 5, 6, 7, 8));

    // merge v1 with v1 into v3
    boost::compute::merge(
        v1.begin(), v1.end(),
        v1.begin(), v1.end(),
        v3.begin(),
        queue
    );
    CHECK_RANGE_EQUAL(int, 8, v3, (1, 1, 3, 3, 5, 5, 7, 7));

    // merge v2 with v2 into v3
    boost::compute::merge(
        v2.begin(), v2.end(),
        v2.begin(), v2.end(),
        v3.begin(),
        queue
    );
    CHECK_RANGE_EQUAL(int, 8, v3, (2, 2, 4, 4, 6, 6, 8, 8));

    // merge v1 with empty range into v3
    boost::compute::merge(
        v1.begin(), v1.end(),
        v1.begin(), v1.begin(),
        v3.begin(),
        queue
    );
    CHECK_RANGE_EQUAL(int, 4, v3, (1, 3, 5, 7));

    // merge v2 with empty range into v3
    boost::compute::merge(
        v1.begin(), v1.begin(),
        v2.begin(), v2.end(),
        v3.begin(),
        queue
    );
    CHECK_RANGE_EQUAL(int, 4, v3, (2, 4, 6, 8));
}

BOOST_AUTO_TEST_CASE(merge_pairs)
{
    std::vector<std::pair<int, float> > data1;
    std::vector<std::pair<int, float> > data2;

    data1.push_back(std::make_pair(0, 0.1f));
    data1.push_back(std::make_pair(2, 2.1f));
    data1.push_back(std::make_pair(4, 4.1f));
    data1.push_back(std::make_pair(6, 6.1f));
    data2.push_back(std::make_pair(1, 1.1f));
    data2.push_back(std::make_pair(3, 3.1f));
    data2.push_back(std::make_pair(5, 5.1f));
    data2.push_back(std::make_pair(7, 7.1f));

    std::vector<std::pair<int, float> > data3(data1.size() + data2.size());
    std::fill(data3.begin(), data3.end(), std::make_pair(-1, -1.f));

    boost::compute::vector<std::pair<int, float> > v1(data1.size(), context);
    boost::compute::vector<std::pair<int, float> > v2(data2.size(), context);
    boost::compute::vector<std::pair<int, float> > v3(data3.size(), context);

    boost::compute::copy(data1.begin(), data1.end(), v1.begin(), queue);
    boost::compute::copy(data2.begin(), data2.end(), v2.begin(), queue);

    using ::boost::compute::lambda::_1;
    using ::boost::compute::lambda::_2;
    using ::boost::compute::lambda::get;

    boost::compute::merge(
        v1.begin(), v1.end(),
        v2.begin(), v2.end(),
        v3.begin(),
        get<0>(_1) < get<0>(_2),
        queue
    );

    boost::compute::copy(v3.begin(), v3.end(), data3.begin(), queue);

    BOOST_CHECK(v3[0] == std::make_pair(0, 0.1f));
    BOOST_CHECK(v3[1] == std::make_pair(1, 1.1f));
    BOOST_CHECK(v3[2] == std::make_pair(2, 2.1f));
    BOOST_CHECK(v3[3] == std::make_pair(3, 3.1f));
    BOOST_CHECK(v3[4] == std::make_pair(4, 4.1f));
    BOOST_CHECK(v3[5] == std::make_pair(5, 5.1f));
    BOOST_CHECK(v3[6] == std::make_pair(6, 6.1f));
    BOOST_CHECK(v3[7] == std::make_pair(7, 7.1f));
}

BOOST_AUTO_TEST_CASE(merge_floats)
{
    float data1[] = { 1.1f, 2.2f, 3.3f, 4.4f,
                      5.5f, 6.6f, 7.7f, 8.8f };
    float data2[] = { 1.0f, 2.0f, 3.9f, 4.9f,
                      6.8f, 6.9f, 7.0f, 7.1f };

    boost::compute::vector<float> v1(8, context);
    boost::compute::vector<float> v2(8, context);
    boost::compute::vector<float> v3(v1.size() + v2.size(), context);

    boost::compute::copy_n(data1, 8, v1.begin(), queue);
    boost::compute::copy_n(data2, 8, v2.begin(), queue);
    boost::compute::fill(v3.begin(), v3.end(), 0.f, queue);

    boost::compute::merge(
        v1.begin(), v1.end(),
        v2.begin(), v2.end(),
        v3.begin(),
        queue
    );
    CHECK_RANGE_EQUAL(float, 16, v3,
      (1.0f, 1.1f, 2.0f, 2.2f, 3.3f, 3.9f, 4.4f, 4.9f,
       5.5f, 6.6f, 6.8f, 6.9f, 7.0f, 7.1f, 7.7f, 8.8f)
    );
}

BOOST_AUTO_TEST_SUITE_END()
