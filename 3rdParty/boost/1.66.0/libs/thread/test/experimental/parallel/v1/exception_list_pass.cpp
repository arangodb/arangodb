// Copyright (C) 2014 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/experimental/parallel/v1/exception_list.hpp>


#define BOOST_THREAD_VERSION 4
#include <boost/config.hpp>
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#include <boost/thread/experimental/parallel/v1/exception_list.hpp>

#include <boost/detail/lightweight_test.hpp>


int main()
{
  return boost::report_errors();
}
