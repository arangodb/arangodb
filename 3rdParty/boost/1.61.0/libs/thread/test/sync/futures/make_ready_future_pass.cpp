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

// future<void> make_ready_future();
// template <class T>
// future<decay_t<T>> make_ready_future(T&&);
// template <class T>
// future<T> make_ready_future(remove_reference_t<T>&);
// template <class T>
// future<T> make_ready_future(remove_reference_t<T>&&);
// template <class T, class ...Args>
// future<T> make_ready_future(Args&& ... args);

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
  BOOST_STATIC_ASSERT((boost::is_copy_constructible<A>::value == true));
  BOOST_STATIC_ASSERT((boost::has_move_emulation_enabled<A>::value == false));
#endif

  {
    boost::future<void> f = boost::make_ready_future();
    f.wait();
  }
  {
    typedef A T;
    T i;
    boost::future<T> f = boost::make_ready_future(i);
    BOOST_TEST(f.get().value==0);
  }
#if ! defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
  {
    typedef A T;
    boost::future<T> f = boost::make_ready_future<T>();
    BOOST_TEST(f.get().value==0);
  }
  {
    typedef A T;
    boost::future<T> f = boost::make_ready_future<T>(1);
    BOOST_TEST(f.get().value==1);
  }
  {
    typedef A T;
    boost::future<T> f = boost::make_ready_future<T>(1,2);
    BOOST_TEST(f.get().value==3);
  }
  {
    typedef A T;
    T i;
    boost::future<T&> f = boost::make_ready_future<T&>(i);
    BOOST_TEST(f.get().value==0);
  }
#endif
#if ! defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
// sync/futures/make_ready_future_pass.cpp:125:65: erreur: conversion from Ôboost::future<boost::rv<movable2> >Õ to non-scalar type Ôboost::future<movable2>Õ requested
  {
    typedef movable2 T;
    T i;
    boost::future<T> f = boost::make_ready_future(boost::move(i));
    BOOST_TEST_EQ(f.get().value(),1);
  }
#endif
#if ! defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
  {
    typedef movable2 T;
    boost::future<T> f = boost::make_ready_future<T>();
    BOOST_TEST(f.get().value()==1);
  }
  {
    typedef movable2 T;
    boost::future<T> f = boost::make_ready_future<T>(1);
    BOOST_TEST(f.get().value()==1);
  }
  {
    typedef movable2 T;
    boost::future<T> f = boost::make_ready_future<T>(1,2);
    BOOST_TEST(f.get().value()==3);
  }
#endif

  return boost::report_errors();
}

