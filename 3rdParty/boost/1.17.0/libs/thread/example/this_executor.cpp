// Copyright (C) 2014 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#define BOOST_THREAD_VERSION 5
#define BOOST_THREAD_USES_LOG_THREAD_ID

#include <boost/thread/caller_context.hpp>
#include <boost/thread/executors/basic_thread_pool.hpp>
#include <boost/thread/executors/generic_executor_ref.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/make_shared.hpp>
#include <string>
#include <iostream>

#include <boost/thread/caller_context.hpp>

struct current_executor_state_type {
  boost::shared_ptr<boost::generic_executor_ref> current_executor_ptr;

  template <class Executor>
  void set_current_executor(Executor& ex)
  {
    current_executor_ptr = boost::make_shared<boost::generic_executor_ref>(ex);
  }
  boost::generic_executor_ref current_executor()
  {
    if (current_executor_ptr)
      return *current_executor_ptr;
    else
      throw "";
  }
};

thread_local current_executor_state_type current_executor_state;

boost::generic_executor_ref current_executor()
{
  return current_executor_state.current_executor();
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
  current_executor().submit(&p2);
  boost::this_thread::sleep_for(boost::chrono::milliseconds(400));
  std::cout << BOOST_CONTEXTOF << std::endl;
}

int main()
{
  std::cout << BOOST_CONTEXTOF << std::endl;

  boost::basic_thread_pool tp(4,
      // at_thread_entry
      [](boost::basic_thread_pool& pool)
      {
        current_executor_state.set_current_executor(pool);
      }
  );

  tp.submit(&p1);

  boost::this_thread::sleep_for(boost::chrono::seconds(5));

  std::cout << BOOST_CONTEXTOF << std::endl;

  return 1;

}
