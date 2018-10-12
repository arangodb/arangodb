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

// template <class Mutex> class upgrade_lock;

// template <class Rep, class Period>
//   upgrade_lock(mutex_type& m, const chrono::duration<Rep, Period>& rel_time);

#define BOOST_THREAD_PROVIDES_GENERIC_SHARED_MUTEX_ON_WIN

#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/chrono/chrono_io.hpp>
#include "../../../../../timming.hpp"

boost::shared_mutex m;

typedef boost::chrono::steady_clock Clock;
typedef Clock::time_point time_point;
typedef Clock::duration duration;
typedef boost::chrono::milliseconds ms;
typedef boost::chrono::nanoseconds ns;

const ms max_diff(BOOST_THREAD_TEST_TIME_MS);

void f1()
{
  time_point t0 = Clock::now();
  boost::upgrade_lock<boost::shared_mutex> lk(m, ms(750));
  BOOST_TEST(lk.owns_lock() == true);
  time_point t1 = Clock::now();
  ns d = t1 - t0 - ms(250);
  BOOST_THREAD_TEST_IT(d, ns(max_diff));
}

void f2()
{
  time_point t0 = Clock::now();
  boost::upgrade_lock<boost::shared_mutex> lk(m, ms(250));
  BOOST_TEST(lk.owns_lock() == false);
  time_point t1 = Clock::now();
  ns d = t1 - t0 - ms(250);
  BOOST_THREAD_TEST_IT(d, ns(max_diff));
}

int main()
{
  {
    m.lock();
    boost::thread t(f1);
    boost::this_thread::sleep_for(ms(250));
    m.unlock();
    t.join();
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

