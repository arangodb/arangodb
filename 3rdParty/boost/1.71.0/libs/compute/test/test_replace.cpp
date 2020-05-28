//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestReplace
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/algorithm/replace.hpp>
#include <boost/compute/algorithm/replace_copy.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(replace_int)
{
    bc::vector<int> vector(5, context);
    bc::iota(vector.begin(), vector.end(), 0, queue);
    CHECK_RANGE_EQUAL(int, 5, vector, (0, 1, 2, 3, 4));

    bc::replace(vector.begin(), vector.end(), 2, 6, queue);
    CHECK_RANGE_EQUAL(int, 5, vector, (0, 1, 6, 3, 4));
}

BOOST_AUTO_TEST_CASE(replace_copy_int)
{
    bc::vector<int> a(5, context);
    bc::iota(a.begin(), a.end(), 0, queue);
    CHECK_RANGE_EQUAL(int, 5, a, (0, 1, 2, 3, 4));

    bc::vector<int> b(5, context);
    bc::vector<int>::iterator iter =
        bc::replace_copy(a.begin(), a.end(), b.begin(), 3, 9, queue);
    BOOST_CHECK(iter == b.end());
    CHECK_RANGE_EQUAL(int, 5, b, (0, 1, 2, 9, 4));

    // ensure 'a' was not modified
    CHECK_RANGE_EQUAL(int, 5, a, (0, 1, 2, 3, 4));
}

BOOST_AUTO_TEST_SUITE_END()
