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

// <boost/thread/condition_variable>

// class condition_variable;

// condition_variable(const condition_variable&) = delete;

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <iostream>
#include <cassert>
#include "../../../timming.hpp"

#if defined BOOST_THREAD_USES_CHRONO
typedef boost::chrono::milliseconds ms;
typedef boost::chrono::nanoseconds ns;
struct Clock
{
  typedef boost::chrono::milliseconds duration;
  typedef duration::rep rep;
  typedef duration::period period;
  typedef boost::chrono::time_point<Clock> time_point;
  static const bool is_steady = true;

  static time_point now()
  {
    using namespace boost::chrono;
    return time_point(duration_cast<duration> (steady_clock::now().time_since_epoch()));
  }
};

boost::condition_variable cv;
boost::mutex mut;


int test1 = 0;
int test2 = 0;

int runs = 0;

const ms max_diff(BOOST_THREAD_TEST_TIME_MS);

void f()
{
  try {
    boost::unique_lock < boost::mutex > lk(mut);
    assert(test2 == 0);
    test1 = 1;
    cv.notify_one();
    Clock::time_point t0 = Clock::now();
    Clock::time_point t = t0 + Clock::duration(250);
    while (test2 == 0 && cv.wait_until(lk, t) == boost::cv_status::no_timeout) {}
    Clock::time_point t1 = Clock::now();
    if (runs == 0)
    {
      ns d = t1 - t0;
      BOOST_THREAD_TEST_IT(d, ns(max_diff));
      assert(test2 != 0);
    }
    else
    {
      ns d = t1 - t0 - Clock::duration(250);
      BOOST_THREAD_TEST_IT(d, ns(max_diff));
      assert(test2 == 0);
    }
    ++runs;
  } catch(...) {
      assert(false);
      std::cout << "ERROR exception" << __LINE__ << std::endl;
  }
}

int main()
{
  try
  {
    boost::unique_lock < boost::mutex > lk(mut);
    boost::thread t(f);
    BOOST_TEST(test1 == 0);
    while (test1 == 0)
      cv.wait(lk);
    BOOST_TEST(test1 != 0);
    test2 = 1;
    lk.unlock();
    cv.notify_one();
    t.join();
  } catch(...) {
    BOOST_TEST(false);
    std::cout << "ERROR exception" << __LINE__ << std::endl;
  }

  test1 = 0;
  test2 = 0;
  try
  {
    boost::unique_lock < boost::mutex > lk(mut);
    boost::thread t(f);
    BOOST_TEST(test1 == 0);
    while (test1 == 0)
      cv.wait(lk);
    BOOST_TEST(test1 != 0);
    lk.unlock();
    t.join();
  } catch(...) {
    BOOST_TEST(false);
    std::cout << "ERROR exception" << __LINE__ << std::endl;
  }

  return boost::report_errors();
}

#else
#error "Test not applicable: BOOST_THREAD_USES_CHRONO not defined for this platform as not supported"
#endif
