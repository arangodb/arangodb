//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestGenerate
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <boost/compute/function.hpp>
#include <boost/compute/algorithm/generate.hpp>
#include <boost/compute/algorithm/generate_n.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/types/pair.hpp>

#include "quirks.hpp"
#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(generate4)
{
    bc::vector<int> vector(4, context);
    bc::fill(vector.begin(), vector.end(), 2, queue);
    CHECK_RANGE_EQUAL(int, 4, vector, (2, 2, 2, 2));

    BOOST_COMPUTE_FUNCTION(int, ret4, (void),
    {
        return 4;
    });

    bc::generate(vector.begin(), vector.end(), ret4, queue);
    CHECK_RANGE_EQUAL(int, 4, vector, (4, 4, 4, 4));
}

BOOST_AUTO_TEST_CASE(generate_pair)
{
    if(bug_in_struct_assignment(device)){
        std::cerr << "skipping generate_pair test" << std::endl;
        return;
    }

    // in order to use std::pair<T1, T2> with BOOST_COMPUTE_FUNCTION() macro we
    // need a typedef. otherwise the commas in the type declaration screw it up.
    typedef std::pair<int, float> pair_type;

    bc::vector<pair_type> vector(3, context);

    BOOST_COMPUTE_FUNCTION(pair_type, generate_pair, (void),
    {
        return boost_make_pair(int, 2, float, 3.14f);
    });

    bc::generate(vector.begin(), vector.end(), generate_pair, queue);

    // check results
    std::vector<pair_type> host_vector(3);
    bc::copy(vector.begin(), vector.end(), host_vector.begin(), queue);
    BOOST_CHECK(host_vector[0] == std::make_pair(2, 3.14f));
    BOOST_CHECK(host_vector[1] == std::make_pair(2, 3.14f));
    BOOST_CHECK(host_vector[2] == std::make_pair(2, 3.14f));
}

BOOST_AUTO_TEST_SUITE_END()
