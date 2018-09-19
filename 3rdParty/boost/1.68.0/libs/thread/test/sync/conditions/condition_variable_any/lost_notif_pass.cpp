//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Copyright (C) 2017 Austin J. Beer
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/condition_variable>

// class condition_variable;

// condition_variable(const condition_variable&) = delete;

#include <iostream>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <cassert>

// Summary of each test:
// 1. Start the test thread and wait for it to start up.
//    The test thread waits for the flag to be set using a large timeout.
// 2. The main thread takes the lock and then sleeps for a long time while holding
//    the lock before setting the flag and calling notify_one(). If the wait
//    function being tested is polling pthread_cond_timedwait() internally, any
//    notifications sent after pthread_cond_timedwait() times out but before it can
//    reacquire the lock may be "lost". pthread_cond_timedwait() will report that
//    it timed out and the wait function may incorrectly assume that no
//    notification was received. This test ensures that that doesn't happen.
// 3. Measure how it takes the test thread to return. If it received the
//    notification, it will return fairly quickly. If it missed the notification,
//    the test thread won't return until the wait function being tested times out.

//------------------------------------------------------------------------------

boost::condition_variable_any cv;
boost::mutex mut;

bool flag;
bool waiting;

bool flagIsSet()
{
    return flag;
}

bool threadIsWaiting()
{
    return waiting;
}

//------------------------------------------------------------------------------

#ifdef BOOST_THREAD_USES_DATETIME

boost::posix_time::milliseconds posix_wait_time(1000);

template <typename F>
void test_posix_wait_function(F f)
{
    flag = false;
    waiting = false;
    boost::thread t(f);
    while (!threadIsWaiting())
    {
        boost::this_thread::sleep(boost::posix_time::milliseconds(1));
    }

    boost::unique_lock<boost::mutex> lk(mut);
    boost::this_thread::sleep(boost::posix_time::milliseconds(500));
    boost::posix_time::ptime t0 = boost::posix_time::microsec_clock::universal_time();
    flag = true;
    cv.notify_one();
    lk.unlock();
    t.join();
    boost::posix_time::ptime t1 = boost::posix_time::microsec_clock::universal_time();

    BOOST_TEST(t1 - t0 < boost::posix_time::milliseconds(250));
}

//------------------------------------------------------------------------------

void timed_wait_absolute_without_pred()
{
    boost::unique_lock<boost::mutex> lk(mut);
    waiting = true;
    while (!flagIsSet())
    {
        cv.timed_wait(lk, boost::posix_time::microsec_clock::universal_time() + posix_wait_time);
    }
}

void timed_wait_absolute_with_pred()
{
    boost::unique_lock<boost::mutex> lk(mut);
    waiting = true;
    cv.timed_wait(lk, boost::posix_time::microsec_clock::universal_time() + posix_wait_time, flagIsSet);
}

//------------------------------------------------------------------------------

void timed_wait_relative_without_pred()
{
    boost::unique_lock<boost::mutex> lk(mut);
    waiting = true;
    while (!flagIsSet())
    {
        cv.timed_wait(lk, posix_wait_time);
    }
}

void timed_wait_relative_with_pred()
{
    boost::unique_lock<boost::mutex> lk(mut);
    waiting = true;
    cv.timed_wait(lk, posix_wait_time, flagIsSet);
}

#else
#error "Test not applicable: BOOST_THREAD_USES_DATETIME not defined for this platform as not supported"
#endif

//------------------------------------------------------------------------------

#ifdef BOOST_THREAD_USES_CHRONO

boost::chrono::milliseconds chrono_wait_time(1000);

template <typename F>
void test_chrono_wait_function(F f)
{
    flag = false;
    waiting = false;
    boost::thread t(f);
    while (!threadIsWaiting())
    {
        boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
    }

    boost::unique_lock<boost::mutex> lk(mut);
    boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
    boost::chrono::steady_clock::time_point t0 = boost::chrono::steady_clock::now();
    flag = true;
    cv.notify_one();
    lk.unlock();
    t.join();
    boost::chrono::steady_clock::time_point t1 = boost::chrono::steady_clock::now();

    BOOST_TEST(t1 - t0 < boost::chrono::milliseconds(250));
}

//------------------------------------------------------------------------------

void wait_until_system_without_pred()
{
    boost::unique_lock<boost::mutex> lk(mut);
    waiting = true;
    while (!flagIsSet())
    {
        cv.wait_until(lk, boost::chrono::system_clock::now() + chrono_wait_time);
    }
}

void wait_until_system_with_pred()
{
    boost::unique_lock<boost::mutex> lk(mut);
    waiting = true;
    cv.wait_until(lk, boost::chrono::system_clock::now() + chrono_wait_time, flagIsSet);
}

//------------------------------------------------------------------------------

void wait_until_steady_without_pred()
{
    boost::unique_lock<boost::mutex> lk(mut);
    waiting = true;
    while (!flagIsSet())
    {
        cv.wait_until(lk, boost::chrono::steady_clock::now() + chrono_wait_time);
    }
}

void wait_until_steady_with_pred()
{
    boost::unique_lock<boost::mutex> lk(mut);
    waiting = true;
    cv.wait_until(lk, boost::chrono::steady_clock::now() + chrono_wait_time, flagIsSet);
}

//------------------------------------------------------------------------------

void wait_for_without_pred()
{
    boost::unique_lock<boost::mutex> lk(mut);
    waiting = true;
    while (!flagIsSet())
    {
        cv.wait_for(lk, chrono_wait_time);
    }
}

void wait_for_with_pred()
{
    boost::unique_lock<boost::mutex> lk(mut);
    waiting = true;
    cv.wait_for(lk, chrono_wait_time, flagIsSet);
}

#else
#error "Test not applicable: BOOST_THREAD_USES_CHRONO not defined for this platform as not supported"
#endif

//------------------------------------------------------------------------------

int main()
{
#ifdef BOOST_THREAD_USES_DATETIME
    test_posix_wait_function(timed_wait_absolute_without_pred);
    test_posix_wait_function(timed_wait_absolute_with_pred);
    test_posix_wait_function(timed_wait_relative_without_pred);
    test_posix_wait_function(timed_wait_relative_with_pred);
#endif

#ifdef BOOST_THREAD_USES_CHRONO
    test_chrono_wait_function(wait_until_system_without_pred);
    test_chrono_wait_function(wait_until_system_with_pred);
    test_chrono_wait_function(wait_until_steady_without_pred);
    test_chrono_wait_function(wait_until_steady_with_pred);
    test_chrono_wait_function(wait_for_without_pred);
    test_chrono_wait_function(wait_for_with_pred);
#endif

    return 0;
}
