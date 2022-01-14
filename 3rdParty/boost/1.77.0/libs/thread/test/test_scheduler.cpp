// Copyright (C) 2014 Ian Forbed
// Copyright (C) 2014 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/config.hpp>
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_PROVIDES_EXECUTORS

#include <boost/thread/executors/scheduler.hpp>
#include <boost/thread/executors/basic_thread_pool.hpp>
#include <boost/chrono/chrono_io.hpp>
#include <iostream>

#include <boost/core/lightweight_test.hpp>

using namespace boost::chrono;


typedef boost::executors::basic_thread_pool thread_pool;

void fn(int x)
{
  //std::cout << "[" << __LINE__ << "] " << steady_clock::now() << std::endl;
    std::cout << x << std::endl;
}

void test_scheduler(const int n, boost::scheduler<>& sch)
{
    for(int i = 1; i <= n; i++)
    {
      sch.submit_after(boost::bind(fn,i), seconds(i));
      sch.submit_after(boost::bind(fn,i), milliseconds(i*100));
    }
}

void test_after(const int n, boost::scheduler<>& sch)
{
    for(int i = 1; i <= n; i++)
    {
      sch.after(seconds(i)).submit(boost::bind(fn,i));
      sch.after(milliseconds(i*100)).submit(boost::bind(fn,i));
    }
}

void test_at(const int n, boost::scheduler<>& sch)
{
    for(int i = 1; i <= n; i++)
    {
      sch.at(steady_clock::now()+seconds(i)).submit(boost::bind(fn,i));
      sch.at(steady_clock::now()+milliseconds(i*100)).submit(boost::bind(fn,i));
    }
}

void test_on(const int n, boost::scheduler<>& sch, thread_pool& tp)
{
    for(int i = 1; i <= n; i++)
    {
      sch.on(tp).after(seconds(i)).submit(boost::bind(fn,i));
      sch.on(tp).after(milliseconds(i*100)).submit(boost::bind(fn,i));
    }
}

int main()
{
  thread_pool tp(4);
  boost::scheduler<> sch;
  test_scheduler(5, sch);
  test_after(5, sch);
  test_at(5, sch);
  test_on(5, sch, tp);
  boost::this_thread::sleep_for(boost::chrono::seconds(10));

  return boost::report_errors();
}
