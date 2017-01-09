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

#include <boost/function.hpp>
#include <boost/thread/executors/executor.hpp>
#include <boost/thread/executors/basic_thread_pool.hpp>
#include <boost/thread/executors/scheduling_adaptor.hpp>
#include <boost/chrono/chrono_io.hpp>

#include <boost/core/lightweight_test.hpp>

using namespace boost::chrono;


typedef boost::executors::basic_thread_pool thread_pool;

void fn(int x)
{
  //std::cout << "[" << __LINE__ << "] " << steady_clock::now() << std::endl;
    std::cout << x << std::endl;
}

void test_timing(const int n)
{
    thread_pool tp(4);
    boost::scheduling_adpator<thread_pool> sa(tp);
    for(int i = 1; i <= n; i++)
    {
        sa.submit_after(boost::bind(fn,i),seconds(i));
        sa.submit_after(boost::bind(fn,i), milliseconds(i*100));
    }
    boost::this_thread::sleep_for(boost::chrono::seconds(10));
}

int main()
{
  steady_clock::time_point start = steady_clock::now();
  test_timing(5);
  steady_clock::duration diff = steady_clock::now() - start;
  BOOST_TEST(diff > seconds(5));
  return boost::report_errors();
}
