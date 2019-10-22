// Copyright 2003 David Abrahams and Jeremy Siek
// Copyright 2019 Hans Dembinski
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HISTOGRAM_TEST_ITERATOR_TESTS_HPP
#define BOOST_HISTOGRAM_TEST_ITERATOR_TESTS_HPP

// This file contains adapted code from
// - boost::core::iterator; boost/pending/iterator_tests.hpp
// - boost::core::conversion; boost/implicit_cast.hpp

#include <boost/core/ignore_unused.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iterator>
#include <type_traits>

namespace boost {
namespace histogram {

namespace detail {
template <class T>
struct icast_identity {
  typedef T type;
};
} // namespace detail

template <typename T>
inline T implicit_cast(typename detail::icast_identity<T>::type x) {
  return x;
}

// use this for the value type
struct dummyT {
  dummyT() {}
  dummyT(char) {}
  dummyT(int x) : m_x(x) {}
  int foo() const { return m_x; }
  bool operator==(const dummyT& d) const { return m_x == d.m_x; }
  int m_x;
};

// Tests whether type Iterator satisfies the requirements for a
// TrivialIterator.
// Preconditions: i != j, *i == val
template <class Iterator, class T>
void trivial_iterator_test(const Iterator i, const Iterator j, T val) {
  Iterator k;
  BOOST_TEST(i == i);
  BOOST_TEST(j == j);
  BOOST_TEST(i != j);
  typename std::iterator_traits<Iterator>::value_type v = *i;
  BOOST_TEST(v == val);
  ignore_unused(v);
  BOOST_TEST(v == i->foo());
  k = i;
  BOOST_TEST(k == k);
  BOOST_TEST(k == i);
  BOOST_TEST(k != j);
  BOOST_TEST(*k == val);
  ignore_unused(k);
}

// Preconditions: i != j
template <class Iterator, class T>
void mutable_trivial_iterator_test(const Iterator i, const Iterator j, T val) {
  *i = val;
  trivial_iterator_test(i, j, val);
}

// Preconditions: *i == v1, *++i == v2
template <class Iterator, class T>
void input_iterator_test(Iterator i, T v1, T v2) {
  Iterator i1(i);

  BOOST_TEST(i == i1);
  BOOST_TEST(!(i != i1));

  // I can see no generic way to create an input iterator
  // that is in the domain of== of i and != i.
  // The following works for istream_iterator but is not
  // guaranteed to work for arbitrary input iterators.
  //
  //   Iterator i2;
  //
  //   BOOST_TEST(i != i2);
  //   BOOST_TEST(!(i == i2));

  BOOST_TEST(*i1 == v1);
  BOOST_TEST(*i == v1);

  // we cannot test for equivalence of (void)++i & (void)i++
  // as i is only guaranteed to be single pass.
  BOOST_TEST(*i++ == v1);
  ignore_unused(i1);

  i1 = i;

  BOOST_TEST(i == i1);
  BOOST_TEST(!(i != i1));

  BOOST_TEST(*i1 == v2);
  BOOST_TEST(*i == v2);
  ignore_unused(i1);

  // i is dereferencable, so it must be incrementable.
  ++i;

  // how to test for operator-> ?
}

template <class Iterator, class T>
void forward_iterator_test(Iterator i, T v1, T v2) {
  input_iterator_test(i, v1, v2);

  Iterator i1 = i, i2 = i;

  BOOST_TEST(i == i1++);
  BOOST_TEST(i != ++i2);

  trivial_iterator_test(i, i1, v1);
  trivial_iterator_test(i, i2, v1);

  ++i;
  BOOST_TEST(i == i1);
  BOOST_TEST(i == i2);
  ++i1;
  ++i2;

  trivial_iterator_test(i, i1, v2);
  trivial_iterator_test(i, i2, v2);

  typedef typename std::iterator_traits<Iterator>::reference reference;
  typedef typename std::iterator_traits<Iterator>::value_type value_type;
  BOOST_TEST(std::is_reference<reference>::value);
  BOOST_TEST((std::is_same<reference, value_type&>::value ||
              std::is_same<reference, const value_type&>::value));
}

// Preconditions: *i == v1, *++i == v2
template <class Iterator, class T>
void bidirectional_iterator_test(Iterator i, T v1, T v2) {
  forward_iterator_test(i, v1, v2);
  ++i;

  Iterator i1 = i, i2 = i;

  BOOST_TEST(i == i1--);
  BOOST_TEST(i != --i2);

  trivial_iterator_test(i, i1, v2);
  trivial_iterator_test(i, i2, v2);

  --i;
  BOOST_TEST(i == i1);
  BOOST_TEST(i == i2);
  ++i1;
  ++i2;

  trivial_iterator_test(i, i1, v1);
  trivial_iterator_test(i, i2, v1);
}

// mutable_bidirectional_iterator_test

template <class U>
struct undefined;

// Preconditions: [i,i+N) is a valid range
template <class Iterator, class TrueVals>
void random_access_iterator_test(Iterator i, int N, TrueVals vals) {
  bidirectional_iterator_test(i, vals[0], vals[1]);
  const Iterator j = i;
  int c;

  typedef typename std::iterator_traits<Iterator>::value_type value_type;
  ignore_unused<value_type>();

  for (c = 0; c < N - 1; ++c) {
    BOOST_TEST(i == j + c);
    BOOST_TEST(*i == vals[c]);
    BOOST_TEST(*i == implicit_cast<value_type>(j[c]));
    BOOST_TEST(*i == *(j + c));
    BOOST_TEST(*i == *(c + j));
    ++i;
    BOOST_TEST(i > j);
    BOOST_TEST(i >= j);
    BOOST_TEST(j <= i);
    BOOST_TEST(j < i);
  }

  Iterator k = j + N - 1;
  for (c = 0; c < N - 1; ++c) {
    BOOST_TEST(i == k - c);
    BOOST_TEST(*i == vals[N - 1 - c]);
    BOOST_TEST(*i == implicit_cast<value_type>(j[N - 1 - c]));
    Iterator q = k - c;
    ignore_unused(q);
    BOOST_TEST(*i == *q);
    BOOST_TEST(i > j);
    BOOST_TEST(i >= j);
    BOOST_TEST(j <= i);
    BOOST_TEST(j < i);
    --i;
  }
}

// Precondition: i != j
template <class Iterator, class ConstIterator>
void const_nonconst_iterator_test(Iterator i, ConstIterator j) {
  BOOST_TEST(i != j);
  BOOST_TEST(j != i);

  ConstIterator k(i);
  BOOST_TEST(k == i);
  BOOST_TEST(i == k);

  k = i;
  BOOST_TEST(k == i);
  BOOST_TEST(i == k);
  ignore_unused(k);
}

} // namespace histogram
} // namespace boost

#endif // BOOST_HISTOGRAM_TEST_ITERATOR_TESTS_HPP
