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

// <boost/thread/null_mutex.hpp>

// class null_mutex;

// template <class Rep, class Period>
//     bool try_lock_for(const chrono::duration<Rep, Period>& rel_time);

#include <boost/thread/null_mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/detail/lightweight_test.hpp>
#include "../../../timming.hpp"

#if defined BOOST_THREAD_USES_CHRONO


boost::null_mutex m;

typedef boost::chrono::high_resolution_clock Clock;
typedef Clock::time_point time_point;
typedef Clock::duration duration;
typedef boost::chrono::milliseconds ms;
typedef boost::chrono::nanoseconds ns;

const ms max_diff(BOOST_THREAD_TEST_TIME_MS);

void f1()
{
  time_point t0 = Clock::now();
  BOOST_TEST(m.try_lock_for(ms(250)) == true);
  time_point t1 = Clock::now();
  BOOST_TEST(m.try_lock());
  m.unlock();
  m.unlock();
  ns d = t1 - t0 ;
  BOOST_THREAD_TEST_IT(d, ns(max_diff));
}

int main()
{
  {
    m.lock();
    boost::thread t(f1);
    m.unlock();
    t.join();
  }

  return boost::report_errors();
}

#else
#error "Test not applicable: BOOST_THREAD_USES_CHRONO not defined for this platform as not supported"
#endif

