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

// <boost/thread/locks.hpp>

// template <class Mutex> class unique_lock;

// template <class Clock, class Duration>
//   unique_lock(mutex_type& m, const chrono::time_point<Clock, Duration>& abs_time);

#define BOOST_THREAD_PROVIDES_GENERIC_SHARED_MUTEX_ON_WIN

#include <boost/thread/lock_types.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/detail/lightweight_test.hpp>
#include "../../../../../timming.hpp"

#if defined BOOST_THREAD_USES_CHRONO

boost::timed_mutex m;

typedef boost::chrono::high_resolution_clock Clock;
typedef Clock::time_point time_point;
typedef Clock::duration duration;
typedef boost::chrono::milliseconds ms;
typedef boost::chrono::nanoseconds ns;
time_point t0;
time_point t1;

const ms max_diff(BOOST_THREAD_TEST_TIME_MS);

void f1()
{
  t0 = Clock::now();
  boost::unique_lock<boost::timed_mutex> lk(m, Clock::now() + ms(750));
  BOOST_TEST(lk.owns_lock() == true);
  t1 = Clock::now();
}

void f2()
{
  t0 = Clock::now();
  boost::unique_lock<boost::timed_mutex> lk(m, Clock::now() + ms(250));
  BOOST_TEST(lk.owns_lock() == false);
  t1 = Clock::now();
  ns d = t1 - t0 - ms(250);
  BOOST_TEST(d < max_diff);
}

int main()
{
  {
    m.lock();
    boost::thread t(f1);
    time_point t2 = Clock::now();
    boost::this_thread::sleep_for(ms(250));
    time_point t3 = Clock::now();
    m.unlock();
    t.join();

    ns sleep_time = t3 - t2;
    ns d_ns = t1 - t0 - sleep_time;
    ms d_ms = boost::chrono::duration_cast<boost::chrono::milliseconds>(d_ns);
    // BOOST_TEST_GE(d_ms.count(), 0);
    BOOST_THREAD_TEST_IT(d_ms, max_diff);
    BOOST_THREAD_TEST_IT(d_ns, ns(max_diff));
  }
  {
    m.lock();
    boost::thread t(f2);
    boost::this_thread::sleep_for(ms(750));
    m.unlock();
    t.join();
  }

  return boost::report_errors();
}

#else
#error "Test not applicable: BOOST_THREAD_USES_CHRONO not defined for this platform as not supported"
#endif
