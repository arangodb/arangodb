// Copyright (C) 2012-2013 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#define BOOST_THREAD_VERSION 4
//#define BOOST_THREAD_USES_LOG
#define BOOST_THREAD_USES_LOG_THREAD_ID

#include <boost/thread/detail/log.hpp>
#include <boost/thread/future.hpp>
#include <boost/assert.hpp>
#include <string>
#include <iostream>

#if defined BOOST_THREAD_PROVIDES_FUTURE_UNWRAP

int p1()
{
  BOOST_THREAD_LOG << "P1" << BOOST_THREAD_END_LOG;
  return 123;
}

boost::future<int> p2()
{
  BOOST_THREAD_LOG << "<P2" << BOOST_THREAD_END_LOG;
  boost::future<int> f1 = boost::async(boost::launch::async, &p1);
  BOOST_THREAD_LOG << "P2>" << BOOST_THREAD_END_LOG;
  return boost::move(f1);
}

int main()
{
  const int number_of_tests = 100;
  BOOST_THREAD_LOG << "<MAIN" << BOOST_THREAD_END_LOG;
  for (int i=0; i< number_of_tests; i++)
  try
  {

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    {
      boost::future<int> inner_future = boost::async(boost::launch::async, &p2);
      inner_future.wait();
      int ii = inner_future.get();
      BOOST_THREAD_LOG << "ii= "<< ii << "" << BOOST_THREAD_END_LOG;
    }
#endif
    {
      boost::future<boost::future<int> > outer_future = boost::async(boost::launch::async, &p2);
      boost::future<int> inner_future = outer_future.unwrap();
      inner_future.wait();
      int ii = inner_future.get();
      BOOST_THREAD_LOG << "ii= "<< ii << "" << BOOST_THREAD_END_LOG;
    }
    {
      boost::future<boost::future<int> > outer_future = boost::async(boost::launch::async, &p2);
      boost::future<int> inner_future = outer_future.unwrap();
      int ii = inner_future.get();
      BOOST_THREAD_LOG << "ii= "<< ii << "" << BOOST_THREAD_END_LOG;
    }
  }
  catch (std::exception& ex)
  {
    std::cout << "ERRORRRRR "<<ex.what() << "" << std::endl;
    BOOST_THREAD_LOG << "ERRORRRRR "<<ex.what() << "" << BOOST_THREAD_END_LOG;
    return 1;
  }
  catch (...)
  {
    std::cout << " ERRORRRRR exception thrown" << std::endl;
    BOOST_THREAD_LOG << " ERRORRRRR exception thrown" << BOOST_THREAD_END_LOG;
    return 2;
  }
  BOOST_THREAD_LOG << "MAIN>" << BOOST_THREAD_END_LOG;
  return 0;
}
#else

int main()
{
  return 0;
}
#endif
