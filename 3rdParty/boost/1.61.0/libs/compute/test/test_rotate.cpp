//---------------------------------------------------------------------------//
// Copyright (c) 2014 Roshan <thisisroshansmail@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestRotate
#include <boost/test/unit_test.hpp>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/copy_n.hpp>
#include <boost/compute/algorithm/rotate.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(rotate_trivial)
{
    int data[] = {1, 4, 2, 6, 3, 2, 5, 3, 4, 6};
    boost::compute::vector<int> vector(10, context);

    boost::compute::copy_n(data, 10, vector.begin(), queue);

    boost::compute::rotate(vector.begin(), vector.begin(), vector.end());
    CHECK_RANGE_EQUAL(int, 10, vector, (1, 4, 2, 6, 3, 2, 5, 3, 4, 6));

    boost::compute::rotate(vector.begin(), vector.end(), vector.end());
    CHECK_RANGE_EQUAL(int, 10, vector, (1, 4, 2, 6, 3, 2, 5, 3, 4, 6));
}

BOOST_AUTO_TEST_CASE(rotate_1)
{
    int data[] = {1, 4, 2, 6, 3, 2, 5, 3, 4, 6};
    boost::compute::vector<int> vector(10, context);

    boost::compute::copy_n(data, 10, vector.begin(), queue);

    boost::compute::rotate(vector.begin(), vector.begin()+1, vector.end());
    CHECK_RANGE_EQUAL(int, 10, vector, (4, 2, 6, 3, 2, 5, 3, 4, 6, 1));
}

BOOST_AUTO_TEST_CASE(rotate_4)
{
    int data[] = {1, 4, 2, 6, 3, 2, 5, 3, 4, 6};
    boost::compute::vector<int> vector(10, context);

    boost::compute::copy_n(data, 10, vector.begin(), queue);

    boost::compute::rotate(vector.begin(), vector.begin()+4, vector.end());
    CHECK_RANGE_EQUAL(int, 10, vector, (3, 2, 5, 3, 4, 6, 1, 4, 2, 6));
}

BOOST_AUTO_TEST_CASE(rotate_9)
{
    int data[] = {1, 4, 2, 6, 3, 2, 5, 3, 4, 6};
    boost::compute::vector<int> vector(10, context);

    boost::compute::copy_n(data, 10, vector.begin(), queue);

    boost::compute::rotate(vector.begin(), vector.begin()+9, vector.end());
    CHECK_RANGE_EQUAL(int, 10, vector, (6, 1, 4, 2, 6, 3, 2, 5, 3, 4));
}

BOOST_AUTO_TEST_SUITE_END()
