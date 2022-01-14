//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestAdjacentDifference
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/lambda.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/fill.hpp>
#include <boost/compute/algorithm/all_of.hpp>
#include <boost/compute/algorithm/adjacent_difference.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(adjacent_difference_int)
{
    using compute::int_;

    compute::vector<int_> a(5, context);
    compute::iota(a.begin(), a.end(), 0, queue);
    CHECK_RANGE_EQUAL(int_, 5, a, (0, 1, 2, 3, 4));

    compute::vector<int_> b(5, context);
    compute::vector<int_>::iterator iter =
        compute::adjacent_difference(a.begin(), a.end(), b.begin(), queue);
    BOOST_CHECK(iter == b.end());
    CHECK_RANGE_EQUAL(int_, 5, b, (0, 1, 1, 1, 1));

    int_ data[] = { 1, 9, 36, 48, 81 };
    compute::copy(data, data + 5, a.begin(), queue);
    CHECK_RANGE_EQUAL(int_, 5, a, (1, 9, 36, 48, 81));

    iter = compute::adjacent_difference(a.begin(), a.end(), b.begin(), queue);
    BOOST_CHECK(iter == b.end());
    CHECK_RANGE_EQUAL(int_, 5, b, (1, 8, 27, 12, 33));
}

BOOST_AUTO_TEST_CASE(adjacent_difference_first_eq_last)
{
    using compute::int_;

    compute::vector<int_> a(size_t(5), int_(1), queue);
    compute::vector<int_> b(size_t(5), int_(0), queue);
    compute::vector<int_>::iterator iter =
        compute::adjacent_difference(a.begin(), a.begin(), b.begin(), queue);
    BOOST_CHECK(iter == b.begin());
    CHECK_RANGE_EQUAL(int_, 5, b, (0, 0, 0, 0, 0));
}

BOOST_AUTO_TEST_CASE(adjacent_difference_first_eq_result)
{
    using compute::int_;

    compute::vector<int_> a(5, context);
    compute::iota(a.begin(), a.end(), 0, queue);
    CHECK_RANGE_EQUAL(int_, 5, a, (0, 1, 2, 3, 4));

    compute::vector<int_>::iterator iter =
        compute::adjacent_difference(a.begin(), a.end(), a.begin(), queue);
    BOOST_CHECK(iter == a.end());
    CHECK_RANGE_EQUAL(int_, 5, a, (0, 1, 1, 1, 1));
}

BOOST_AUTO_TEST_CASE(all_same)
{
    using compute::int_;

    compute::vector<int_> input(1000, context);
    compute::fill(input.begin(), input.end(), 42, queue);

    compute::vector<int_> output(input.size(), context);

    compute::adjacent_difference(
        input.begin(), input.end(), output.begin(), queue
    );

    int_ first;
    compute::copy_n(output.begin(), 1, &first, queue);
    BOOST_CHECK_EQUAL(first, 42);

    using compute::lambda::_1;

    BOOST_CHECK(
        compute::all_of(output.begin() + 1, output.end(), _1 == 0, queue)
    );
}

BOOST_AUTO_TEST_SUITE_END()
