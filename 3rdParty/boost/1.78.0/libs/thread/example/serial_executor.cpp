// Copyright (C) 2015 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_PROVIDES_EXECUTORS
//#define BOOST_THREAD_USES_LOG
#define BOOST_THREAD_USES_LOG_THREAD_ID
#define BOOST_THREAD_QUEUE_DEPRECATE_OLD

#include <boost/thread/caller_context.hpp>
#include <boost/thread/executors/basic_thread_pool.hpp>
#include <boost/thread/executors/serial_executor.hpp>
#include <boost/thread/executors/executor.hpp>
#include <boost/thread/executors/executor_adaptor.hpp>
#include <boost/thread/executor.hpp>
#include <boost/thread/future.hpp>
#include <boost/assert.hpp>
#include <string>
#include <iostream>

void p1()
{
  //std::cout << BOOST_CONTEXTOF << std::endl;
  boost::this_thread::sleep_for(boost::chrono::milliseconds(30));
  //std::cout << BOOST_CONTEXTOF << std::endl;
}

void p2()
{
  //std::cout << BOOST_CONTEXTOF << std::endl;
  boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
  //std::cout << BOOST_CONTEXTOF << std::endl;
}

int f1()
{
  // std::cout << BOOST_CONTEXTOF << std::endl;
  boost::this_thread::sleep_for(boost::chrono::seconds(1));
  return 1;
}
int f2(int i)
{
  // std::cout << BOOST_CONTEXTOF << std::endl;
  boost::this_thread::sleep_for(boost::chrono::seconds(2));
  return i + 1;
}

void submit_some(boost::serial_executor& tp)
{
  for (int i = 0; i < 3; ++i) {
    std::cout << BOOST_CONTEXTOF << std::endl;
    tp.submit(&p2);
  }
  for (int i = 0; i < 3; ++i) {
    std::cout << BOOST_CONTEXTOF << std::endl;
    tp.submit(&p1);
  }

}


void at_th_entry(boost::basic_thread_pool& )
{

}

int test_executor_adaptor()
{
  {
    try
    {

#if ! defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
      {
        boost::basic_thread_pool ea1(4);
        boost::serial_executor ea2(ea1);
        submit_some(ea2);
        boost::this_thread::sleep_for(boost::chrono::seconds(10));
      }
#endif
    }
    catch (std::exception& ex)
    {
      std::cout << "ERROR= " << ex.what() << "" << std::endl;
      return 1;
    }
    catch (...)
    {
      std::cout << " ERROR= exception thrown" << std::endl;
      return 2;
    }
  }
  return 0;
}


int main()
{
  return test_executor_adaptor();
}
