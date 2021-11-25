//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestRemove
#include <boost/test/unit_test.hpp>

#include <boost/compute/algorithm/remove.hpp>
#include <boost/compute/algorithm/remove_if.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(remove_int)
{
    int data[] = { 1, 2, 1, 3, 2, 4, 3, 4, 5 };
    bc::vector<int> vector(data, data + 9, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(9));

    // remove 2's
    bc::vector<int>::const_iterator iter =
        bc::remove(vector.begin(), vector.end(), 2, queue);
    BOOST_VERIFY(iter == vector.begin() + 7);
    CHECK_RANGE_EQUAL(int, 7, vector, (1, 1, 3, 4, 3, 4, 5));

    // remove 4's
    iter = bc::remove(vector.begin(), vector.begin() + 7, 4, queue);
    BOOST_VERIFY(iter == vector.begin() + 5);
    CHECK_RANGE_EQUAL(int, 5, vector, (1, 1, 3, 3, 5));

    // remove 1's
    iter = bc::remove(vector.begin(), vector.begin() + 5, 1, queue);
    BOOST_VERIFY(iter == vector.begin() + 3);
    CHECK_RANGE_EQUAL(int, 3, vector, (3, 3, 5));

    // remove 5's
    iter = bc::remove(vector.begin(), vector.begin() + 3, 5, queue);
    BOOST_VERIFY(iter == vector.begin() + 2);
    CHECK_RANGE_EQUAL(int, 2, vector, (3, 3));

    // remove 3's
    iter = bc::remove(vector.begin(), vector.begin() + 2, 3, queue);
    BOOST_VERIFY(iter == vector.begin());
}

BOOST_AUTO_TEST_SUITE_END()
