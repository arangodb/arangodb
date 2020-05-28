/* Copyright 2016-2018 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_TEST_ANY_TYPES_HPP
#define BOOST_POLY_COLLECTION_TEST_ANY_TYPES_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/mpl/vector/vector10.hpp>
#include <boost/poly_collection/any_collection.hpp>
#include <boost/type_erasure/any_cast.hpp>
#include <boost/type_erasure/builtin.hpp>
#include <boost/type_erasure/call.hpp>
#include <boost/type_erasure/operators.hpp>

namespace any_types{

struct incrementable1
{
  incrementable1(int n):n{n}{}
  incrementable1(incrementable1&&)=default;
  incrementable1(const incrementable1&)=delete;
  incrementable1& operator=(incrementable1&&)=default;
  incrementable1& operator=(const incrementable1&)=delete;
  bool operator==(const incrementable1& x)const{return n==x.n;}
  incrementable1& operator++(){++n;return *this;}
  int n;
};

struct incrementable3
{
  incrementable3():n{-1}{}
  incrementable3(int n):n{(double)n}{}
  incrementable3& operator++(){++n;return *this;}
  double n;
};

using concept_=boost::type_erasure::incrementable<>;
using collection=boost::any_collection<concept_>;

template<typename T=boost::type_erasure::_self>
struct convertible_to_int
{
  static int apply(const T& x){return x;}
};

using t1=incrementable1;
using t2=double;
using t3=incrementable3;
using t4=int;
using t5=boost::type_erasure::any<
  boost::mpl::vector4<
    boost::type_erasure::copy_constructible<>,
    boost::type_erasure::assignable<>,
    concept_,
    convertible_to_int<>
  >
>;

struct to_int
{
  template<typename Concept,typename Tag>
  int operator()(const boost::type_erasure::any<Concept,Tag>& x)const
  {
    using boost::type_erasure::any_cast;

    if(auto p=any_cast<t1*>(&x))return (*this)(*p);
    if(auto p=any_cast<t2*>(&x))return (*this)(*p);
    if(auto p=any_cast<t3*>(&x))return (*this)(*p);
    if(auto p=any_cast<t4*>(&x))return (*this)(*p);
    if(auto p=any_cast<t5*>(&x))return (*this)(*p);
    else return 0;
  }

  int operator()(const t1& x)const{return x.n;}
  int operator()(const t2& x)const{return static_cast<int>(x);};
  int operator()(const t3& x)const{return static_cast<int>(x.n);}
  int operator()(const t4& x)const{return x;}
  int operator()(const t5& x)const
  {
    return boost::type_erasure::call(convertible_to_int<>{},x);
  }
};

} /* namespace any_types */

#endif
