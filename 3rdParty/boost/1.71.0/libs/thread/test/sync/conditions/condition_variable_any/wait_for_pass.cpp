//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Copyright (C) 2011 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/condition_variable_any>

// class condition_variable_any;

// condition_variable_any(const condition_variable_any&) = delete;

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/detail/lightweight_test.hpp>
#include "../../../timming.hpp"

#if defined BOOST_THREAD_USES_CHRONO

boost::condition_variable_any cv;

typedef boost::timed_mutex L0;
typedef boost::unique_lock<L0> L1;

L0 m0;

int test1 = 0;
int test2 = 0;

int runs = 0;

typedef boost::chrono::system_clock Clock;
typedef boost::chrono::milliseconds milliseconds;
typedef boost::chrono::milliseconds ms;
typedef boost::chrono::nanoseconds ns;

const ms max_diff(BOOST_THREAD_TEST_TIME_MS);

void f()
{
    L1 lk(m0);
    BOOST_TEST(test2 == 0);
    test1 = 1;
    cv.notify_one();
    Clock::time_point t0 = Clock::now();
    Clock::time_point t = t0 + milliseconds(250);
    while (test2 == 0 && cv.wait_for(lk, t - Clock::now()) == boost::cv_status::no_timeout) {}
    Clock::time_point t1 = Clock::now();
    if (runs == 0)
    {
        ns d = t1 - t0;
        BOOST_THREAD_TEST_IT(d, ns(max_diff));
        BOOST_TEST(test2 != 0);
    }
    else
    {
        ns d = t1 - t0 - ms(250);
        BOOST_THREAD_TEST_IT(d, ns(max_diff));
        BOOST_TEST(test2 == 0);
    }
    ++runs;
}

int main()
{
    {
        L1 lk(m0);
        boost::thread t(f);
        BOOST_TEST(test1 == 0);
        while (test1 == 0)
            cv.wait(lk);
        BOOST_TEST(test1 != 0);
        test2 = 1;
        lk.unlock();
        cv.notify_one();
        t.join();
    }
    test1 = 0;
    test2 = 0;
    {
        L1 lk(m0);
        boost::thread t(f);
        BOOST_TEST(test1 == 0);
        while (test1 == 0)
            cv.wait(lk);
        BOOST_TEST(test1 != 0);
        lk.unlock();
        t.join();
    }

  return boost::report_errors();
}

#else
#error "Test not applicable: BOOST_THREAD_USES_CHRONO not defined for this platform as not supported"
#endif
