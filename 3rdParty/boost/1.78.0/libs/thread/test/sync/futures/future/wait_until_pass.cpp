//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Copyright (C) 2013 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/future.hpp>

// class future<R>

// template <class Rep, class Period>
//   future_status
//   wait_until(const chrono::time_point<Clock, Duration>& abs_time) const;

//#define BOOST_THREAD_VERSION 3
#define BOOST_THREAD_VERSION 4
//#define BOOST_THREAD_USES_LOG
#define BOOST_THREAD_USES_LOG_THREAD_ID
#include <boost/thread/detail/log.hpp>

#include <boost/thread/future.hpp>
#include <boost/thread/thread.hpp>
#include <boost/chrono/chrono_io.hpp>
#include <boost/detail/lightweight_test.hpp>
#include "../../../timming.hpp"

#if defined BOOST_THREAD_USES_CHRONO

#ifdef BOOST_MSVC
#pragma warning(disable: 4127) // conditional expression is constant
#endif

typedef boost::chrono::milliseconds ms;
typedef boost::chrono::nanoseconds ns;

namespace boost
{
  template <typename OStream>
  OStream& operator<<(OStream& os , boost::future_status st )
  {
    os << underlying_cast<int>(st) << " ";
    return os;
  }
}

void func1(boost::promise<int> p)
{
  boost::this_thread::sleep_for(ms(500));
  p.set_value(3);
}

int j = 0;

void func3(boost::promise<int&> p)
{
  boost::this_thread::sleep_for(ms(500));
  j = 5;
  p.set_value(j);
}

void func5(boost::promise<void> p)
{
  boost::this_thread::sleep_for(ms(500));
  p.set_value();
}

const ms max_diff(BOOST_THREAD_TEST_TIME_MS);

int main()
{
  BOOST_THREAD_LOG << BOOST_THREAD_END_LOG;
  {
    typedef boost::chrono::high_resolution_clock Clock;
    {
      typedef int T;
      boost::promise<T> p;
      boost::future<T> f = p.get_future();
#if defined BOOST_THREAD_PROVIDES_SIGNATURE_PACKAGED_TASK && defined(BOOST_THREAD_PROVIDES_VARIADIC_THREAD)
      boost::thread(func1, boost::move(p)).detach();
#endif
      BOOST_TEST(f.valid());
      BOOST_TEST_EQ(f.wait_until(Clock::now() + ms(250)) , boost::future_status::timeout);
#if defined BOOST_THREAD_PROVIDES_SIGNATURE_PACKAGED_TASK && defined(BOOST_THREAD_PROVIDES_VARIADIC_THREAD)
#else
      func1(boost::move(p));
#endif
      BOOST_TEST(f.valid());
      BOOST_TEST_EQ(f.wait_until(Clock::now() + ms(750)) , boost::future_status::ready);
      BOOST_TEST(f.valid());
      Clock::time_point t0 = Clock::now();
      f.wait();
      Clock::time_point t1 = Clock::now();
      BOOST_TEST(f.valid());
      ns d = t1 - t0;
      BOOST_THREAD_TEST_IT(d, ns(max_diff));
    }
    {
      typedef int& T;
      boost::promise<T> p;
      boost::future<T> f = p.get_future();
#if defined BOOST_THREAD_PROVIDES_SIGNATURE_PACKAGED_TASK && defined(BOOST_THREAD_PROVIDES_VARIADIC_THREAD)
      boost::thread(func3, boost::move(p)).detach();
#endif
      BOOST_TEST(f.valid());
      BOOST_TEST_EQ(f.wait_until(Clock::now() + ms(250)) , boost::future_status::timeout);
      BOOST_TEST(f.valid());
#if defined BOOST_THREAD_PROVIDES_SIGNATURE_PACKAGED_TASK && defined(BOOST_THREAD_PROVIDES_VARIADIC_THREAD)
#else
      func3(boost::move(p));
#endif
      BOOST_TEST_EQ(f.wait_until(Clock::now() + ms(750)) , boost::future_status::ready);
      BOOST_TEST(f.valid());
      Clock::time_point t0 = Clock::now();
      f.wait();
      Clock::time_point t1 = Clock::now();
      BOOST_TEST(f.valid());
      ns d = t1 - t0;
      BOOST_THREAD_TEST_IT(d, ns(max_diff));
    }
    {
      typedef void T;
      boost::promise<T> p;
      boost::future<T> f = p.get_future();
#if defined BOOST_THREAD_PROVIDES_SIGNATURE_PACKAGED_TASK && defined(BOOST_THREAD_PROVIDES_VARIADIC_THREAD)
      boost::thread(func5, boost::move(p)).detach();
#endif
      BOOST_TEST(f.valid());
      BOOST_TEST_EQ(f.wait_until(Clock::now() + ms(250)) , boost::future_status::timeout);
      BOOST_TEST(f.valid());
#if defined BOOST_THREAD_PROVIDES_SIGNATURE_PACKAGED_TASK && defined(BOOST_THREAD_PROVIDES_VARIADIC_THREAD)
#else
      func5(boost::move(p));
#endif
      BOOST_TEST_EQ(f.wait_until(Clock::now() + ms(750)) , boost::future_status::ready);
      BOOST_TEST(f.valid());
      Clock::time_point t0 = Clock::now();
      f.wait();
      Clock::time_point t1 = Clock::now();
      BOOST_TEST(f.valid());
      ns d = t1 - t0;
      BOOST_THREAD_TEST_IT(d, ns(max_diff));
    }
  }
  BOOST_THREAD_LOG << BOOST_THREAD_END_LOG;

  return boost::report_errors();
}

#else
#error "Test not applicable: BOOST_THREAD_USES_CHRONO not defined for this platform as not supported"
#endif
