//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestPartialSum
#include <boost/test/unit_test.hpp>

#include <vector>
#include <numeric>

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/partial_sum.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(partial_sum_int)
{
    int data[] = { 1, 2, 5, 3, 9, 1, 4, 2 };
    bc::vector<int> a(8, context);
    bc::copy(data, data + 8, a.begin(), queue);

    bc::vector<int> b(a.size(), context);
    bc::vector<int>::iterator iter =
        bc::partial_sum(a.begin(), a.end(), b.begin(), queue);
    BOOST_CHECK(iter == b.end());
    CHECK_RANGE_EQUAL(int, 8, b, (1, 3, 8, 11, 20, 21, 25, 27));
}

BOOST_AUTO_TEST_SUITE_END()
