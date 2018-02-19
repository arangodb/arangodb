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
#include <exception>
#include <string>
#include <iostream>

#if defined BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION

#ifdef BOOST_MSVC
#pragma warning(disable: 4127) // conditional expression is constant
#endif

int p1_ex()
{
  BOOST_THREAD_LOG << "P1" << BOOST_THREAD_END_LOG;
  throw std::logic_error("kk");
}

int p1()
{
  BOOST_THREAD_LOG << "P1" << BOOST_THREAD_END_LOG;
  return 1;;
}

int main()
{
  const int number_of_tests = 200;
  BOOST_THREAD_LOG << "<MAIN" << BOOST_THREAD_END_LOG;

  {
    for (int i=0; i< number_of_tests; i++)
    try
    {
      BOOST_THREAD_LOG << "" << BOOST_THREAD_END_LOG;
      boost::future<int> f1 = boost::async(boost::launch::async, &p1);
      BOOST_THREAD_LOG << "" << BOOST_THREAD_END_LOG;
      f1.wait();
      BOOST_ASSERT(f1.get()==1);
      BOOST_THREAD_LOG << "" << BOOST_THREAD_END_LOG;
    }
    catch (std::exception& ex)
    {
      std::cout << __FILE__ << "["<< __LINE__<<"] " << "ERRORRRRR "<<ex.what() << "" << std::endl;
      BOOST_THREAD_LOG << "ERRORRRRR "<<ex.what() << "" << BOOST_THREAD_END_LOG;
      return 1;
    }
    catch (...)
    {
      std::cout << __FILE__ << "["<< __LINE__<<"] " << " ERRORRRRR exception thrown" << std::endl;
      BOOST_THREAD_LOG << " ERRORRRRR exception thrown" << BOOST_THREAD_END_LOG;
      return 2;
    }
  }

  {
    for (int i=0; i< number_of_tests; i++)
    try
    {
      BOOST_THREAD_LOG << "" << BOOST_THREAD_END_LOG;
      boost::future<int> f1 = boost::async(&p1);
      BOOST_THREAD_LOG << "" << BOOST_THREAD_END_LOG;
      boost::future<int> f2 = f1.fallback_to(-1);
      BOOST_THREAD_LOG << "" << BOOST_THREAD_END_LOG;
      f2.wait();
      //std::cout << __FILE__ << "["<< __LINE__<<"] " << std::endl;
      BOOST_ASSERT(f2.get()==1);
      //std::cout << __FILE__ << "["<< __LINE__<<"] " << std::endl;
      BOOST_THREAD_LOG << "" << BOOST_THREAD_END_LOG;
    }
    catch (std::exception& ex)
    {
      std::cout << __FILE__ << "["<< __LINE__<<"] " << "ERRORRRRR "<<ex.what() << "" << std::endl;
      BOOST_THREAD_LOG << "ERRORRRRR "<<ex.what() << "" << BOOST_THREAD_END_LOG;
      return 1;
    }
    catch (...)
    {
      std::cout << __FILE__ << "["<< __LINE__<<"] " << " ERRORRRRR exception thrown" << std::endl;
      BOOST_THREAD_LOG << " ERRORRRRR exception thrown" << BOOST_THREAD_END_LOG;
      return 2;
    }
  }

  {
    for (int i=0; i< number_of_tests; i++)
    try
    {
      BOOST_THREAD_LOG << "" << BOOST_THREAD_END_LOG;
      boost::future<int> f1 = boost::async(boost::launch::async, &p1_ex);
      BOOST_THREAD_LOG << "" << BOOST_THREAD_END_LOG;
      f1.wait();
      BOOST_ASSERT(f1.get_or(-1)==-1);
      BOOST_THREAD_LOG << "" << BOOST_THREAD_END_LOG;
    }
    catch (std::exception& ex)
    {
      std::cout << __FILE__ << "["<< __LINE__<<"] " << "ERRORRRRR "<<ex.what() << "" << std::endl;
      BOOST_THREAD_LOG << "ERRORRRRR "<<ex.what() << "" << BOOST_THREAD_END_LOG;
      return 1;
    }
    catch (...)
    {
      std::cout << __FILE__ << "["<< __LINE__<<"] " << " ERRORRRRR exception thrown" << std::endl;
      BOOST_THREAD_LOG << " ERRORRRRR exception thrown" << BOOST_THREAD_END_LOG;
      return 2;
    }
  }

  {
    for (int i=0; i< number_of_tests; i++)
    try
    {
      BOOST_THREAD_LOG << "" << BOOST_THREAD_END_LOG;
      boost::future<int> f1 = boost::async(boost::launch::async, &p1_ex);
      BOOST_THREAD_LOG << "" << BOOST_THREAD_END_LOG;
      boost::future<int> f2 = f1.fallback_to(-1);
      BOOST_THREAD_LOG << "" << BOOST_THREAD_END_LOG;
      f2.wait();
      //std::cout << __FILE__ << "["<< __LINE__<<"] " << std::endl;
      BOOST_ASSERT(f2.get()==-1);
      //std::cout << __FILE__ << "["<< __LINE__<<"] " << std::endl;
      BOOST_THREAD_LOG << "" << BOOST_THREAD_END_LOG;
    }
    catch (std::exception& ex)
    {
      std::cout << __FILE__ << "["<< __LINE__<<"] " << "ERRORRRRR "<<ex.what() << "" << std::endl;
      BOOST_THREAD_LOG << "ERRORRRRR "<<ex.what() << "" << BOOST_THREAD_END_LOG;
      return 1;
    }
    catch (...)
    {
      std::cout << __FILE__ << "["<< __LINE__<<"] " << " ERRORRRRR exception thrown" << std::endl;
      BOOST_THREAD_LOG << " ERRORRRRR exception thrown" << BOOST_THREAD_END_LOG;
      return 2;
    }
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
