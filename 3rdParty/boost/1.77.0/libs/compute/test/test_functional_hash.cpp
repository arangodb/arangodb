//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestFunctionalHash
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/copy_n.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/functional/hash.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(hash_int)
{
    using compute::ulong_;

    int data[] = { 1, 2, 3, 4 };
    compute::vector<int> input_values(data, data + 4, queue);
    compute::vector<ulong_> hash_values(4, context);

    compute::transform(
        input_values.begin(),
        input_values.end(),
        hash_values.begin(),
        compute::hash<int>(),
        queue
    );
}

BOOST_AUTO_TEST_SUITE_END()
