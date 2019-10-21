// Copyright (C) 2012 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/lock_factories.hpp>

// template <class Mutex>
// unique_lock<Mutex> make_unique_lock(Mutex&);

#define BOOST_THREAD_VERSION 4

#include <boost/thread/lock_factories.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/detail/lightweight_test.hpp>
#include "../../../../../timming.hpp"

#if ! defined(BOOST_NO_CXX11_AUTO_DECLARATIONS) && ! defined BOOST_THREAD_NO_MAKE_UNIQUE_LOCKS && ! defined BOOST_NO_CXX11_RVALUE_REFERENCES

boost::mutex m1;
boost::mutex m2;
boost::mutex m3;


#if defined BOOST_THREAD_USES_CHRONO

typedef boost::chrono::high_resolution_clock Clock;
typedef Clock::time_point time_point;
typedef Clock::duration duration;
typedef boost::chrono::milliseconds ms;
typedef boost::chrono::nanoseconds ns;
time_point t0;
time_point t1;
#else
#endif

const ms max_diff(BOOST_THREAD_TEST_TIME_MS);

void f()
{
#if defined BOOST_THREAD_USES_CHRONO
  t0 = Clock::now();
  {
    auto&& _ = boost::make_unique_locks(m1,m2,m3); (void)_;
    t1 = Clock::now();
  }
#else
  //time_point t0 = Clock::now();
  //time_point t1;
  {
    auto&& _ = boost::make_unique_locks(m1,m2,m3); (void)_;
    //t1 = Clock::now();
  }
  //ns d = t1 - t0 - ms(250);
  //BOOST_TEST(d < max_diff);
#endif
}

int main()
{
  m1.lock();
  m2.lock();
  m3.lock();
  boost::thread t(f);
#if defined BOOST_THREAD_USES_CHRONO
  time_point t2 = Clock::now();
  boost::this_thread::sleep_for(ms(250));
  time_point t3 = Clock::now();
#else
#endif
  m1.unlock();
  m2.unlock();
  m3.unlock();
  t.join();

#if defined BOOST_THREAD_USES_CHRONO
  ns sleep_time = t3 - t2;
  ns d_ns = t1 - t0 - sleep_time;
  ms d_ms = boost::chrono::duration_cast<boost::chrono::milliseconds>(d_ns);
  // BOOST_TEST_GE(d_ms.count(), 0);
  BOOST_THREAD_TEST_IT(d_ms, max_diff);
  BOOST_THREAD_TEST_IT(d_ns, ns(max_diff));
#endif

  return boost::report_errors();
}
#else
int main()
{
  return boost::report_errors();
}
#endif

