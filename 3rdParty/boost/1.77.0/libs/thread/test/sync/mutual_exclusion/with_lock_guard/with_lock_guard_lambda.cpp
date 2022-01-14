// (C) Copyright 2013 Ruslan Baratov
// Copyright (C) 2014 Vicente J. Botet Escriba
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  See www.boost.org/libs/thread for documentation.

#include <boost/config.hpp>

#if !defined(BOOST_NO_CXX11_DECLTYPE)
# define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#define BOOST_THREAD_VERSION 4

#include <boost/detail/lightweight_test.hpp> // BOOST_TEST

#include <iostream> // std::cout
#include <boost/thread/mutex.hpp>
#include <boost/thread/with_lock_guard.hpp>

#if defined(BOOST_NO_CXX11_LAMBDAS) || (defined BOOST_MSVC && _MSC_VER < 1700)
void test_lambda() {
  std::cout << "C++11 lambda disabled" << std::endl;
}
#else
void test_lambda() {
  boost::mutex m;
  int res_1 = boost::with_lock_guard(
      m,
      [](int a) {
        BOOST_TEST(a == 13);
        return a + 3;
      },
      13
  );
  BOOST_TEST(res_1 == 16);

  int v = 0;
  int res_2 = boost::with_lock_guard(
      m,
      [&v](int a) {
        BOOST_TEST(a == 55);
        v = 15;
        return 45;
      },
      55
  );
  BOOST_TEST(res_2 == 45);
  BOOST_TEST(v == 15);
}
#endif

int main() {
  std::cout << std::boolalpha;
  test_lambda();
  return boost::report_errors();
}
