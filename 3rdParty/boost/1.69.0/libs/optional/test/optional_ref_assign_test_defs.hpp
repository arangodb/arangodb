// Copyright (C) 2014 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at: akrzemi1@gmail.com

#ifndef BOOST_OPTIONAL_TEST_OPTIONAL_REF_ASSIGN_TEST_DEFS_AK_07JAN2015_HPP
#define BOOST_OPTIONAL_TEST_OPTIONAL_REF_ASSIGN_TEST_DEFS_AK_07JAN2015_HPP

#include "boost/optional/optional.hpp"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#include "boost/core/addressof.hpp"
#include "testable_classes.hpp"

using boost::optional;
using boost::none;
using boost::addressof;

template <typename T>
void test_copy_assignment_for_const()
{
  const typename concrete_type_of<T>::type v(2);
  optional<const T&> o;
  o = optional<const T&>(v);
  
  BOOST_TEST(o);
  BOOST_TEST(o != none);
  BOOST_TEST(addressof(*o) == addressof(v));
  BOOST_TEST(val(*o) == val(v));
  BOOST_TEST(val(*o) == 2);
}

template <typename T>
void test_copy_assignment_for_noconst_const()
{
  typename concrete_type_of<T>::type v(2);
  optional<const T&> o;
  o = optional<const T&>(v);
  
  BOOST_TEST(o);
  BOOST_TEST(o != none);
  BOOST_TEST(addressof(*o) == addressof(v));
  BOOST_TEST(val(*o) == val(v));
  BOOST_TEST(val(*o) == 2);
  
  val(v) = 9;
  BOOST_TEST(addressof(*o) == addressof(v));
  BOOST_TEST_EQ(val(*o), val(v));
  BOOST_TEST_EQ(val(*o), 9);
  BOOST_TEST_EQ(val(v), 9);
}

template <typename T>
void test_copy_assignment_for()
{
  typename concrete_type_of<T>::type v(2);
  optional<T&> o;
  o = optional<T&>(v);
  
  BOOST_TEST(o);
  BOOST_TEST(o != none);
  BOOST_TEST(addressof(*o) == addressof(v));
  BOOST_TEST(val(*o) == val(v));
  BOOST_TEST(val(*o) == 2);
  
  val(v) = 9;
  BOOST_TEST(addressof(*o) == addressof(v));
  BOOST_TEST_EQ(val(*o), val(v));
  BOOST_TEST_EQ(val(*o), 9);
  BOOST_TEST_EQ(val(v), 9);
  
  val(*o) = 7;
  BOOST_TEST(addressof(*o) == addressof(v));
  BOOST_TEST_EQ(val(*o), val(v));
  BOOST_TEST_EQ(val(*o), 7);
  BOOST_TEST_EQ(val(v), 7);
}

template <typename T>
void test_rebinding_assignment_semantics_const()
{
  const typename concrete_type_of<T>::type v(2), w(7);
  optional<const T&> o(v);
  
  BOOST_TEST(o);
  BOOST_TEST(addressof(*o) == addressof(v));
  BOOST_TEST_EQ(val(*o), val(v));
  BOOST_TEST_EQ(val(*o), 2);
  
  o = optional<const T&>(w);
  BOOST_TEST_EQ(val(v), 2);
  
  BOOST_TEST(o);
  BOOST_TEST(addressof(*o) != addressof(v));
  BOOST_TEST_NE(val(*o), val(v));
  BOOST_TEST_NE(val(*o), 2);
  
  BOOST_TEST(addressof(*o) == addressof(w));
  BOOST_TEST_EQ(val(*o), val(w));
  BOOST_TEST_EQ(val(*o), 7);
}

template <typename T>
void test_rebinding_assignment_semantics_noconst_const()
{
  typename concrete_type_of<T>::type v(2), w(7);
  optional<const T&> o(v);
  
  BOOST_TEST(o);
  BOOST_TEST(addressof(*o) == addressof(v));
  BOOST_TEST_EQ(val(*o), val(v));
  BOOST_TEST_EQ(val(*o), 2);
  
  o = optional<const T&>(w);
  BOOST_TEST_EQ(val(v), 2);
  
  BOOST_TEST(o);
  BOOST_TEST(addressof(*o) != addressof(v));
  BOOST_TEST_NE(val(*o), val(v));
  BOOST_TEST_NE(val(*o), 2);
  
  BOOST_TEST(addressof(*o) == addressof(w));
  BOOST_TEST_EQ(val(*o), val(w));
  BOOST_TEST_EQ(val(*o), 7);
}

template <typename T>
void test_rebinding_assignment_semantics()
{
  typename concrete_type_of<T>::type v(2), w(7);
  optional<T&> o(v);
  
  BOOST_TEST(o);
  BOOST_TEST(addressof(*o) == addressof(v));
  BOOST_TEST_EQ(val(*o), val(v));
  BOOST_TEST_EQ(val(*o), 2);
  
  o = optional<T&>(w);
  BOOST_TEST_EQ(val(v), 2);
  
  BOOST_TEST(o);
  BOOST_TEST(addressof(*o) != addressof(v));
  BOOST_TEST_NE(val(*o), val(v));
  BOOST_TEST_NE(val(*o), 2);
  
  BOOST_TEST(addressof(*o) == addressof(w));
  BOOST_TEST_EQ(val(*o), val(w));
  BOOST_TEST_EQ(val(*o), 7);
  
  val(*o) = 8;
  BOOST_TEST(addressof(*o) == addressof(w));
  BOOST_TEST_EQ(val(*o), val(w));
  BOOST_TEST_EQ(val(*o), 8);
  BOOST_TEST_EQ(val(w), 8);
  BOOST_TEST_EQ(val(v), 2);
}

template <typename T, typename U>
void test_converting_assignment()
{
  typename concrete_type_of<T>::type v1(1), v2(2), v3(3);
  optional<U&> oA(v1), oB(none);
  
  oA = v2;
  BOOST_TEST(oA);
  BOOST_TEST(addressof(*oA) == addressof(v2));
  
  oB = v3;
  BOOST_TEST(oB);
  BOOST_TEST(addressof(*oB) == addressof(v3));
}

#endif //BOOST_OPTIONAL_TEST_OPTIONAL_REF_ASSIGN_TEST_DEFS_AK_07JAN2015_HPP
