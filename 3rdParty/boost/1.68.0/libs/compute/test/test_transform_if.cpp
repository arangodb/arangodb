//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestTransformIf
#include <boost/test/unit_test.hpp>

#include <boost/compute/lambda.hpp>
#include <boost/compute/algorithm/transform_if.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(transform_if_odd)
{
    using boost::compute::abs;
    using boost::compute::lambda::_1;

    int data[] = { -2, -3, -4, -5, -6, -7, -8, -9 };
    compute::vector<int> input(data, data + 8, queue);
    compute::vector<int> output(input.size(), context);

    compute::vector<int>::iterator end = compute::transform_if(
        input.begin(), input.end(), output.begin(), abs<int>(), _1 % 2 != 0, queue
    );
    BOOST_CHECK_EQUAL(std::distance(output.begin(), end), 4);

    CHECK_RANGE_EQUAL(int, 4, output, (+3, +5, +7, +9));
}

BOOST_AUTO_TEST_SUITE_END()
