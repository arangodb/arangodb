//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestReverse
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/algorithm/reverse.hpp>
#include <boost/compute/algorithm/reverse_copy.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/counting_iterator.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(reverse_int)
{
    bc::vector<int> vector(5, context);
    bc::iota(vector.begin(), vector.end(), 0, queue);
    CHECK_RANGE_EQUAL(int, 5, vector, (0, 1, 2, 3, 4));

    bc::reverse(vector.begin(), vector.end(), queue);
    CHECK_RANGE_EQUAL(int, 5, vector, (4, 3, 2, 1, 0));

    bc::reverse(vector.begin() + 1, vector.end(), queue);
    CHECK_RANGE_EQUAL(int, 5, vector, (4, 0, 1, 2, 3));

    bc::reverse(vector.begin() + 1, vector.end() - 1, queue);
    CHECK_RANGE_EQUAL(int, 5, vector, (4, 2, 1, 0, 3));

    bc::reverse(vector.begin(), vector.end() - 2, queue);
    CHECK_RANGE_EQUAL(int, 5, vector, (1, 2, 4, 0, 3));

    vector.resize(6, queue);
    bc::iota(vector.begin(), vector.end(), 10, queue);
    CHECK_RANGE_EQUAL(int, 6, vector, (10, 11, 12, 13, 14, 15));

    bc::reverse(vector.begin(), vector.end(), queue);
    CHECK_RANGE_EQUAL(int, 6, vector, (15, 14, 13, 12, 11, 10));

    bc::reverse(vector.begin() + 3, vector.end(), queue);
    CHECK_RANGE_EQUAL(int, 6, vector, (15, 14, 13, 10, 11, 12));

    bc::reverse(vector.begin() + 1, vector.end() - 2, queue);
    CHECK_RANGE_EQUAL(int, 6, vector, (15, 10, 13, 14, 11, 12));
}

BOOST_AUTO_TEST_CASE(reverse_copy_int)
{
    bc::vector<int> a(5, context);
    bc::iota(a.begin(), a.end(), 0, queue);
    CHECK_RANGE_EQUAL(int, 5, a, (0, 1, 2, 3, 4));

    bc::vector<int> b(5, context);
    bc::vector<int>::iterator iter =
        bc::reverse_copy(a.begin(), a.end(), b.begin(), queue);
    BOOST_CHECK(iter == b.end());
    CHECK_RANGE_EQUAL(int, 5, b, (4, 3, 2, 1, 0));

    iter = bc::reverse_copy(b.begin() + 1, b.end(), a.begin() + 1, queue);
    BOOST_CHECK(iter == a.end());
    CHECK_RANGE_EQUAL(int, 5, a, (0, 0, 1, 2, 3));

    iter = bc::reverse_copy(a.begin(), a.end() - 1, b.begin(), queue);
    BOOST_CHECK(iter == (b.end() - 1));
    CHECK_RANGE_EQUAL(int, 5, b, (2, 1, 0, 0, 0));
}

BOOST_AUTO_TEST_CASE(reverse_copy_counting_iterator)
{
    bc::vector<int> vector(5, context);
    bc::reverse_copy(
        bc::make_counting_iterator(1),
        bc::make_counting_iterator(6),
        vector.begin(),
        queue
    );
    CHECK_RANGE_EQUAL(int, 5, vector, (5, 4, 3, 2, 1));
}

BOOST_AUTO_TEST_SUITE_END()
