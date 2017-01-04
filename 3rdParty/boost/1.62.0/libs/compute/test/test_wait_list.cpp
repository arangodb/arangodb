//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestWaitList
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <vector>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/async/future.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/utility/wait_list.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(create_wait_list)
{
    compute::wait_list events;
    BOOST_CHECK_EQUAL(events.size(), size_t(0));
    BOOST_CHECK_EQUAL(events.empty(), true);
    BOOST_CHECK(events.get_event_ptr() == 0);
}

#ifndef BOOST_COMPUTE_NO_HDR_INITIALIZER_LIST
BOOST_AUTO_TEST_CASE(create_wait_list_from_initializer_list)
{
    compute::event event0;
    compute::event event1;
    compute::event event2;
    compute::wait_list events = { event0, event1, event2 };
    BOOST_CHECK_EQUAL(events.size(), size_t(3));
    CHECK_RANGE_EQUAL(compute::event, 3, events, (event0, event1, event2));
}
#endif // BOOST_COMPUTE_NO_HDR_INITIALIZER_LIST

BOOST_AUTO_TEST_CASE(insert_future)
{
    // create vector on the host
    std::vector<int> host_vector(4);
    std::fill(host_vector.begin(), host_vector.end(), 7);

    // create vector on the device
    compute::vector<int> device_vector(4, context);

    // create wait list
    compute::wait_list events;

    // copy values to device
    compute::future<void> future = compute::copy_async(
        host_vector.begin(), host_vector.end(), device_vector.begin(), queue
    );

    // add future event to the wait list
    events.insert(future);
    BOOST_CHECK_EQUAL(events.size(), size_t(1));
    BOOST_CHECK(events.get_event_ptr() != 0);

    // wait for copy to complete
    events.wait();

    // check values
    CHECK_RANGE_EQUAL(int, 4, device_vector, (7, 7, 7, 7));

    // clear the event list
    events.clear();
    BOOST_CHECK_EQUAL(events.size(), size_t(0));
}

BOOST_AUTO_TEST_SUITE_END()
