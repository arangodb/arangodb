//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestEvent
#include <boost/test/unit_test.hpp>

#include <vector>

#ifdef BOOST_COMPUTE_USE_CPP11
#include <mutex>
#include <future>
#endif // BOOST_COMPUTE_USE_CPP11

#include <boost/compute/event.hpp>

#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(null_event)
{
    boost::compute::event null;
    BOOST_CHECK(null.get() == cl_event());
}

#if defined(BOOST_COMPUTE_CL_VERSION_1_1) && defined(BOOST_COMPUTE_USE_CPP11)
std::mutex callback_mutex;
std::condition_variable callback_condition_variable;
static bool callback_invoked = false;

static void BOOST_COMPUTE_CL_CALLBACK
callback(cl_event event, cl_int status, void *user_data)
{
    std::lock_guard<std::mutex> lock(callback_mutex);
    callback_invoked = true;
    callback_condition_variable.notify_one();
}

BOOST_AUTO_TEST_CASE(event_callback)
{
    REQUIRES_OPENCL_VERSION(1,2);

    // ensure callback has not yet been executed
    BOOST_CHECK_EQUAL(callback_invoked, false);

    // enqueue marker and set callback to be invoked
    boost::compute::event marker = queue.enqueue_marker();
    marker.set_callback(callback);
    marker.wait();

    // wait up to one second for the callback to be executed
    std::unique_lock<std::mutex> lock(callback_mutex);
    callback_condition_variable.wait_for(
        lock, std::chrono::seconds(1), [&](){ return callback_invoked; }
    );

    // ensure callback has been executed
    BOOST_CHECK_EQUAL(callback_invoked, true);
}

BOOST_AUTO_TEST_CASE(lambda_callback)
{
    REQUIRES_OPENCL_VERSION(1,2);

    bool lambda_invoked = false;

    boost::compute::event marker = queue.enqueue_marker();
    marker.set_callback([&](){
        std::lock_guard<std::mutex> lock(callback_mutex);
        lambda_invoked = true;
        callback_condition_variable.notify_one();
    });
    marker.wait();

    // wait up to one second for the callback to be executed
    std::unique_lock<std::mutex> lock(callback_mutex);
    callback_condition_variable.wait_for(
        lock, std::chrono::seconds(1), [&](){ return lambda_invoked; }
    );
    BOOST_CHECK_EQUAL(lambda_invoked, true);
}

void BOOST_COMPUTE_CL_CALLBACK
event_promise_fulfiller_callback(cl_event event, cl_int status, void *user_data)
{
    auto *promise = static_cast<std::promise<void> *>(user_data);
    promise->set_value();
    delete promise;
}

BOOST_AUTO_TEST_CASE(event_to_std_future)
{
    REQUIRES_OPENCL_VERSION(1,2);

    // enqueue an asynchronous copy to the device
    std::vector<float> vector(1000, 3.14f);
    boost::compute::buffer buffer(context, 1000 * sizeof(float));
    auto event = queue.enqueue_write_buffer_async(
        buffer, 0, 1000 * sizeof(float), vector.data()
    );

    // create a promise and future to be set by the callback
    auto *promise = new std::promise<void>;
    std::future<void> future = promise->get_future();
    event.set_callback(event_promise_fulfiller_callback, CL_COMPLETE, promise);

    // ensure commands are submitted to the device before waiting
    queue.flush();

    // wait for future to become ready
    future.wait();
}
#endif // BOOST_COMPUTE_CL_VERSION_1_1

BOOST_AUTO_TEST_SUITE_END()
