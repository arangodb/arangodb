//  Copyright (c) 2020 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

// This is a fuzzing test for waiting and notifying operations.
// The test creates a number of threads exceeding the number of hardware threads, each of which
// blocks on the atomic object. The main thread then notifies one or all threads repeatedly,
// while incrementing the atomic object. The test ends when the atomic counter reaches the predefined limit.
// The goal of the test is to verify that (a) it doesn't crash and (b) all threads get unblocked in the end.

#include <boost/memory_order.hpp>
#include <boost/atomic/atomic.hpp>

#include <iostream>
#include <boost/config.hpp>
#include <boost/bind/bind.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/smart_ptr/scoped_array.hpp>

namespace chrono = boost::chrono;

boost::atomic< unsigned int > g_atomic(0u);

BOOST_CONSTEXPR_OR_CONST unsigned int loop_count = 4096u;

void thread_func(boost::barrier* barrier)
{
    barrier->wait();

    unsigned int old_count = 0u;
    while (true)
    {
        unsigned int new_count = g_atomic.wait(old_count, boost::memory_order_relaxed);
        if (new_count >= loop_count)
            break;

        old_count = new_count;
    }
}

int main()
{
    const unsigned int thread_count = boost::thread::hardware_concurrency() + 4u;
    boost::barrier barrier(thread_count + 1u);
    boost::scoped_array< boost::thread > threads(new boost::thread[thread_count]);

    for (unsigned int i = 0u; i < thread_count; ++i)
        boost::thread(boost::bind(&thread_func, &barrier)).swap(threads[i]);

    barrier.wait();

    // Let the threads block on the atomic counter
    boost::this_thread::sleep_for(chrono::milliseconds(100));

    while (true)
    {
        for (unsigned int i = 0u; i < thread_count; ++i)
        {
            g_atomic.opaque_add(1u, boost::memory_order_relaxed);
            g_atomic.notify_one();
        }

        unsigned int old_count = g_atomic.fetch_add(1u, boost::memory_order_relaxed);
        g_atomic.notify_all();

        if ((old_count + 1u) >= loop_count)
            break;
    }

    for (unsigned int i = 0u; i < thread_count; ++i)
        threads[i].join();

    return 0u;
}
