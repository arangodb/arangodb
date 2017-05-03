//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestRandomFill
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/count_if.hpp>
#include <boost/compute/algorithm/detail/random_fill.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/lambda.hpp>

#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(random_fill_float)
{
    using compute::lambda::_1;

    compute::vector<float> vector(10, context);

    // fill with values between 0 and 1
    compute::detail::random_fill(
        vector.begin(),
        vector.end(),
        0.0f,
        1.0f,
        queue
    );

    BOOST_CHECK_EQUAL(
        compute::count_if(
            vector.begin(), vector.end(), _1 < 0.0f || _1 > 1.0f, queue
        ),
        size_t(0)
    );

    // fill with values between 5 and 10
    compute::detail::random_fill(
        vector.begin(),
        vector.end(),
        5.0f,
        10.0f,
        queue
    );

    BOOST_CHECK_EQUAL(
        compute::count_if(
            vector.begin(), vector.end(), _1 < 5.0f || _1 > 10.0f, queue
        ),
        size_t(0)
    );

    // fill with values between -25 and 25
    compute::detail::random_fill(
        vector.begin(), vector.end(), -25.0f, 25.0f, queue
    );

    BOOST_CHECK_EQUAL(
        compute::count_if(
            vector.begin(), vector.end(), _1 < -25.0f || _1 > 25.0f, queue
        ),
        size_t(0)
    );
}

BOOST_AUTO_TEST_SUITE_END()
