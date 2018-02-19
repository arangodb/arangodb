//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestGather
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/copy_if.hpp>
#include <boost/compute/algorithm/gather.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(gather_int)
{
    int input_data[] = { 1, 2, 3, 4, 5 };
    compute::vector<int> input(input_data, input_data + 5, queue);

    int indices_data[] = { 0, 4, 1, 3, 2 };
    compute::vector<int> indices(indices_data, indices_data + 5, queue);

    compute::vector<int> output(5, context);
    compute::gather(
        indices.begin(), indices.end(), input.begin(), output.begin(), queue
    );
    CHECK_RANGE_EQUAL(int, 5, output, (1, 5, 2, 4, 3));

    compute::gather(
        indices.begin() + 1, indices.end(), input.begin(), output.begin(), queue
    );
    CHECK_RANGE_EQUAL(int, 5, output, (5, 2, 4, 3, 3));
}

BOOST_AUTO_TEST_CASE(copy_index_then_gather)
{
    // input data
    int data[] = { 1, 4, 3, 2, 5, 9, 8, 7 };
    compute::vector<int> input(data, data + 8, queue);

    // function returning true if the input is odd
    BOOST_COMPUTE_FUNCTION(bool, is_odd, (int x),
    {
        return x % 2 != 0;
    });

    // copy indices of all odd values
    compute::vector<int> odds(5, context);
    compute::detail::copy_index_if(
        input.begin(), input.end(), odds.begin(), is_odd, queue
    );
    CHECK_RANGE_EQUAL(int, 5, odds, (0, 2, 4, 5, 7));

    // gather all odd values
    compute::vector<int> odd_values(5, context);
    compute::gather(
        odds.begin(), odds.end(), input.begin(), odd_values.begin(), queue
    );
    CHECK_RANGE_EQUAL(int, 5, odd_values, (1, 3, 5, 9, 7));
}

BOOST_AUTO_TEST_SUITE_END()
