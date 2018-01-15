//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestFunctionalAs
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/copy_n.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/functional/as.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(roundtrip_int_float)
{
    int data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    compute::vector<int> input(8, context);
    compute::copy_n(data, 8, input.begin(), queue);

    // convert int -> float
    compute::vector<float> output(8, context);
    compute::transform(
        input.begin(),
        input.end(),
        output.begin(),
        compute::as<float>(),
        queue
    );

    // zero out input
    compute::fill(input.begin(), input.end(), 0, queue);

    // convert float -> int
    compute::transform(
        output.begin(),
        output.end(),
        input.begin(),
        compute::as<int>(),
        queue
    );

    // check values
    CHECK_RANGE_EQUAL(
        int, 8, input, (1, 2, 3, 4, 5, 6, 7, 8)
    );
}

BOOST_AUTO_TEST_SUITE_END()
