//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestClampRange
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/experimental/clamp_range.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(clamp_float_range)
{
    float data[] = { 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f };
    compute::vector<float> input(data, data + 8, queue);

    compute::vector<float> result(8, context);
    compute::experimental::clamp_range(
        input.begin(),
        input.end(),
        result.begin(),
        3.f, // low
        6.f, // high
        queue
    );
    CHECK_RANGE_EQUAL(
        float, 8, result,
        (3.f, 3.f, 3.f, 4.f, 5.f, 6.f, 6.f, 6.f)
    );
}

BOOST_AUTO_TEST_SUITE_END()
