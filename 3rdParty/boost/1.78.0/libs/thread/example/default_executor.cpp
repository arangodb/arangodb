// Copyright (C) 2014 Vicente Botet
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
#include <boost/thread/executors/generic_executor_ref.hpp>
#include <string>
#include <iostream>

#include <boost/thread/caller_context.hpp>


boost::generic_executor_ref default_executor()
{
  static boost::basic_thread_pool tp(4);
  return boost::generic_executor_ref(tp);
}

void p2()
{
  std::cout << BOOST_CONTEXTOF << std::endl;
  boost::this_thread::sleep_for(boost::chrono::milliseconds(200));
  std::cout << BOOST_CONTEXTOF << std::endl;
}


void p1()
{
  std::cout << BOOST_CONTEXTOF << std::endl;
  boost::this_thread::sleep_for(boost::chrono::milliseconds(200));
  default_executor().submit(&p2);
  boost::this_thread::sleep_for(boost::chrono::milliseconds(400));
  std::cout << BOOST_CONTEXTOF << std::endl;
}

int main()
{
  std::cout << BOOST_CONTEXTOF << std::endl;

  default_executor().submit(&p1);

  boost::this_thread::sleep_for(boost::chrono::seconds(5));

  std::cout << BOOST_CONTEXTOF << std::endl;

  return 1;

}
