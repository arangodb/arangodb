//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Copyright (C) 2011,2014 Vicente J. Botet Escriba
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// <boost/thread/future.hpp>

// class promise<R>

// template <class ...Args>
// void promise::emplace(Args&& ... args);

#define BOOST_THREAD_VERSION 3

#include <boost/thread/future.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/static_assert.hpp>

struct A
{
  A() :
    value(0)
  {
  }
  A(int i) :
    value(i)
  {
  }
  A(int i, int j) :
    value(i+j)
  {
  }
  BOOST_THREAD_MOVABLE_ONLY(A)

  A(BOOST_THREAD_RV_REF(A) rhs)
  {
    if(rhs.value==0)
    throw 9;
    else
    {
      value=rhs.value;
      rhs.value=0;
    }
  }
  A& operator=(BOOST_THREAD_RV_REF(A) rhs)
  {
    if(rhs.value==0)
    throw 9;
    else
    {
      value=rhs.value;
      rhs.value=0;
    }
    return *this;
  }
  int value;
};

A make(int i) {
  return A(i);
}
A make(int i, int j) {
  return A(i, j);
}

struct movable2
{
   int value_;
   BOOST_THREAD_MOVABLE_ONLY(movable2)
   movable2() : value_(1){}
   movable2(int i) : value_(i){}
   movable2(int i, int j) : value_(i+j){}

   //Move constructor and assignment
   movable2(BOOST_RV_REF(movable2) m)
   {  value_ = m.value_;   m.value_ = 0;  }

   movable2 & operator=(BOOST_THREAD_RV_REF(movable2) m)
   {  value_ = m.value_;   m.value_ = 0;  return *this;  }

   bool moved() const //Observer
   {  return !value_; }

   int value() const //Observer
   {  return value_; }
};


movable2 move_return_function2(int i) {
  return movable2(i);
}

int main()
{
#if defined  BOOST_NO_CXX11_RVALUE_REFERENCES
  BOOST_STATIC_ASSERT((boost::is_copy_constructible<movable2>::value == false));
  BOOST_STATIC_ASSERT((boost::has_move_emulation_enabled<movable2>::value == true));
  BOOST_STATIC_ASSERT((boost::is_copy_constructible<A>::value == false));
  BOOST_STATIC_ASSERT((boost::has_move_emulation_enabled<A>::value == true));
#endif
#if ! defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

  {
    typedef A T;
    T i;
    boost::promise<T> p;
    boost::future<T> f = p.get_future();
    p.emplace();
    try
    {
      T a = f.get(); (void)a;
      BOOST_TEST(false);
    }
    catch (int j)
    {
      BOOST_TEST(j == 9);
    }
    catch (...)
    {
      BOOST_TEST(false);
    }
  }
  {
    typedef A T;
    boost::promise<T> p;
    boost::future<T> f = p.get_future();
    p.emplace(3);
    BOOST_TEST(f.get().value == 3);
    try
    {
      T j(3);
      p.set_value(boost::move(j));
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
    boost::promise<movable2> p;
    boost::future<movable2> f = p.get_future();
    p.emplace(3);
    BOOST_TEST(f.get().value_ == 3);
    try
    {
      p.emplace(3);
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
    boost::promise<A> p;
    boost::future<A> f = p.get_future();
    p.emplace(1,2);
    BOOST_TEST(f.get().value == 3);
    try
    {
      p.emplace(1,2);
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
    typedef A T;
    boost::promise<T> p;
    boost::future<T> f = p.get_future();
    p.emplace(3);
    boost::promise<T> p2(boost::move(p));
    BOOST_TEST(f.get().value == 3);

  }
#endif

  return boost::report_errors();
}

