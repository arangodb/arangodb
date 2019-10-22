//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestInplaceMerge
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/inplace_merge.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(simple_merge_int)
{
    int data[] = { 1, 3, 5, 7, 2, 4, 6, 8 };
    compute::vector<int> vector(data, data + 8, queue);

    // merge each half in-place
    compute::inplace_merge(
        vector.begin(),
        vector.begin() + 4,
        vector.end(),
        queue
    );
    CHECK_RANGE_EQUAL(int, 8, vector, (1, 2, 3, 4, 5, 6, 7, 8));

    // run again on already sorted list
    compute::inplace_merge(
        vector.begin(),
        vector.begin() + 4,
        vector.end(),
        queue
    );
    CHECK_RANGE_EQUAL(int, 8, vector, (1, 2, 3, 4, 5, 6, 7, 8));
}

BOOST_AUTO_TEST_SUITE_END()
