//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestFunctionalIdentity
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/functional/identity.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(copy_with_identity_transform)
{
    int data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    compute::vector<int> input(data, data + 8, queue);
    compute::vector<int> output(8, context);

    compute::transform(
        input.begin(), input.end(), output.begin(), compute::identity<int>(), queue
    );

    CHECK_RANGE_EQUAL(
        int, 8, output, (1, 2, 3, 4, 5, 6, 7, 8)
    );
}

BOOST_AUTO_TEST_SUITE_END()
