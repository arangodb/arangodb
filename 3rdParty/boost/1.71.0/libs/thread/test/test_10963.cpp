// Copyright (C) 2014 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_THREAD_VERSION 4
#include <boost/config.hpp>
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif
#define BOOST_THREAD_PROVIDES_EXECUTORS

#include <boost/thread/future.hpp>
#include <boost/static_assert.hpp>
#include <cassert>
#include <boost/thread/executors/basic_thread_pool.hpp>


struct TestCallback
{
    typedef boost::future<void> result_type;

    result_type operator()(boost::future<void> future) const
    {
      assert(future.is_ready());
        future.get();
        return boost::make_ready_future();
    }

    result_type operator()(boost::future<boost::future<void> > future) const
    {
      assert(future.is_ready());
       future.get();
       return boost::make_ready_future();
    }
};

int main()
{
#if ! defined  BOOST_NO_CXX11_DECLTYPE && ! defined  BOOST_NO_CXX11_AUTO_DECLARATIONS
  {
    boost::promise<void> test_promise;
    boost::future<void> test_future(test_promise.get_future());
    auto f1 = test_future.then(TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f1), boost::future<boost::future<void> > >::value);
    auto f2 = f1.then(TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f2), boost::future<boost::future<void> > >::value);
  }
  {
    boost::basic_thread_pool executor;
    boost::promise<void> test_promise;
    boost::future<void> test_future(test_promise.get_future());
    auto f1 = test_future.then(executor, TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f1), boost::future<boost::future<void> > >::value);
    auto f2 = f1.then(executor, TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f2), boost::future<boost::future<void> > >::value);

  }
#endif
    return 0;
}
