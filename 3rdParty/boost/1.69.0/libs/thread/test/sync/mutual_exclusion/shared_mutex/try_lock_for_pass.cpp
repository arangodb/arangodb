//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Copyright (C) 2012 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/shared_mutex.hpp>

// class shared_mutex;

// template <class Rep, class Period>
//     bool try_lock_for(const chrono::duration<Rep, Period>& rel_time);

#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/detail/lightweight_test.hpp>
#include "../../../timming.hpp"

boost::shared_mutex m;

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
  BOOST_TEST(m.try_lock_for(ms(750)) == true);
  t1 = Clock::now();
  m.unlock();
}

void f2()
{
  t0 = Clock::now();
  BOOST_TEST(m.try_lock_for(ms(250)) == false);
  t1 = Clock::now();
  ns d = t1 - t0 - ms(250);
  BOOST_THREAD_TEST_IT(d, ns(max_diff));
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

#if defined BOOST_THREAD_USES_CHRONO
    ns sleep_time = t3 - t2;
    ns d_ns = t1 - t0 - sleep_time;
    ms d_ms = boost::chrono::duration_cast<boost::chrono::milliseconds>(d_ns);
    // BOOST_TEST_GE(d_ms.count(), 0);
    BOOST_THREAD_TEST_IT(d_ms, max_diff);
    BOOST_THREAD_TEST_IT(d_ns, ns(max_diff));
#endif
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

