// Copyright (C) 2013 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// futtest.cpp
#include <iostream>
#define BOOST_THREAD_VERSION 4

#include <boost/thread/future.hpp>
#include <boost/thread/thread.hpp>
#include <boost/bind/bind.hpp>
#include <boost/chrono.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>

typedef boost::shared_ptr< boost::promise<int> > IntPromise;

void foo(IntPromise p)
{
    std::cout << "foo" << std::endl;
    p->set_value(123); // This line locks the future's mutex, then calls the continuation with the mutex already locked.
}

void bar(boost::future<int> fooResult)
{
  try {
    std::cout << "bar" << std::endl;
    int i = fooResult.get(); // Code hangs on this line (Due to future already being locked by the set_value call)
    std::cout << "i: " << i << std::endl;
  } catch(...) {
    std::cout << __FILE__ << ":" << __LINE__ << std::endl;
  }
}

int main()
{
  std::cout << __FILE__ << ":" << __LINE__ << std::endl;
  try {
    IntPromise p(new boost::promise<int>());
    boost::thread t(boost::bind(foo, p));
    boost::future<int> f1 = p->get_future();
    f1.then(boost::launch::deferred, &bar);
    t.join();
  } catch(...) {
    return 1;
  }
  std::cout << __FILE__ << ":" << __LINE__ << std::endl;
  try {
    IntPromise p(new boost::promise<int>());
    boost::thread t(boost::bind(foo, p));
    boost::future<int> f1 = p->get_future();
    f1.then(boost::launch::async, &bar);
    t.join();
  } catch(...) {
    return 2;
  }
  std::cout << __FILE__ << ":" << __LINE__ << std::endl;
}

