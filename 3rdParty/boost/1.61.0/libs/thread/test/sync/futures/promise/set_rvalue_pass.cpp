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

// void promise::set_value(R&& r);

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

struct movable2
{
   int value_;
   BOOST_THREAD_MOVABLE_ONLY(movable2)
   movable2() : value_(1){}
   movable2(int i) : value_(i){}

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

  {
    typedef A T;
    T i;
    boost::promise<T> p;
    boost::future<T> f = p.get_future();
    try
    {
      p.set_value(boost::move(i));
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
    T i;
    boost::promise<T> p;
    boost::future<T> f = p.get_future();
    try
    {
      p.set_value((T()));
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
    T i(3);
    boost::promise<T> p;
    boost::future<T> f = p.get_future();
    p.set_value(boost::move(i));
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
    movable2 i(3);
    boost::promise<movable2> p;
    boost::future<movable2> f = p.get_future();
    p.set_value(move_return_function2(3));
    BOOST_TEST(f.get().value_ == 3);
    try
    {
      movable2 j(3);
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
    boost::promise<A> p;
    boost::future<A> f = p.get_future();
    p.set_value(make(3));
    BOOST_TEST(f.get().value == 3);
    try
    {
      A j(3);
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
    typedef A T;
    T i(3);
    boost::promise<T> p;
    boost::future<T> f = p.get_future();
    p.set_value(boost::move(i));
    BOOST_TEST(i.value == 0);
    boost::promise<T> p2(boost::move(p));
    BOOST_TEST(f.get().value == 3);

  }
  {
    typedef A T;
    T i(3);
    boost::promise<T> p;
    boost::future<T> f = p.get_future();
    p.set_value(boost::move(i));
    BOOST_TEST(i.value == 0);
    boost::promise<T> p2(boost::move(p));
    boost::future<T> f2(boost::move(f));
    BOOST_TEST(f2.get().value == 3);

  }
  {
    typedef A T;
    T i(3);
    boost::promise<T> p;
    p.set_value(boost::move(i));
    BOOST_TEST(i.value == 0);
    boost::promise<T> p2(boost::move(p));
    boost::future<T> f = p2.get_future();
    BOOST_TEST(f.get().value == 3);

  }

  {
    typedef boost::future<int> T;
    boost::promise<int> pi;
    T fi=pi.get_future();
    pi.set_value(3);

    boost::promise<T> p;
    boost::future<T> f = p.get_future();
    p.set_value(boost::move(fi));
    boost::future<T> f2(boost::move(f));
    BOOST_TEST(f2.get().get() == 3);
  }

  return boost::report_errors();
}

