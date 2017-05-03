//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestTabulate
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/function.hpp>
#include <boost/compute/algorithm/copy_n.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/experimental/tabulate.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(tabulate_negative_int)
{
    BOOST_COMPUTE_FUNCTION(int, negate, (int x),
    {
        return -x;
    });

    compute::vector<int> vector(10, context);
    compute::experimental::tabulate(vector.begin(), vector.end(), negate, queue);
    CHECK_RANGE_EQUAL(int, 10, vector, (0, -1, -2, -3, -4, -5, -6, -7, -8, -9));
}

BOOST_AUTO_TEST_SUITE_END()
