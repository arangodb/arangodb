//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Copyright (C) 2011 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/future.hpp>

// class promise<R>

// void set_exception(exception_ptr p);

#define BOOST_THREAD_VERSION 3

#include <boost/thread/future.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/static_assert.hpp>

  template <typename T>
  struct wrap
  {
    wrap(T const& v) :
      value(v)
    {
    }
    T value;

  };

  template <typename T>
  boost::exception_ptr make_exception_ptr(T v)
  {
    return boost::copy_exception(wrap<T> (v));
  }

int main()
{

  {
    typedef int T;
    boost::promise<T> p;
    boost::future<T> f = p.get_future();
    p.set_exception(::make_exception_ptr(3));
    try
    {
      f.get();
      BOOST_TEST(false);
    }
    catch (::wrap<int> i)
    {
      BOOST_TEST(i.value == 3);
    }
    try
    {
      p.set_exception(::make_exception_ptr(3));
      BOOST_TEST(false);
    }
    catch (const boost::future_error& e)
    {
      BOOST_TEST(e.code() == boost::system::make_error_code(boost::future_errc::promise_already_satisfied));
    }
    catch (...)
    {
      BOOST_TEST(false);
    }
  }
  {
    typedef int T;
    boost::promise<T> p;
    boost::future<T> f = p.get_future();
    p.set_exception_deferred(::make_exception_ptr(3));
    BOOST_TEST(!f.is_ready());
    p.notify_deferred();
    try
    {
      f.get();
      BOOST_TEST(false);
    }
    catch (::wrap<int> i)
    {
      BOOST_TEST(i.value == 3);
    }
    try
    {
      p.set_exception(::make_exception_ptr(3));
      BOOST_TEST(false);
    }
    catch (const boost::future_error& e)
    {
      BOOST_TEST(e.code() == boost::system::make_error_code(boost::future_errc::promise_already_satisfied));
    }
    catch (...)
    {
      BOOST_TEST(false);
    }
  }
  return boost::report_errors();
}

