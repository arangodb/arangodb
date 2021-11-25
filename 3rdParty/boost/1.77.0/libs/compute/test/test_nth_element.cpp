//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestNthElement
#include <boost/test/unit_test.hpp>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/copy_n.hpp>
#include <boost/compute/algorithm/is_partitioned.hpp>
#include <boost/compute/algorithm/nth_element.hpp>
#include <boost/compute/algorithm/partition_point.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(nth_element_int)
{
    int data[] = { 9, 15, 1, 4, 9, 9, 4, 15, 12, 1 };
    boost::compute::vector<int> vector(10, context);

    boost::compute::copy_n(data, 10, vector.begin(), queue);

    boost::compute::nth_element(
        vector.begin(), vector.begin() + 5, vector.end(), queue
    );

    BOOST_CHECK_EQUAL(vector[5], 9);
    BOOST_VERIFY(boost::compute::is_partitioned(
        vector.begin(), vector.end(), boost::compute::_1 <= 9, queue
    ));
    BOOST_VERIFY(boost::compute::partition_point(
        vector.begin(), vector.end(), boost::compute::_1 <= 9, queue
    ) > vector.begin() + 5);

    boost::compute::copy_n(data, 10, vector.begin(), queue);

    boost::compute::nth_element(
        vector.begin(), vector.end(), vector.end(), queue
    );
    CHECK_RANGE_EQUAL(int, 10, vector, (9, 15, 1, 4, 9, 9, 4, 15, 12, 1));
}

BOOST_AUTO_TEST_CASE(nth_element_median)
{
    int data[] = { 5, 6, 4, 3, 2, 6, 7, 9, 3 };
    boost::compute::vector<int> v(9, context);
    boost::compute::copy_n(data, 9, v.begin(), queue);

    boost::compute::nth_element(v.begin(), v.begin() + 4, v.end(), queue);

    BOOST_CHECK_EQUAL(v[4], 5);
    BOOST_VERIFY(boost::compute::is_partitioned(
        v.begin(), v.end(), boost::compute::_1 <= 5, queue
    ));
    BOOST_VERIFY(boost::compute::partition_point(
        v.begin(), v.end(), boost::compute::_1 <= 5, queue
    ) > v.begin() + 4);
}

BOOST_AUTO_TEST_CASE(nth_element_second_largest)
{
    int data[] = { 5, 6, 4, 3, 2, 6, 7, 9, 3 };
    boost::compute::vector<int> v(9, context);
    boost::compute::copy_n(data, 9, v.begin(), queue);

    boost::compute::nth_element(v.begin(), v.begin() + 1, v.end(), queue);

    BOOST_CHECK_EQUAL(v[1], 3);
    BOOST_VERIFY(boost::compute::is_partitioned(
        v.begin(), v.end(), boost::compute::_1 <= 3, queue
    ));
    BOOST_VERIFY(boost::compute::partition_point(
        v.begin(), v.end(), boost::compute::_1 <= 3, queue
    ) > v.begin() + 1);
}

BOOST_AUTO_TEST_CASE(nth_element_comparator)
{
    int data[] = { 9, 15, 1, 4, 9, 9, 4, 15, 12, 1 };
    boost::compute::vector<int> vector(10, context);

    boost::compute::less<int> less_than;

    boost::compute::copy_n(data, 10, vector.begin(), queue);

    boost::compute::nth_element(
        vector.begin(), vector.begin() + 5, vector.end(), less_than, queue
    );
    BOOST_CHECK_EQUAL(vector[5], 9);
    BOOST_VERIFY(boost::compute::is_partitioned(
        vector.begin(), vector.end(), boost::compute::_1 <= 9, queue
    ));
    BOOST_VERIFY(boost::compute::partition_point(
        vector.begin(), vector.end(), boost::compute::_1 <= 9, queue
    ) > vector.begin() + 5);

    boost::compute::copy_n(data, 10, vector.begin(), queue);

    boost::compute::nth_element(
        vector.begin(), vector.end(), vector.end(), less_than, queue
    );
    CHECK_RANGE_EQUAL(int, 10, vector, (9, 15, 1, 4, 9, 9, 4, 15, 12, 1));
}

BOOST_AUTO_TEST_SUITE_END()
