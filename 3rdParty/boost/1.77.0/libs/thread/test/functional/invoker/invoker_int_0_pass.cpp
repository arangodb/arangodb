//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Copyright (C) 2014 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/detail/invoker.hpp>

#include <boost/thread/detail/invoker.hpp>
#include <boost/detail/lightweight_test.hpp>

int f()
{
  return 1;
}

struct A_int_0
{
  A_int_0()
  {
  }
  int operator()()
  {
    return 4;
  }
  int operator()() const
  {
    return 5;
  }
};


int main()
{
  const A_int_0 ca;
  A_int_0 a;

#if defined BOOST_THREAD_PROVIDES_INVOKE
  BOOST_TEST_EQ(boost::detail::invoker<int(*)()>(f)(), 1);
  BOOST_TEST_EQ(boost::detail::invoker<int(*)()>(&f)(), 1);
  BOOST_TEST_EQ(boost::detail::invoker<A_int_0>(A_int_0())(), 4);
  BOOST_TEST_EQ(boost::detail::invoker<A_int_0>(a)(), 4);
  BOOST_TEST_EQ(boost::detail::invoker<const A_int_0>(ca)(), 5);
#endif

  //BOOST_TEST_EQ(boost::detail::invoker<int>(f), 1);
  //BOOST_TEST_EQ(boost::detail::invoker<int>(&f), 1);
  BOOST_TEST_EQ(A_int_0()(), 4);
#if defined BOOST_THREAD_PROVIDES_INVOKE || ! defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
  //BOOST_TEST_EQ(boost::detail::invoker<int>(A_int_0()), 4);
#else
  //BOOST_TEST_EQ(boost::detail::invoke<int>(A_int_0()), 5);
#endif
  //BOOST_TEST_EQ(a(), 4);
  //BOOST_TEST_EQ(boost::detail::invoke<int>(a), 4);
  //BOOST_TEST_EQ(ca(), 5);
  //BOOST_TEST_EQ(boost::detail::invoke<int>(ca), 5);

  return boost::report_errors();
}
