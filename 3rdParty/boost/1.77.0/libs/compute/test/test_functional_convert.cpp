//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestFunctionalConvert
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/copy_n.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/functional/convert.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(convert_int_float)
{
    int data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    compute::vector<int> input(8, context);
    compute::copy_n(data, 8, input.begin(), queue);

    compute::vector<float> output(8, context);
    compute::transform(
        input.begin(),
        input.end(),
        output.begin(),
        compute::convert<float>(),
        queue
    );
    CHECK_RANGE_EQUAL(
        float, 8, output, (1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f)
    );
}

BOOST_AUTO_TEST_SUITE_END()
