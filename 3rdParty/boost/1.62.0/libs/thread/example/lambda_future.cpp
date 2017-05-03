// Copyright (C) 2013 Vicente Botet
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

#if    defined BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION \
  && ! defined BOOST_NO_CXX11_LAMBDAS && ! (defined BOOST_MSVC && _MSC_VER < 1700)


int main()
{
  const int number_of_tests = 100;
  BOOST_THREAD_LOG << "<MAIN" << BOOST_THREAD_END_LOG;

  for (int i=0; i< number_of_tests; i++)
  try
  {
    {
      boost::future<int> f1 = boost::async(boost::launch::async, []()  {return 123;});
      int result = f1.get();
      BOOST_THREAD_LOG << "f1 " << result << BOOST_THREAD_END_LOG;
    }
    {
      boost::future<int> f1 = boost::async(boost::launch::async, []() {return 123;});
      boost::future<int> f2 = f1.then([](boost::future<int> f)  {return 2*f.get(); });
      int result = f2.get();
      BOOST_THREAD_LOG << "f2 " << result << BOOST_THREAD_END_LOG;
    }
#if ! defined    BOOST_NO_CXX14_GENERIC_LAMBDAS
    {
      boost::future<int> f1 = boost::async(boost::launch::async, []() {return 123;});
      boost::future<int> f2 = f1.then([](auto f)  {return 2*f.get(); });
      int result = f2.get();
      BOOST_THREAD_LOG << "f2 " << result << BOOST_THREAD_END_LOG;
    }
#endif
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
