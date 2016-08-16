//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestForEach
#include <boost/test/unit_test.hpp>

#include <boost/compute/function.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/algorithm/for_each.hpp>
#include <boost/compute/algorithm/for_each_n.hpp>
#include <boost/compute/container/vector.hpp>

#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(for_each_nop)
{
    bc::vector<int> vector(4, context);
    bc::iota(vector.begin(), vector.end(), 0);

    BOOST_COMPUTE_FUNCTION(void, nop, (int ignored), {});

    bc::for_each(vector.begin(), vector.end(), nop, queue);
}

BOOST_AUTO_TEST_CASE(for_each_n_nop)
{
    bc::vector<int> vector(4, context);
    bc::iota(vector.begin(), vector.end(), 0);

    BOOST_COMPUTE_FUNCTION(void, nop, (int ignored), {});

    bc::for_each_n(vector.begin(), vector.size(), nop, queue);
}

BOOST_AUTO_TEST_SUITE_END()
