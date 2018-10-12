// Copyright (C) 2012 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/locks.hpp>

// template <class Mutex> class nested_strict_lock;

// nested_strict_lock(Mutex &);

#include <boost/thread/lock_types.hpp>
#include <boost/thread/strict_lock.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/detail/lightweight_test.hpp>
#include "../../../../timming.hpp"

#ifdef BOOST_THREAD_USES_CHRONO
typedef boost::chrono::high_resolution_clock Clock;
typedef Clock::time_point time_point;
typedef Clock::duration duration;
typedef boost::chrono::milliseconds ms;
typedef boost::chrono::nanoseconds ns;
#endif

boost::mutex m;

const ms max_diff(BOOST_THREAD_TEST_TIME_MS);

void f()
{
#ifdef BOOST_THREAD_USES_CHRONO
  time_point t0 = Clock::now();
  time_point t1;
  boost::unique_lock<boost::mutex> lg(m);
  {
    boost::nested_strict_lock<boost::unique_lock<boost::mutex> > nlg(lg);
    t1 = Clock::now();
  }
  ns d = t1 - t0 - ms(250);
  BOOST_THREAD_TEST_IT(d, ns(max_diff));
#else
  //time_point t0 = Clock::now();
  //time_point t1;
  boost::unique_lock<boost::mutex> lg(m);
  {
    boost::nested_strict_lock<boost::unique_lock<boost::mutex> > nlg(lg);
    //t1 = Clock::now();
  }
  //ns d = t1 - t0 - ms(250);
  //BOOST_TEST(d < max_diff);
#endif
}

int main()
{
  {
  m.lock();
  boost::thread t(f);
#ifdef BOOST_THREAD_USES_CHRONO
  boost::this_thread::sleep_for(ms(250));
#endif
  m.unlock();
  t.join();
  }

  return boost::report_errors();
}
