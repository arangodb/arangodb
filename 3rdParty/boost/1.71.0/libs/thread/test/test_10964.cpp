// Copyright (C) 2015 Vicente Botet
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
#include <iostream>
#include <boost/thread/executors/basic_thread_pool.hpp>

struct TestCallback
{
  typedef boost::future<void> result_type;

  result_type operator()(boost::future<void> future) const
  {
    std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
    assert(future.is_ready());
    std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
    future.wait();
    std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
    return boost::make_ready_future();
  }

  result_type operator()(boost::future<boost::future<void> > future) const
  {
    std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
    assert(future.is_ready());
    std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
    assert(future.get().is_ready());
    std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
    //boost::future<void> ff = future.get();

    return boost::make_ready_future();
  }
  result_type operator()(boost::shared_future<void> future) const
  {
    std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
    assert(future.is_ready());
    std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
    future.wait();
    std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
    return boost::make_ready_future();
  }

  result_type operator()(boost::shared_future<boost::future<void> > future) const
  {
    std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
    assert(future.is_ready());
    std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
    assert(future.get().is_ready());
    std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
    //boost::future<void> ff = future.get();

    return boost::make_ready_future();
  }
};

void p1()
{
}

int main()
{
  const int number_of_tests = 2;
  (void)(number_of_tests);

#if ! defined  BOOST_NO_CXX11_DECLTYPE && ! defined  BOOST_NO_CXX11_AUTO_DECLARATIONS
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  {
    auto f1 = boost::make_ready_future().then(TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f1), boost::future<boost::future<void> > >::value);
    f1.wait();
  }
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  for (int i=0; i< number_of_tests; i++)
  {
    auto f1 = boost::make_ready_future().then(TestCallback());
    std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
    BOOST_STATIC_ASSERT(std::is_same<decltype(f1), boost::future<boost::future<void> > >::value);
    std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
    auto f2 = f1.unwrap();
    std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
    BOOST_STATIC_ASSERT(std::is_same<decltype(f2), boost::future<void> >::value);
    std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
    f2.wait();
    std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  }
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  for (int i=0; i< number_of_tests; i++)
  {
    auto f1 = boost::make_ready_future().then(TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f1), boost::future<boost::future<void> > >::value);
    boost::future<void> f2 = f1.get();
  }
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  {
    auto f1 = boost::make_ready_future().then(TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f1), boost::future<boost::future<void> > >::value);
    auto f3 = f1.then(TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f3), boost::future<boost::future<void> > >::value);
    f3.wait();
  }
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  for (int i=0; i< number_of_tests; i++)
  {
    auto f1 = boost::make_ready_future().then(TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f1), boost::future<boost::future<void> > >::value);
    auto f2 = f1.unwrap();
    BOOST_STATIC_ASSERT(std::is_same<decltype(f2), boost::future<void> >::value);
    auto f3 = f2.then(TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f3), boost::future<boost::future<void> > >::value);
    f3.wait();
  }
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  for (int i=0; i< number_of_tests; i++)
  {
        boost::make_ready_future().then(
            TestCallback()).unwrap().then(TestCallback()).get();
  }
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  for (int i=0; i< number_of_tests; i++)
  {
    boost::future<void> f = boost::async(p1);
    f.then(
            TestCallback()).unwrap().then(TestCallback()).get();
  }
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  for (int i=0; i< number_of_tests; i++)
  {
    auto f1 = boost::make_ready_future().then(TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f1), boost::future<boost::future<void> > >::value);
    auto f3 = f1.then(TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f3), boost::future<boost::future<void> > >::value);
    f3.wait();
  }
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  for (int i=0; i< number_of_tests; i++)
  {
    boost::basic_thread_pool executor;
    auto f1 = boost::make_ready_future().then(executor, TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f1), boost::future<boost::future<void> > >::value);
    auto f3 = f1.then(executor, TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f3), boost::future<boost::future<void> > >::value);
    f3.wait();
  }
#if 1
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  // fixme
  for (int i=0; i< number_of_tests; i++)
  {
    boost::basic_thread_pool executor(2);

    auto f1 = boost::make_ready_future().then(executor, TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f1), boost::future<boost::future<void> > >::value);
    std::cout << __FILE__ << "[" << __LINE__ << "] " << int(f1.valid()) << std::endl;
    auto f2 = f1.unwrap();
    std::cout << __FILE__ << "[" << __LINE__ << "] " << int(f2.valid()) << std::endl;

    BOOST_STATIC_ASSERT(std::is_same<decltype(f2), boost::future<void> >::value);
    auto f3 = f2.then(executor, TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f3), boost::future<boost::future<void> > >::value);
    f3.wait();
  }
#endif
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;
  for (int i=0; i< number_of_tests; i++)
  {
    boost::basic_thread_pool executor;

    auto f1 = boost::make_ready_future().then(executor, TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f1), boost::future<boost::future<void> > >::value);
    auto f2 = f1.unwrap();
    BOOST_STATIC_ASSERT(std::is_same<decltype(f2), boost::future<void> >::value);
    auto f3 = f2.then(executor, TestCallback());
    BOOST_STATIC_ASSERT(std::is_same<decltype(f3), boost::future<boost::future<void> > >::value);
    f3.wait();
  }
  std::cout << __FILE__ << "[" << __LINE__ << "]" << std::endl;

#endif
  return 0;
}
