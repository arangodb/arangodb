/* Copyright 2016-2018 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_TEST_FUNCTION_TYPES_HPP
#define BOOST_POLY_COLLECTION_TEST_FUNCTION_TYPES_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/poly_collection/function_collection.hpp>
#include <typeinfo>

namespace function_types{

struct function1 final
{
  function1(int n):n{n}{}
  function1(function1&&)=default;
  function1(const function1&)=delete;
  function1& operator=(function1&&)=default;
  function1& operator=(const function1&)=delete;
  int operator()(int)const{return n;}
  friend bool operator==(
    const function1& x,const function1& y){return x.n==y.n;}
  int n;
};

struct function2
{
  function2(int n):n{n}{}
  int operator()(int x)const{return x*n;}
  bool operator==(const function2& x)const{return n==x.n;}
  int n;
};

struct function3
{
  function3():n{-1}{}
  function3(int n):n{n}{}
  int operator()(int x)const{return x*x*n;}
  int n;
};

struct function4:function3
{
  using function3::function3;
  int operator()(int x)const{return -(this->function3::operator()(x));}
  bool operator==(const function4& x)const{return n==x.n;}
};

struct function5
{
  function5(int n):n{n}{}
  int operator()(int x)const{return x*x*x*n;}
  int n;
};

struct int_alias /* brings this namespace into ADL for operator== below */
{
  int_alias(int n):n{n}{}
  operator int()const{return n;}
  int n;
};

using signature=int_alias(int);
using collection=boost::function_collection<signature>;

using t1=function1;
using t2=function2;
using t3=function3;
using t4=function4;
using t5=function5;

inline bool operator==(
  const collection::value_type& x,const collection::value_type& y)
{
  const std::type_info& xi=x.target_type();
  const std::type_info& yi=y.target_type();
  if(xi==yi){
    if(xi==typeid(t1))return (*x.target<t1>())==(*y.target<t1>());
    if(xi==typeid(t2))return (*x.target<t2>()).operator==(*y.target<t2>());
    if(xi==typeid(t4))return (*x.target<t4>()).operator==(*y.target<t4>());
  }
  return false;
}

inline bool operator==(const collection::value_type& x,const t1& y)
{
  const std::type_info& xi=x.target_type();
  if(xi==typeid(t1))return (*x.target<t1>())==y;
  return false;
}

inline bool operator==(const t1& x,const collection::value_type& y)
{
  return y==x;
}

inline bool operator==(const collection::value_type& x,const t2& y)
{
  const std::type_info& xi=x.target_type();
  if(xi==typeid(t2))return (*x.target<t2>())==y;
  return false;
}

inline bool operator==(const t2& x,const collection::value_type& y)
{
  return y==x;
}

inline bool operator==(const collection::value_type& x,const t4& y)
{
  const std::type_info& xi=x.target_type();
  if(xi==typeid(t4))return (*x.target<t4>())==y;
  return false;
}

inline bool operator==(const t4& x,const collection::value_type& y)
{
  return y==x;
}

inline bool operator==(const t1&,const t2&){return false;}
inline bool operator==(const t1&,const t4&){return false;}
inline bool operator==(const t2&,const t1&){return false;}
inline bool operator==(const t2&,const t4&){return false;}
inline bool operator==(const t4&,const t1&){return false;}
inline bool operator==(const t4&,const t2&){return false;}

struct to_int
{
  template<typename F>
  int operator()(const F& f)const{return f(1);}
};

} /* namespace function_types */

#endif
