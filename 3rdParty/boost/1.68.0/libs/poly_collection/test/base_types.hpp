/* Copyright 2016-2018 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_TEST_BASE_TYPES_HPP
#define BOOST_POLY_COLLECTION_TEST_BASE_TYPES_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/poly_collection/base_collection.hpp>

namespace base_types{

struct base
{
  virtual ~base()=default;
  virtual int operator()(int)const=0;
};

struct derived1 final:base
{
  derived1(int n):n{n}{}
  derived1(derived1&&)=default;
  derived1(const derived1&)=delete;
  derived1& operator=(derived1&&)=default;
  derived1& operator=(const derived1&)=delete;
  virtual int operator()(int)const{return n;}
  bool operator==(const derived1& x)const{return n==x.n;}
  int n;
};

struct derived2:base
{
  derived2(int n):n{n}{}
  derived2(derived2&&)=default;
  derived2& operator=(derived2&&)=delete;
  virtual int operator()(int x)const{return x*n;}
  bool operator==(const derived2& x)const{return n==x.n;}
  int n;
};

struct derived3:base
{
  derived3():n{-1}{}
  derived3(int n):n{n}{}
  virtual int operator()(int x)const{return x*x*n;}
  int n;
};

struct another_base
{
  virtual ~another_base()=default;
  char x[5];
};

struct derived4:another_base,derived3
{
  using derived3::derived3;
  virtual int operator()(int x)const{return -(this->derived3::operator()(x));}
  bool operator==(const derived4& x)const{return n==x.n;}
};

struct derived5:base,another_base
{
  derived5(int n):n{n}{}
  virtual int operator()(int x)const{return x*x*x*n;}
  int n;
};

using collection=boost::base_collection<base>;

using t1=derived1;
using t2=derived2;
using t3=derived3;
using t4=derived4;
using t5=derived5;

struct to_int
{
  template<typename F>
  int operator()(const F& f)const{return f(1);}
};

} /* namespace base_types */

#endif
