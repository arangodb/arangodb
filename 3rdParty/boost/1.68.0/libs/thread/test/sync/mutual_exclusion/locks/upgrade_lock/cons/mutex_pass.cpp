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

// explicit upgrade_lock(Mutex& m);


#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <iostream>
#include "../../../../../timming.hpp"

boost::shared_mutex m;

#if defined BOOST_THREAD_USES_CHRONO
typedef boost::chrono::system_clock Clock;
typedef Clock::time_point time_point;
typedef Clock::duration duration;
typedef boost::chrono::milliseconds ms;
typedef boost::chrono::nanoseconds ns;
#else
#endif

const ms max_diff(BOOST_THREAD_TEST_TIME_MS);

void f()
{
#if defined BOOST_THREAD_USES_CHRONO
  time_point t0 = Clock::now();
  time_point t1;
  {
    boost::upgrade_lock<boost::shared_mutex> ul(m);
    t1 = Clock::now();
  }
  ns d = t1 - t0 - ms(250);
  std::cout << "diff= " << d.count() << std::endl;
  std::cout << "max_diff= " << max_diff.count() << std::endl;
  BOOST_THREAD_TEST_IT(d, ns(max_diff));
#else
  //time_point t0 = Clock::now();
  //time_point t1;
  {
    boost::upgrade_lock<boost::shared_mutex> ul(m);
    //t1 = Clock::now();
  }
  //ns d = t1 - t0 - ms(250);
  //BOOST_TEST(d < max_diff);
#endif
}

int main()
{
  m.lock();
  boost::thread t(f);
#if defined BOOST_THREAD_USES_CHRONO
  boost::this_thread::sleep_for(ms(250));
#else
#endif
  m.unlock();
  t.join();

  return boost::report_errors();
}

