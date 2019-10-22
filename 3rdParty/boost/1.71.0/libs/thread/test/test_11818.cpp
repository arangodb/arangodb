// Copyright (C) 2014 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_THREAD_VERSION 4
#include <boost/config.hpp>
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#include <boost/thread/future.hpp>
#include <boost/thread/thread.hpp>
#include <thread>

int main()
{

  {
    boost::promise<int> promise;
    boost::future<int> future = promise.get_future();

    boost::future<int> result =
    future.then
    (
        boost::launch::deferred,
        [](boost::future<int> && f)
        {
            std::cout << std::this_thread::get_id() << ": callback" << std::endl;
            std::cout << "The value is: " << f.get() << std::endl;
            return f.get();
        }
    );

    // We could not reach here.
    std::cout << std::this_thread::get_id() << ": function" << std::endl;

    promise.set_value(0);
  }

  {
    boost::promise<int> promise;
    boost::shared_future<int> future = promise.get_future().share();

    boost::future<int> result =
    future.then
    (
        boost::launch::deferred,
        [](boost::shared_future<int> && f)
        {
            std::cout << std::this_thread::get_id() << ": callback" << std::endl;
            std::cout << "The value is: " << f.get() << std::endl;
            return f.get();
        }
    );

    // We could not reach here.
    std::cout << std::this_thread::get_id() << ": function" << std::endl;

    promise.set_value(0);
  }

  return 0;
}
