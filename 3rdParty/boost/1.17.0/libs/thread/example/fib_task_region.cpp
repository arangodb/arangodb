// Copyright (C) 2012 Vicente Botet
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>
#if ! defined  BOOST_NO_CXX11_DECLTYPE
#define BOOST_RESULT_OF_USE_DECLTYPE
#endif

#define BOOST_THREAD_VERSION 4
#define BOOST_THREAD_PROVIDES_EXECUTORS

#include <boost/thread/experimental/task_region.hpp>
#include <iostream>

#if ! defined BOOST_NO_CXX11_RANGE_BASED_FOR &&  ! defined BOOST_NO_CXX11_LAMBDAS

int fib_task_region(int n)
{
  using boost::experimental::parallel::task_region;
  using boost::experimental::parallel::task_region_handle;

  if (n == 0) return 0;
  if (n == 1) return 1;

  int n1;
  int n2;

  task_region([&](task_region_handle& trh)
      {
        trh.run([&]
            {
              n1 = fib_task_region(n - 1);
            });

        n2 = fib_task_region(n - 2);
      });

  return n1 + n2;
}

#if defined BOOST_THREAD_PROVIDES_EXECUTORS
template <class Ex>
int fib_task_region_gen( Ex& ex, int n)
{
  using boost::experimental::parallel::task_region;
  using boost::experimental::parallel::task_region_handle_gen;

  if (n == 0) return 0;
  if (n == 1) return 1;

  int n1;
  int n2;

  task_region(ex, [&](task_region_handle_gen<Ex>& trh)
      {
        trh.run([&]
            {
              n1 = fib_task_region(n - 1);
            });

        n2 = fib_task_region(n - 2);
      });

  return n1 + n2;
}
#endif

int main()
{
  for (int i = 0; i<10; ++i) {
    std::cout << fib_task_region(i) << " ";
  }
  std::cout << std::endl;

#if defined BOOST_THREAD_PROVIDES_EXECUTORS
  boost::basic_thread_pool tp;
  for (int i = 0; i<10; ++i) {
    std::cout << fib_task_region_gen(tp,i) << " ";
  }
  std::cout << std::endl;
#endif
  return 0;
}
#else
int main()
{
  return 0;
}
#endif
