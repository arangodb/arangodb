//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestAsyncWait
#include <boost/test/unit_test.hpp>

#include <boost/compute/async/future.hpp>
#include <boost/compute/async/wait.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(empty)
{
}

#ifndef BOOST_COMPUTE_NO_VARIADIC_TEMPLATES
BOOST_AUTO_TEST_CASE(wait_for_copy)
{
    // wait list
    compute::wait_list events;

    // create host data array
    int data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

    // create vector on the device
    compute::vector<int> vector(8, context);

    // fill vector with 9's
    compute::future<void> fill_future =
        compute::fill_async(vector.begin(), vector.end(), 9, queue);

    // wait for fill() to complete
    compute::wait_for_all(fill_future);

    // check data on the device
    CHECK_RANGE_EQUAL(int, 8, vector, (9, 9, 9, 9, 9, 9, 9, 9));

    // copy each pair of values independently and asynchronously
    compute::event copy1 = queue.enqueue_write_buffer_async(
        vector.get_buffer(), 0 * sizeof(int), 2 * sizeof(int), data + 0
    );
    compute::event copy2 = queue.enqueue_write_buffer_async(
        vector.get_buffer(), 2 * sizeof(int), 2 * sizeof(int), data + 2
    );
    compute::event copy3 = queue.enqueue_write_buffer_async(
        vector.get_buffer(), 4 * sizeof(int), 2 * sizeof(int), data + 4
    );
    compute::event copy4 = queue.enqueue_write_buffer_async(
        vector.get_buffer(), 6 * sizeof(int), 2 * sizeof(int), data + 6
    );

    // wait for all copies to complete
    compute::wait_for_all(copy1, copy2, copy3, copy4);

    // check data on the device
    CHECK_RANGE_EQUAL(int, 8, vector, (1, 2, 3, 4, 5, 6, 7, 8));
}
#endif // BOOST_COMPUTE_NO_VARIADIC_TEMPLATES

BOOST_AUTO_TEST_SUITE_END()
