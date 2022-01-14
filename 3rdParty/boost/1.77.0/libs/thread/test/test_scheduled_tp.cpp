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

#include <boost/bind/bind.hpp>
#include <boost/chrono.hpp>
#include <boost/chrono/chrono_io.hpp>
#include <boost/function.hpp>
#include <boost/thread/executors/scheduled_thread_pool.hpp>
#include <iostream>

#include <boost/core/lightweight_test.hpp>

using namespace boost::chrono;

typedef boost::scheduled_thread_pool scheduled_tp;

void fn(int x)
{
  std::cout << x << std::endl;
}

void func(steady_clock::time_point pushed, steady_clock::duration dur)
{
    BOOST_TEST(pushed + dur < steady_clock::now());
}
void func2(scheduled_tp* tp, steady_clock::duration d)
{
  boost::function<void()> fn = boost::bind(func,steady_clock::now(),d);
  tp->submit_after(fn, d);
}



void test_timing(const int n)
{
  //This function should take n seconds to execute.
  boost::scheduled_thread_pool se(4);

  for(int i = 1; i <= n; i++)
  {
    se.submit_after(boost::bind(fn,i), milliseconds(i*100));
  }
  boost::this_thread::sleep_for(boost::chrono::seconds(10));
  //dtor is called here so all task will have to be executed before we return
}

void test_deque_timing()
{
    boost::scheduled_thread_pool se(4);
    for(int i = 0; i < 10; i++)
    {
        steady_clock::duration d = milliseconds(i*100);
        boost::function<void()> fn = boost::bind(func,steady_clock::now(),d);
        se.submit_after(fn,d);
    }
}

void test_deque_multi(const int n)
{
    scheduled_tp se(4);
    boost::thread_group tg;
    for(int i = 0; i < n; i++)
    {
        steady_clock::duration d = milliseconds(i*100);
        //boost::function<void()> fn = boost::bind(func,steady_clock::now(),d);
        //tg.create_thread(boost::bind(boost::mem_fn(&scheduled_tp::submit_after), &se, fn, d));
        tg.create_thread(boost::bind(func2, &se, d));
    }
    tg.join_all();
    //dtor is called here so execution will block until all the closures
    //have been completed.
}

int main()
{
  steady_clock::time_point start = steady_clock::now();
  test_timing(5);
  steady_clock::duration diff = steady_clock::now() - start;
  BOOST_TEST(diff > milliseconds(500));
  test_deque_timing();
  test_deque_multi(4);
  test_deque_multi(8);
  test_deque_multi(16);
  return boost::report_errors();
}
