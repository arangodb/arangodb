//---------------------------------------------------------------------------//
// Copyright (c) 2014 Roshan <thisisroshansmail@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestUniqueCopy
#include <boost/test/unit_test.hpp>

#include <boost/compute/algorithm/unique_copy.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;
namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(unique_copy_int)
{
    int data[] = {1, 6, 6, 4, 2, 2, 4};

    bc::vector<int> input(data, data + 7, queue);
    bc::vector<int> result(5, context);
    
    bc::vector<int>::iterator iter =
        bc::unique_copy(input.begin(), input.end(), result.begin(), queue);
    
    BOOST_VERIFY(iter == result.begin() + 5);
    CHECK_RANGE_EQUAL(int, 5, result, (1, 6, 4, 2, 4));
}

BOOST_AUTO_TEST_SUITE_END()
