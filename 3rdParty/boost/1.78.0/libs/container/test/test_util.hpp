//////////////////////////////////////////////////////////////////////////////
//
// \(C\) Copyright Benedek Thaler 2015-2016
// \(C\) Copyright Ion Gaztanaga 2019-2020. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://erenon.hu/double_ended for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_CONTAINER_TEST_TEST_UTIL_HPP
#define BOOST_CONTAINER_TEST_TEST_UTIL_HPP

#include "test_elem.hpp"

// get_range

template <typename DeVector>
void get_range(int fbeg, int fend, int bbeg, int bend, DeVector &c)
{
   c.clear();

   for (int i = fend; i > fbeg ;)
   {
      c.emplace_front(--i);
   }

   for (int i = bbeg; i < bend; ++i)
   {
      c.emplace_back(i);
   }
}

template <typename Container>
void get_range(int count, Container &c)
{
   c.clear();

   for (int i = 1; i <= count; ++i)
   {
      c.emplace_back(i);
   }
}

template <typename Container>
void get_range(Container &c)
{
   get_range<Container>(1, 13, 13, 25, c);
}

template <typename C1>
void test_equal_range(const C1& a)
{
   BOOST_TEST(a.empty());
}

template <typename Iterator>
void print_range(std::ostream& out, Iterator b, Iterator e)
{
   out << '[';
   bool first = true;

   for (; b != e; ++b)
   {
      if (first) { first = false; }
      else { out << ','; }
      out << *b;
   }
   out << ']';
}

template <typename Range>
void print_range(std::ostream& out, const Range& range)
{
   print_range(out, range.begin(), range.end());
}

template <typename Array, std::size_t N>
void print_range(std::ostream& out, Array (&range)[N])
{
   print_range(out, range, range + N);
}

template <typename C1, typename C2, unsigned N>
void test_equal_range(const C1& a, const C2 (&b)[N])
{
   bool equals = boost::algorithm::equal
      (a.begin(), a.end(), b, b+N);

   BOOST_TEST(equals);

   if (!equals)
   {
      print_range(std::cerr, a);
      std::cerr << "\n";
      print_range(std::cerr, b);
      std::cerr << "\n";
   }
}


template <typename C1, typename C2>
void test_equal_range(const C1& a, const C2&b)
{
   bool equals = boost::algorithm::equal
      (a.begin(), a.end(), b.begin(), b.end());

   BOOST_TEST(equals);

   if (!equals)
   {
      print_range(std::cerr, a);
      std::cerr << "\n";
      print_range(std::cerr, b);
      std::cerr << "\n";
   }
}

#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)

// support initializer_list
template <typename C>
void test_equal_range(const C& a, std::initializer_list<unsigned> il)
{
  typedef typename C::value_type T;
  boost::container::vector<T> b;

  for (auto&& elem : il)
  {
    b.emplace_back((int)elem);
  }

  test_equal_range(a, b);
}

#endif   //#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)

#endif //BOOST_CONTAINER_TEST_TEST_UTIL_HPP
