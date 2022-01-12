//////////////////////////////////////////////////////////////////////////////
//
// \(C\) Copyright Benedek Thaler 2015-2016
// \(C\) Copyright Ion Gaztanaga 2019-2020. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/core/lightweight_test.hpp>
#include <cstring> // memcmp
#include <iostream>
#include <algorithm>
#include <limits>
#include <boost/container/list.hpp>
#include <boost/container/vector.hpp>
#include <boost/core/no_exceptions_support.hpp>
#include <boost/type_traits/is_default_constructible.hpp>
#include <boost/type_traits/is_nothrow_move_constructible.hpp>
#include "dummy_test_allocator.hpp"
#include "propagate_allocator_test.hpp"
#include "check_equal_containers.hpp"
#include "movable_int.hpp"

#include <boost/algorithm/cxx14/equal.hpp>

#define BOOST_CONTAINER_DEVECTOR_ALLOC_STATS
#include <boost/container/devector.hpp>
#undef BOOST_CONTAINER_DEVECTOR_ALLOC_STATS

#include "test_util.hpp"
#include "test_elem.hpp"
#include "input_iterator.hpp"

using namespace boost::container;

struct boost_container_devector;

#ifdef _MSC_VER
   #pragma warning (push)
   #pragma warning (disable : 4127) // conditional expression is constant
#endif

namespace boost {
namespace container {
namespace test {

template<>
struct alloc_propagate_base<boost_container_devector>
{
   template <class T, class Allocator>
   struct apply
   {
      typedef devector<T, Allocator> type;
   };
};

}}}   //namespace boost::container::test {

struct different_growth_policy
{
  template <typename SizeType>
  static SizeType new_capacity(SizeType capacity)
  {
    return (capacity) ? capacity * 4u : 32u;
  }
};

// END HELPERS

template <class Devector> void test_constructor_default()
{
  Devector a;

  BOOST_TEST(a.empty());
  BOOST_TEST(a.get_alloc_count() == 0u);
  BOOST_TEST(a.capacity() == 0u);
}

template <class Devector> void test_constructor_allocator()
{
  typename Devector::allocator_type alloc_template;

  Devector a(alloc_template);

  BOOST_TEST(a.empty());
  BOOST_TEST(a.get_alloc_count() == 0u);
  BOOST_TEST(a.capacity() == 0u);
}

template <class Devector> void test_constructor_reserve_only()
{
  {
    Devector a(16, reserve_only_tag_t());
    BOOST_TEST(a.size() == 0u);
    BOOST_TEST(a.capacity() >= 16u);
  }

  {
    Devector b(0, reserve_only_tag_t());
    BOOST_TEST(b.get_alloc_count() == 0u);
  }
}

template <class Devector> void test_constructor_reserve_only_front_back()
{
  {
    Devector a(8, 8, reserve_only_tag_t());
    BOOST_TEST(a.size() == 0u);
    BOOST_TEST(a.capacity() >= 16u);

    for (int i = 8; i; --i)
    {
      a.emplace_front(i);
    }

    for (int i = 9; i < 17; ++i)
    {
      a.emplace_back(i);
    }

    const int expected [] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    test_equal_range(a, expected);
    BOOST_TEST(a.get_alloc_count() <= 1u);
  }

  {
    Devector b(0, 0, reserve_only_tag_t());
    BOOST_TEST(b.get_alloc_count() == 0u);
  }
}

template <class Devector> void test_constructor_n()
{
   {
      Devector a(8);
      const int expected [] = {0, 0, 0, 0, 0, 0, 0, 0};
      test_equal_range(a, expected);
   }

   {
      Devector b(0);

      test_equal_range(b);
      BOOST_TEST(b.get_alloc_count() == 0u);
   }

   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::value_type T;

   BOOST_IF_CONSTEXPR (! boost::move_detail::is_nothrow_default_constructible<T>::value)
   {
      test_elem_throw::on_ctor_after(4);
      BOOST_TEST_THROWS(Devector(8), test_exception);
      BOOST_TEST(test_elem_base::no_living_elem());
      test_elem_throw::do_not_throw();
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_constructor_n_copy_throwing(dtl::true_)
{
   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::value_type T;
   test_elem_throw::on_copy_after(4);
   const T x(404);
   BOOST_TEST_THROWS(Devector(8, x), test_exception);
   test_elem_throw::do_not_throw();
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}


template <class Devector> void test_constructor_n_copy_throwing(dtl::false_)
{}

template <class Devector> void test_constructor_n_copy()
{
   typedef typename Devector::value_type T;
   {
      const T x(9);
      Devector a(8, x);
      const int expected [] = {9, 9, 9, 9, 9, 9, 9, 9};
      test_equal_range(a, expected);
   }

   {
      const T x(9);
      Devector b(0, x);

      test_equal_range(b);
      BOOST_TEST(b.get_alloc_count() == 0u);
   }

   test_constructor_n_copy_throwing<Devector>
      (dtl::bool_<! boost::move_detail::is_nothrow_copy_constructible<T>::value>());

   BOOST_TEST(test_elem_base::no_living_elem());
}

template <class Devector> void test_constructor_input_range()
{
   typedef typename Devector::value_type T;
   {
      devector<T> expected; get_range<devector<T> >(16, expected);
      devector<T> input = expected;

      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
      input_iterator<Devector> input_end   = make_input_iterator(input, input.end());

      Devector a(input_begin, input_end);
      BOOST_TEST(a == expected);
   }

   { // empty range
      devector<T> input;
      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());

      Devector b(input_begin, input_begin);

      test_equal_range(b);
      BOOST_TEST(b.get_alloc_count() == 0u);
   }

   BOOST_TEST(test_elem_base::no_living_elem());
/* //if move_if_noexcept is implemented
   #ifndef BOOST_NO_EXCEPTIONS
   if (! boost::move_detail::is_nothrow_copy_constructible<T>::value)
   {
      devector<T> input; get_range<devector<T> >(16, input);

      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
      input_iterator<Devector> input_end   = make_input_iterator(input, input.end());

      test_elem_throw::on_copy_after(4);

      BOOST_TEST_THROWS(Devector c(input_begin, input_end), test_exception);
   }

   BOOST_TEST(test_elem_base::no_living_elem());
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
*/
}


void test_constructor_forward_range_throwing(dtl::true_)
{}

void test_constructor_forward_range_throwing(dtl::false_)
{}

template <class Devector> void test_constructor_forward_range()
{
   typedef typename Devector::value_type T;
   boost::container::list<T> ncx;
   ncx.emplace_back(1);
   ncx.emplace_back(2);
   ncx.emplace_back(3);
   ncx.emplace_back(4);
   ncx.emplace_back(5);
   ncx.emplace_back(6);
   ncx.emplace_back(7);
   ncx.emplace_back(8);
   const boost::container::list<T> &x = ncx;

   {
      Devector a(x.begin(), x.end());
      const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8};
      test_equal_range(a, expected);
      BOOST_TEST(a.get_alloc_count() <= 1u);
      a.reset_alloc_stats();
   }

   {
      Devector b(x.begin(), x.begin());

      test_equal_range(b);
      BOOST_TEST(b.get_alloc_count() == 0u);
   }

   #ifndef BOOST_NO_EXCEPTIONS
   BOOST_IF_CONSTEXPR (! boost::move_detail::is_nothrow_copy_constructible<T>::value)
   {
      test_elem_throw::on_copy_after(4);
      BOOST_TEST_THROWS(Devector c(x.begin(), x.end()), test_exception);
      test_elem_throw::do_not_throw();
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_constructor_pointer_range()
{
   typedef typename Devector::value_type T;

   boost::container::vector<T> x; get_range<boost::container::vector<T> >(8, x);
   const T* xbeg = x.data();
   const T* xend = x.data() + x.size();

   {
      Devector a(xbeg, xend);

      const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8};
      test_equal_range(a, expected);
      BOOST_TEST(a.get_alloc_count() <= 1u);
   }

   {
      Devector b(xbeg, xbeg);

      test_equal_range(b);
      BOOST_TEST(b.get_alloc_count() == 0u);
   }

   #ifndef BOOST_NO_EXCEPTIONS
   if (! boost::move_detail::is_nothrow_copy_constructible<T>::value)
   {
      test_elem_throw::on_copy_after(4);
      BOOST_TEST_THROWS(Devector c(xbeg, xend), test_exception);
      test_elem_throw::do_not_throw();
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_copy_constructor()
{
   {
      Devector a;
      Devector b(a);

      test_equal_range(b);
      BOOST_TEST(b.get_alloc_count() == 0u);
   }

   {
      Devector a; get_range<Devector>(8, a);
      Devector b(a);
      const int expected [] = {1, 2, 3, 4, 5, 6, 7, 8};
      test_equal_range(b, expected);
      BOOST_TEST(b.get_alloc_count() <= 1u);
   }

   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::value_type T;

   if (! boost::move_detail::is_nothrow_copy_constructible<T>::value)
   {
      Devector a; get_range<Devector>(8, a);

      test_elem_throw::on_copy_after(4);
      BOOST_TEST_THROWS(Devector b(a), test_exception);
      test_elem_throw::do_not_throw();
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_move_constructor()
{
  { // empty
    Devector a;
    Devector b(boost::move(a));

    BOOST_TEST(a.empty());
    BOOST_TEST(b.empty());
  }

  { // maybe small
    Devector a; get_range<Devector>(1, 5, 5, 9, a);
    Devector b(boost::move(a));

    const int expected [] = {1, 2, 3, 4, 5, 6, 7, 8};
    test_equal_range(b, expected);

    // a is unspecified but valid state
    a.clear();
    BOOST_TEST(a.empty());
  }

  { // big
    Devector a; get_range<Devector>(32, a);
    Devector b(boost::move(a));

    boost::container::vector<int> expected; get_range<boost::container::vector<int> >(32, expected);
    test_equal_range(b, expected);

    // a is unspecified but valid state
    a.clear();
    BOOST_TEST(a.empty());
  }
}

template <class Devector> void test_destructor()
{
  Devector a;

  Devector b; get_range<Devector>(3, b);
}

template <class Devector> void test_assignment()
{
   const typename Devector::size_type alloc_count =
      boost::container::allocator_traits
      < typename Devector::allocator_type >::propagate_on_container_copy_assignment::value;

   { // assign to empty (maybe small)
      Devector a;
      Devector c; get_range<Devector>(6, c);
      Devector &b = c;

      a = b;
      const int expected[] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected);
   }

   { // assign from empty
      Devector a; get_range<Devector>(6, a);
      const Devector b;

      a = b;

      test_equal_range(a);
   }

   { // assign to non-empty
      Devector a; get_range<Devector>(11, 15, 15, 19, a);
      Devector c; get_range<Devector>(6, c);
      const Devector &b = c; 

      a = b;
      const int expected[] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected);
   }

   { // assign to free front
      Devector a; get_range<Devector>(11, 15, 15, 19, a);
      a.reserve_front(8);
      a.reset_alloc_stats();

      Devector c; get_range<Devector>(6, c);
      const Devector &b = c;

      a = b;
      const int expected [] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected);
      BOOST_TEST(a.get_alloc_count() == alloc_count);
   }

   { // assignment overlaps contents
      Devector a; get_range<Devector>(11, 15, 15, 19, a);
      a.reserve_front(12);
      a.reset_alloc_stats();

      Devector c; get_range<Devector>(6, c);
      const Devector &b = c;

      a = b;
      const int expected [] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected);
      BOOST_TEST(a.get_alloc_count() == alloc_count);
   }

   { // assignment exceeds contents
      Devector a; get_range<Devector>(11, 13, 13, 15, a);
      a.reserve_front(8);
      a.reserve_back(8);
      a.reset_alloc_stats();

      Devector c; get_range<Devector>(12, c);
      const Devector &b = c;

      a = b;
      const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
      test_equal_range(a, expected);
      BOOST_TEST(a.get_alloc_count() == alloc_count);
   }

   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::value_type T;
   BOOST_IF_CONSTEXPR (! boost::move_detail::is_nothrow_copy_constructible<T>::value)
   {
      // strong guarantee if reallocation is needed (no guarantee otherwise)
      Devector a; get_range<Devector>(6, a);
      Devector c; get_range<Devector>(12, c);
      const Devector &b = c;

      test_elem_throw::on_copy_after(3);
      BOOST_TEST_THROWS(a = b, test_exception);
      test_elem_throw::do_not_throw();

      const int expected[] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected);
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_move_assignment_throwing(dtl::true_)
// move should be used on the slow path
{
   Devector a; get_range<Devector>(11, 15, 15, 19, a);
   Devector b; get_range<Devector>(6, b);

   test_elem_throw::on_copy_after(3);
   a = boost::move(b);
   test_elem_throw::do_not_throw();

   const int expected [] = {1, 2, 3, 4, 5, 6};
   test_equal_range(a, expected);

   b.clear();
   test_equal_range(b);
}

template <class Devector> void test_move_assignment_throwing(dtl::false_)
{}

template <class Devector> void test_move_assignment()
{
  { // assign to empty (maybe small)
    Devector a;
    Devector b; get_range<Devector>(6, b);

    a = boost::move(b);

    const int expected [] = {1, 2, 3, 4, 5, 6};
    test_equal_range(a, expected);

    // b is in unspecified but valid state
    b.clear();
    test_equal_range(b);
  }

  { // assign from empty
    Devector a; get_range<Devector>(6, a);
    Devector b;

    a = boost::move(b);

    test_equal_range(a);
    test_equal_range(b);
  }

  { // assign to non-empty
    Devector a; get_range<Devector>(11, 15, 15, 19, a);
    Devector b; get_range<Devector>(6, b);

    a = boost::move(b);
    const int expected [] = {1, 2, 3, 4, 5, 6};
    test_equal_range(a, expected);

    b.clear();
    test_equal_range(b);
  }

  typedef typename Devector::value_type T;
   test_move_assignment_throwing<Devector>
      (boost::move_detail::bool_<! boost::move_detail::is_nothrow_copy_constructible<T>::value>());
}

template <class Devector> void test_il_assignment()
{
   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)

   { // assign to empty (maybe small)
      Devector a;
      a = {1, 2, 3, 4, 5, 6 };
      test_equal_range(a, {1, 2, 3, 4, 5, 6});
   }

   { // assign from empty
      Devector a; get_range<Devector>(6, a);
      a = {};

      test_equal_range(a);
   }

   { // assign to non-empty
      Devector a; get_range<Devector>(11, 15, 15, 19, a);

      a = {1, 2, 3, 4, 5, 6};

      test_equal_range(a, {1, 2, 3, 4, 5, 6});
   }

   { // assign to free front
      Devector a; get_range<Devector>(11, 15, 15, 19, a);
      a.reserve_front(8);
      a.reset_alloc_stats();

      a = {1, 2, 3, 4, 5, 6};

      test_equal_range(a, {1, 2, 3, 4, 5, 6});
      BOOST_TEST(a.get_alloc_count() == 0u);
   }

   { // assignment overlaps contents
      Devector a; get_range<Devector>(11, 15, 15, 19, a);
      a.reserve_front(12);
      a.reset_alloc_stats();

      a = {1, 2, 3, 4, 5, 6};

      test_equal_range(a, {1, 2, 3, 4, 5, 6});
      BOOST_TEST(a.get_alloc_count() == 0u);
   }

   { // assignment exceeds contents
      Devector a; get_range<Devector>(11, 13, 13, 15, a);
      a.reserve_front(8);
      a.reserve_back(8);
      a.reset_alloc_stats();

      a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

      test_equal_range(a, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
      BOOST_TEST(a.get_alloc_count() == 0u);
   }

   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::value_type T;
   if (! boost::move_detail::is_nothrow_copy_constructible<T>::value)
   {
      // strong guarantee if reallocation is needed (no guarantee otherwise)
      Devector a; get_range<Devector>(6, a);

      test_elem_throw::on_copy_after(3);

      BOOST_TRY
      {
         a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
         BOOST_TEST(false);
      }
      BOOST_CATCH(const test_exception&) {}
      BOOST_CATCH_END
      test_elem_throw::do_not_throw();

      test_equal_range(a, {1, 2, 3, 4, 5, 6});
   }
   #endif   //BOOST_NO_EXCEPTIONS
   #endif   //#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
}

template <class Devector> void test_assign_input_range()
{
   typedef typename Devector::value_type T;

   { // assign to empty, keep it small
      devector<T> expected; get_range<Devector>(1, 13, 13, 25, expected);
      devector<T> input = expected;

      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
      input_iterator<Devector> input_end   = make_input_iterator(input, input.end());

      Devector a;
      a.reset_alloc_stats();
      a.assign(input_begin, input_end);

      BOOST_TEST(a == expected);
   }

   { // assign to empty (maybe small)
      devector<T> input; get_range<devector<T> >(6, input);

      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
      input_iterator<Devector> input_end   = make_input_iterator(input, input.end());

      Devector a;

      a.assign(input_begin, input_end);
      const int expected [] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected);
   }

   { // assign from empty
      devector<T> input; get_range<devector<T> >(6, input);
      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());

      Devector a; get_range<Devector>(6, a);
      a.assign(input_begin, input_begin);

      test_equal_range(a);
   }

   { // assign to non-empty
      devector<T> input; get_range<devector<T> >(6, input);
      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
      input_iterator<Devector> input_end   = make_input_iterator(input, input.end());

      Devector a; get_range<Devector>(11, 15, 15, 19, a);
      a.assign(input_begin, input_end);
      const int expected [] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected);
   }

   { // assign to free front
      devector<T> input; get_range<devector<T> >(6, input);
      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
      input_iterator<Devector> input_end   = make_input_iterator(input, input.end());

      Devector a; get_range<Devector>(11, 15, 15, 19, a);
      a.reserve_front(8);
      a.reset_alloc_stats();

      a.assign(input_begin, input_end);
      const int expected[] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected);
   }

   { // assignment overlaps contents
      devector<T> input; get_range<devector<T> >(6, input);
      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
      input_iterator<Devector> input_end   = make_input_iterator(input, input.end());

      Devector a; get_range<Devector>(11, 15, 15, 19, a);
      a.reserve_front(12);
      a.reset_alloc_stats();

      a.assign(input_begin, input_end);
      const int expected[] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected);
   }

   { // assignment exceeds contents
      devector<T> input; get_range<devector<T> >(12, input);
      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
      input_iterator<Devector> input_end   = make_input_iterator(input, input.end());

      Devector a; get_range<Devector>(11, 13, 13, 15, a);
      a.reserve_front(8);
      a.reserve_back(8);
      a.reset_alloc_stats();

      a.assign(input_begin, input_end);
      const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
      test_equal_range(a, expected);
   }

   #ifndef BOOST_NO_EXCEPTIONS
   if (! boost::move_detail::is_nothrow_copy_constructible<T>::value)
   {
      // strong guarantee if reallocation is needed (no guarantee otherwise)

      devector<T> input; get_range<devector<T> >(12, input);
      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
      input_iterator<Devector> input_end   = make_input_iterator(input, input.end());

      Devector a; get_range<Devector>(6, a);

      test_elem_throw::on_copy_after(3);
      BOOST_TEST_THROWS(a.assign(input_begin, input_end), test_exception);
      test_elem_throw::do_not_throw();

      const int expected[] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected);
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_assign_forward_range_throwing(dtl::false_)
{}

template <class Devector> void test_assign_forward_range()
{
  typedef typename Devector::value_type T;
  typedef boost::container::list<T> List;

  boost::container::list<T> x;
  typedef typename List::iterator list_iterator;
  x.emplace_back(1);
  x.emplace_back(2);
  x.emplace_back(3);
  x.emplace_back(4);
  x.emplace_back(5);
  x.emplace_back(6);
  x.emplace_back(7);
  x.emplace_back(8);
  x.emplace_back(9);
  x.emplace_back(10);
  x.emplace_back(11);
  x.emplace_back(12);

  list_iterator one = x.begin();
  list_iterator six = one;
  list_iterator twelve = one;

  iterator_advance(six, 6);
  iterator_advance(twelve, 12);

  { // assign to empty (maybe small)
    Devector a;

    a.assign(one, six);

    const int expected [] = {1, 2, 3, 4, 5, 6};
    test_equal_range(a, expected);
  }

  { // assign from empty
    Devector a; get_range<Devector>(6, a);

    a.assign(one, one);

    test_equal_range(a);
  }

  { // assign to non-empty
    Devector a; get_range<Devector>(11, 15, 15, 19, a);

    a.assign(one, six);

    const int expected [] = {1, 2, 3, 4, 5, 6};
    test_equal_range(a, expected);
  }

  { // assign to free front
    Devector a; get_range<Devector>(11, 15, 15, 19, a);
    a.reserve_front(8);
    a.reset_alloc_stats();

    a.assign(one, six);

    const int expected [] = {1, 2, 3, 4, 5, 6};
    test_equal_range(a, expected);
    BOOST_TEST(a.get_alloc_count() == 0u);
  }

  { // assignment overlaps contents
    Devector a; get_range<Devector>(11, 15, 15, 19, a);
    a.reserve_front(12);
    a.reset_alloc_stats();

    a.assign(one, six);

    const int expected [] = {1, 2, 3, 4, 5, 6};
    test_equal_range(a, expected);
    BOOST_TEST(a.get_alloc_count() == 0u);
  }

  { // assignment exceeds contents
    Devector a; get_range<Devector>(11, 13, 13, 15, a);
    a.reserve_front(8);
    a.reserve_back(8);
    a.reset_alloc_stats();

    a.assign(one, twelve);

    const int expected [] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    test_equal_range(a, expected);
    BOOST_TEST(a.get_alloc_count() == 0u);
  }

   #ifndef BOOST_NO_EXCEPTIONS
   BOOST_IF_CONSTEXPR(! boost::move_detail::is_nothrow_copy_constructible<T>::value)
   {
      // strong guarantee if reallocation is needed (no guarantee otherwise)
      Devector a; get_range<Devector>(6, a);

      test_elem_throw::on_copy_after(3);
      BOOST_TEST_THROWS(a.assign(one, twelve), test_exception);
      test_elem_throw::do_not_throw();

      const int expected [] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected);
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_assign_pointer_range()
{
   typedef typename Devector::value_type T;

   boost::container::vector<T> x; get_range<boost::container::vector<T> >(12, x);
   const T* one = x.data();
   const T* six = one + 6;
   const T* twelve = one + 12;

   { // assign to empty (maybe small)
      Devector a;

      a.assign(one, six);

      const int expected[] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected);
   }

   { // assign from empty
      Devector a; get_range<Devector>(6, a);

      a.assign(one, one);

      test_equal_range(a);
   }

   { // assign to non-empty
      Devector a; get_range<Devector>(11, 15, 15, 19, a);

      a.assign(one, six);

      const int expected[] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected);
   }

   { // assign to free front
      Devector a; get_range<Devector>(11, 15, 15, 19, a);
      a.reserve_front(8);
      a.reset_alloc_stats();

      a.assign(one, six);

      const int expected[] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected);
      BOOST_TEST(a.get_alloc_count() == 0u);
   }

   { // assignment overlaps contents
      Devector a; get_range<Devector>(11, 15, 15, 19, a);
      a.reserve_front(12);
      a.reset_alloc_stats();

      a.assign(one, six);

      const int expected[] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected);
      BOOST_TEST(a.get_alloc_count() == 0u);
   }

   { // assignment exceeds contents
      Devector a; get_range<Devector>(11, 13, 13, 15, a);
      a.reserve_front(8);
      a.reserve_back(8);
      a.reset_alloc_stats();

      a.assign(one, twelve);

      const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
      test_equal_range(a, expected);
      BOOST_TEST(a.get_alloc_count() == 0u);
   }

   #ifndef BOOST_NO_EXCEPTIONS
   if (! boost::move_detail::is_nothrow_copy_constructible<T>::value)
   {
      // strong guarantee if reallocation is needed (no guarantee otherwise)
      Devector a; get_range<Devector>(6, a);

      test_elem_throw::on_copy_after(3);
      BOOST_TEST_THROWS(a.assign(one, twelve), test_exception);
      test_elem_throw::do_not_throw();

      const int expected[] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected);
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_assign_n()
{
   typedef typename Devector::value_type T;

   { // assign to empty (maybe small)
      Devector a;

      a.assign(6, T(9));

      const int expected[] = {9, 9, 9, 9, 9, 9};
      test_equal_range(a, expected);
   }

   { // assign from empty
      Devector a; get_range<Devector>(6, a);

      a.assign(0, T(404));

      test_equal_range(a);
   }

   { // assign to non-empty
      Devector a; get_range<Devector>(11, 15, 15, 19, a);

      a.assign(6, T(9));

      const int expected[] = {9, 9, 9, 9, 9, 9};
      test_equal_range(a, expected);
   }

   { // assign to free front
      Devector a; get_range<Devector>(11, 15, 15, 19, a);
      a.reserve_front(8);
      a.reset_alloc_stats();

      a.assign(6, T(9));

      const int expected[] = {9, 9, 9, 9, 9, 9};
      test_equal_range(a, expected);
      BOOST_TEST(a.get_alloc_count() == 0u);
   }

   { // assignment overlaps contents
      Devector a; get_range<Devector>(11, 15, 15, 19, a);
      a.reserve_front(12);
      a.reset_alloc_stats();

      a.assign(6, T(9));

      const int expected[] = {9, 9, 9, 9, 9, 9};
      test_equal_range(a, expected);
      BOOST_TEST(a.get_alloc_count() == 0u);
   }

   { // assignment exceeds contents
      Devector a; get_range<Devector>(11, 13, 13, 15, a);
      a.reserve_front(8);
      a.reserve_back(8);
      a.reset_alloc_stats();

      a.assign(12, T(9));

      const int expected[] = {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9};
      test_equal_range(a, expected);
      BOOST_TEST(a.get_alloc_count() == 0u);
   }

   #ifndef BOOST_NO_EXCEPTIONS
   if (! boost::move_detail::is_nothrow_copy_constructible<T>::value)
   {
      // strong guarantee if reallocation is needed (no guarantee otherwise)
      Devector a; get_range<Devector>(6, a);

      test_elem_throw::on_copy_after(3);
      BOOST_TEST_THROWS(a.assign(32, T(9)), test_exception);
      test_elem_throw::do_not_throw();

      const int expected[] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected);
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_assign_il()
{
   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)

   { // assign to empty (maybe small)
      Devector a;

      a.assign({1, 2, 3, 4, 5, 6});

      test_equal_range(a, {1, 2, 3, 4, 5, 6});
   }

   { // assign from empty
      Devector a; get_range<Devector>(6, a);

      a.assign({});

      test_equal_range(a);
   }

   { // assign to non-empty
      Devector a; get_range<Devector>(11, 15, 15, 19, a);

      a.assign({1, 2, 3, 4, 5, 6});

      test_equal_range(a, {1, 2, 3, 4, 5, 6});
   }

   { // assign to free front
      Devector a; get_range<Devector>(11, 15, 15, 19, a);
      a.reserve_front(8);
      a.reset_alloc_stats();

      a.assign({1, 2, 3, 4, 5, 6});

      test_equal_range(a, {1, 2, 3, 4, 5, 6});
      BOOST_TEST(a.get_alloc_count() == 0u);
   }

   { // assignment overlaps contents
      Devector a; get_range<Devector>(11, 15, 15, 19, a);
      a.reserve_front(12);
      a.reset_alloc_stats();

      a.assign({1, 2, 3, 4, 5, 6});

      test_equal_range(a, {1, 2, 3, 4, 5, 6});
      BOOST_TEST(a.get_alloc_count() == 0u);
   }

   { // assignment exceeds contents
      Devector a; get_range<Devector>(11, 13, 13, 15, a);
      a.reserve_front(8);
      a.reserve_back(8);
      a.reset_alloc_stats();

      a.assign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});

      test_equal_range(a, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
      BOOST_TEST(a.get_alloc_count() == 0u);
   }

   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::value_type T;
   if (! boost::move_detail::is_nothrow_copy_constructible<T>::value)
   {
      // strong guarantee if reallocation is needed (no guarantee otherwise)
      Devector a; get_range<Devector>(6, a);

      test_elem_throw::on_copy_after(3);
      BOOST_TEST_THROWS(a.assign({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18}), test_exception);
      test_elem_throw::do_not_throw();

      test_equal_range(a, {1, 2, 3, 4, 5, 6});
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
   #endif   //#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
}

template <class Devector> void test_get_allocator()
{
  Devector a;
  (void) a.get_allocator();
}

template <class Devector> void test_begin_end()
{
   boost::container::vector<int> expected; get_range<boost::container::vector<int> >(10, expected);
   {
      Devector actual; get_range<Devector>(10, actual);

      BOOST_TEST(boost::algorithm::equal(expected.begin(), expected.end(), actual.begin(), actual.end()));
      BOOST_TEST(boost::algorithm::equal(expected.rbegin(), expected.rend(), actual.rbegin(), actual.rend()));
      BOOST_TEST(boost::algorithm::equal(expected.cbegin(), expected.cend(), actual.cbegin(), actual.cend()));
      BOOST_TEST(boost::algorithm::equal(expected.crbegin(), expected.crend(), actual.crbegin(), actual.crend()));
   }

   {
      Devector cactual; get_range<Devector>(10, cactual);

      BOOST_TEST(boost::algorithm::equal(expected.begin(), expected.end(), cactual.begin(), cactual.end()));
      BOOST_TEST(boost::algorithm::equal(expected.rbegin(), expected.rend(), cactual.rbegin(), cactual.rend()));
   }
}

template <class Devector> void test_empty()
{
  typedef typename Devector::value_type T;

  Devector a;
  BOOST_TEST(a.empty());

  a.push_front(T(1));
  BOOST_TEST(! a.empty());

  a.pop_back();
  BOOST_TEST(a.empty());

  Devector b(16, reserve_only_tag_t());
  BOOST_TEST(b.empty());

  Devector c; get_range<Devector>(3, c);
  BOOST_TEST(! c.empty());
}

//template <typename ST>
//using gp_devector = devector<unsigned, different_growth_policy>;

void test_max_size()
{/*
  gp_devector<unsigned char> a;
  BOOST_TEST(a.max_size() == (std::numeric_limits<unsigned char>::max)());

  gp_devector<unsigned short> b;
  BOOST_TEST(b.max_size() == (std::numeric_limits<unsigned short>::max)());

  gp_devector<unsigned int> c;
  BOOST_TEST(c.max_size() >= b.max_size());

  gp_devector<std::size_t> d;
  BOOST_TEST(d.max_size() >= c.max_size());
*/
}

void test_exceeding_max_size()
{/*
   #ifndef BOOST_NO_EXCEPTIONS
   using Devector = gp_devector<unsigned char>;

   Devector a((std::numeric_limits<typename Devector::size_type>::max)());
   BOOST_TEST_THROWS(a.emplace_back(404), std::length_error);
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
*/
}

template <class Devector> void test_size()
{
  typedef typename Devector::value_type T;

  Devector a;
  BOOST_TEST(a.size() == 0u);

  a.push_front(T(1));
  BOOST_TEST(a.size() == 1u);

  a.pop_back();
  BOOST_TEST(a.size() == 0u);

  Devector b(16, reserve_only_tag_t());
  BOOST_TEST(b.size() == 0u);

  Devector c; get_range<Devector>(3, c);
  BOOST_TEST(c.size() == 3u);
}

template <class Devector> void test_capacity()
{
  Devector a;
  BOOST_TEST(a.capacity() == 0u);

  Devector b(128, reserve_only_tag_t());
  BOOST_TEST(b.capacity() >= 128u);

  Devector c; get_range<Devector>(10, c);
  BOOST_TEST(c.capacity() >= 10u);
}

template <class Devector> void test_resize_front_throwing(dtl::true_)
{
   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::iterator iterator;

   Devector d; get_range<Devector>(5, d);
   boost::container::vector<int> d_origi; get_range<boost::container::vector<int> >(5, d_origi);
   iterator origi_begin = d.begin();

   test_elem_throw::on_ctor_after(3);
   BOOST_TEST_THROWS(d.resize_front(256), test_exception);
   test_elem_throw::do_not_throw();

   test_equal_range(d, d_origi);
   BOOST_TEST(origi_begin == d.begin());
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_resize_front_throwing(dtl::false_)
{}


template <class Devector> void test_resize_front()
{
   typedef typename Devector::value_type T;

   // size < required, alloc needed
   {
      Devector a; get_range<Devector>(5, a);
      a.resize_front(8);
      const int expected [] = {0, 0, 0, 1, 2, 3, 4, 5};
      test_equal_range(a, expected);
   }

   // size < required, but capacity provided
   {
      Devector b; get_range<Devector>(5, b);
      b.reserve_front(16);
      b.resize_front(8);
      const int expected [] = {0, 0, 0, 1, 2, 3, 4, 5};
      test_equal_range(b, expected);
   }
   /*
   // size < required, move would throw
   if (! boost::is_nothrow_move_constructible<T>::value && std::is_copy_constructible<T>::value)
   {
      Devector c; get_range<Devector>(5, c);

      test_elem_throw::on_move_after(3);
      c.resize_front(8); // shouldn't use the throwing move
      test_elem_throw::do_not_throw();

      test_equal_range(c, {0, 0, 0, 1, 2, 3, 4, 5});
   }
   */

   test_resize_front_throwing<Devector>
      (dtl::bool_<! boost::move_detail::is_nothrow_default_constructible<T>::value>());

   // size >= required
   {
      Devector e; get_range<Devector>(6, e);
      e.resize_front(4);
      const int expected [] = {3, 4, 5, 6};
      test_equal_range(e, expected);
   }

   // size < required, does not fit front small buffer
   {
      boost::container::vector<int> expected(128);
      Devector g;
      g.resize_front(128);
      test_equal_range(g, expected);
   }

   // size = required
   {
      Devector e; get_range<Devector>(6, e);
      e.resize_front(6);
      const int expected [] = {1, 2, 3, 4, 5, 6};
      test_equal_range(e, expected);
   }
}

template <class Devector> void test_resize_front_copy_throwing(dtl::true_)
{
   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::value_type T;
   typedef typename Devector::iterator iterator;

   // size < required, copy throws
   {
      Devector c; get_range<Devector>(5, c);
      boost::container::vector<int> c_origi; get_range<boost::container::vector<int> >(5, c_origi);

      test_elem_throw::on_copy_after(3);
      BOOST_TEST_THROWS(c.resize_front(256, T(404)), test_exception);
      test_elem_throw::do_not_throw();

      test_equal_range(c, c_origi);
   }

   // size < required, copy throws, but later
   {
      Devector c; get_range<Devector>(5, c);
      boost::container::vector<int> c_origi; get_range<boost::container::vector<int> >(5, c_origi);
      iterator origi_begin = c.begin();

      test_elem_throw::on_copy_after(7);
      BOOST_TEST_THROWS(c.resize_front(256, T(404)), test_exception);
      test_elem_throw::do_not_throw();

      test_equal_range(c, c_origi);
      BOOST_TEST(origi_begin == c.begin());
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_resize_front_copy_throwing(dtl::false_)
{}

template <class Devector> void test_resize_front_copy()
{
   typedef typename Devector::value_type T;

   // size < required, alloc needed
   {
      Devector a; get_range<Devector>(5, a);
      a.resize_front(8, T(9));
      const int expected [] = {9, 9, 9, 1, 2, 3, 4, 5};
      test_equal_range(a, expected);
   }

   // size < required, but capacity provided
   {
      Devector b; get_range<Devector>(5, b);
      b.reserve_front(16);
      b.resize_front(8, T(9));
      const int expected [] = {9, 9, 9, 1, 2, 3, 4, 5};
      test_equal_range(b, expected);
   }

   test_resize_front_copy_throwing<Devector>
      (dtl::bool_<! boost::move_detail::is_nothrow_copy_constructible<T>::value>());

   // size >= required
   {
      Devector e; get_range<Devector>(6, e);
      e.resize_front(4, T(404));
      const int expected[] = {3, 4, 5, 6};
      test_equal_range(e, expected);
   }

   // size < required, does not fit front small buffer
   {
      boost::container::vector<int> expected(128, 9);
      Devector g;
      g.resize_front(128, T(9));
      test_equal_range(g, expected);
   }

   // size = required
   {
      Devector e; get_range<Devector>(6, e);
      e.resize_front(6, T(9));
      const int expected[] = {1, 2, 3, 4, 5, 6};
      test_equal_range(e, expected);
   }

   // size < required, tmp is already inserted
   {
      Devector f; get_range<Devector>(8, f);
      const T& tmp = *(f.begin() + 1);
      f.resize_front(16, tmp);
      const int expected[] = {2,2,2,2,2,2,2,2,1,2,3,4,5,6,7,8};
      test_equal_range(f, expected);
   }
}

template <class Devector> void test_resize_back_throwing(dtl::true_)
// size < required, constructor throws
{
   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::iterator iterator;

   Devector d; get_range<Devector>(5, d);
   boost::container::vector<int> d_origi; get_range<boost::container::vector<int> >(5, d_origi);
   iterator origi_begin = d.begin();

   test_elem_throw::on_ctor_after(3);
   BOOST_TEST_THROWS(d.resize_back(256), test_exception);
   test_elem_throw::do_not_throw();

   test_equal_range(d, d_origi);
   BOOST_TEST(origi_begin == d.begin());
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_resize_back_throwing(dtl::false_)
{}

template <class Devector> void test_resize_back()
{
   typedef typename Devector::value_type T;

   // size < required, alloc needed
   {
      Devector a; get_range<Devector>(5, a);
      a.resize_back(8);
      const int expected [] = {1, 2, 3, 4, 5, 0, 0, 0};
      test_equal_range(a, expected);
   }

   // size < required, but capacity provided
   {
      Devector b; get_range<Devector>(5, b);
      b.reserve_back(16);
      b.resize_back(8);
      const int expected [] = {1, 2, 3, 4, 5, 0, 0, 0};
      test_equal_range(b, expected);
   }
   /*
   // size < required, move would throw
   if (! boost::is_nothrow_move_constructible<T>::value && std::is_copy_constructible<T>::value)
   {
      Devector c; get_range<Devector>(5, c);

      test_elem_throw::on_move_after(3);
      c.resize_back(8); // shouldn't use the throwing move
      test_elem_throw::do_not_throw();

      test_equal_range(c, {1, 2, 3, 4, 5, 0, 0, 0});
   }
   */

   test_resize_back_throwing<Devector>
      (dtl::bool_<! boost::move_detail::is_nothrow_default_constructible<T>::value>());

   // size >= required
   {
      Devector e; get_range<Devector>(6, e);
      e.resize_back(4);
      const int expected [] = {1, 2, 3, 4};
      test_equal_range(e, expected);
   }

   // size < required, does not fit front small buffer
   {
      boost::container::vector<int> expected(128);
      Devector g;
      g.resize_back(128);
      test_equal_range(g, expected);
   }

   // size = required
   {
      Devector e; get_range<Devector>(6, e);
      e.resize_back(6);
      const int expected [] = {1, 2, 3, 4, 5, 6};
      test_equal_range(e, expected);
   }
}

template <class Devector> void test_resize_back_copy_throwing(dtl::true_)
{
   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::value_type T;
   typedef typename Devector::iterator iterator;

   // size < required, copy throws
   {
      Devector c; get_range<Devector>(5, c);
      boost::container::vector<int> c_origi; get_range<boost::container::vector<int> >(5, c_origi);

      test_elem_throw::on_copy_after(3);
      BOOST_TEST_THROWS(c.resize_back(256, T(404)), test_exception);
      test_elem_throw::do_not_throw();

      test_equal_range(c, c_origi);
   }

   // size < required, copy throws, but later
   {
      Devector c; get_range<Devector>(5, c);
      boost::container::vector<int> c_origi; get_range<boost::container::vector<int> >(5, c_origi);
      iterator origi_begin = c.begin();

      test_elem_throw::on_copy_after(7);
      BOOST_TEST_THROWS(c.resize_back(256, T(404)), test_exception);
      test_elem_throw::do_not_throw();

      test_equal_range(c, c_origi);
      BOOST_TEST(origi_begin == c.begin());
   }

   // size < required, copy throws
   {
      Devector c; get_range<Devector>(5, c);
      boost::container::vector<int> c_origi; get_range<boost::container::vector<int> >(5, c_origi);
      iterator origi_begin = c.begin();

      test_elem_throw::on_copy_after(3);
      BOOST_TEST_THROWS(c.resize_back(256, T(404)), test_exception);
      test_elem_throw::do_not_throw();

      test_equal_range(c, c_origi);
      BOOST_TEST(origi_begin == c.begin());
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_resize_back_copy_throwing(dtl::false_)
{}

template <class Devector> void test_resize_back_copy()
{
  typedef typename Devector::value_type T;

   // size < required, alloc needed
   {
      Devector a; get_range<Devector>(5, a);
      a.resize_back(8, T(9));
      const int expected [] = {1, 2, 3, 4, 5, 9, 9, 9};
      test_equal_range(a, expected);
   }

   // size < required, but capacity provided
   {
      Devector b; get_range<Devector>(5, b);
      b.reserve_back(16);
      b.resize_back(8, T(9));
      const int expected [] = {1, 2, 3, 4, 5, 9, 9, 9};
      test_equal_range(b, expected);
   }

   test_resize_back_copy_throwing<Devector>
      (dtl::bool_<! boost::move_detail::is_nothrow_copy_constructible<T>::value>());

   // size >= required
   {
      Devector e; get_range<Devector>(6, e);
      e.resize_back(4, T(404));
      const int expected [] = {1, 2, 3, 4};
      test_equal_range(e, expected);
   }

   // size < required, does not fit front small buffer
   {
      boost::container::vector<int> expected(128, 9);
      Devector g;
      g.resize_back(128, T(9));
      test_equal_range(g, expected);
   }

   // size = required
   {
      Devector e; get_range<Devector>(6, e);
      e.resize_back(6, T(9));
      const int expected [] = {1, 2, 3, 4, 5, 6};
      test_equal_range(e, expected);
   }

   // size < required, tmp is already inserted
   {
      Devector f; get_range<Devector>(8, f);
      const T& tmp = *(f.begin() + 1);
      f.resize_back(16, tmp);
      const int expected [] = {1,2,3,4,5,6,7,8,2,2,2,2,2,2,2,2};
      test_equal_range(f, expected);
   }
}

/*
template <class Devector> void test_constructor_unsafe_uninitialized()
{
  {
    Devector a(8, unsafe_uninitialized_tag_t());
    BOOST_TEST(a.size() == 8u);

    for (int i = 0; i < 8; ++i)
    {
      new (a.data() + i) T(i+1);
    }

    const int expected [] = {1, 2, 3, 4, 5, 6, 7, 8};
    test_equal_range(a, expected);
  }

  {
    Devector b(0, unsafe_uninitialized_tag_t());
    BOOST_TEST(b.get_alloc_count() == 0u);
  }
}
*/

/*
template <class Devector> void test_unsafe_uninitialized_resize_front()
{
  typedef typename Devector::value_type T;

  { // noop
    Devector a; get_range<Devector>(8, a);
    a.reset_alloc_stats();

    a.unsafe_uninitialized_resize_front(a.size());

    const int expected [] = {1, 2, 3, 4, 5, 6, 7, 8};
    test_equal_range(a, expected);
    BOOST_TEST(a.get_alloc_count() == 0u);
  }

  { // grow (maybe has enough capacity)
    Devector b; get_range<Devector>(0, 0, 5, 9, b);

    b.unsafe_uninitialized_resize_front(8);

    for (int i = 0; i < 4; ++i)
    {
      new (b.data() + i) T(i+1);
    }

   const int expected [] = {1, 2, 3, 4, 5, 6, 7, 8};
    test_equal_range(b, expected);
  }

  { // shrink uninitialized
    Devector c; get_range<Devector>(8, c);

    c.unsafe_uninitialized_resize_front(16);
    c.unsafe_uninitialized_resize_front(8);

      const int expected [] = {1, 2, 3, 4, 5, 6, 7, 8};
    test_equal_range(c, expected );
  }

  if (std::is_trivially_destructible<T>::value)
  {
    // shrink
    Devector d; get_range<Devector>(8, d);

    d.unsafe_uninitialized_resize_front(4);

    test_equal_range(d, {5, 6, 7, 8});
  }
}

template <class Devector> void test_unsafe_uninitialized_resize_back()
{
  typedef typename Devector::value_type T;

  { // noop
    Devector a; get_range<Devector>(8, a);
    a.reset_alloc_stats();

    a.unsafe_uninitialized_resize_back(a.size());

    test_equal_range(a, {1, 2, 3, 4, 5, 6, 7, 8});
    BOOST_TEST(a.get_alloc_count() == 0u);
  }

  { // grow (maybe has enough capacity)
    Devector b; get_range<Devector>(1, 5, 0, 0, b);

    b.unsafe_uninitialized_resize_back(8);

    for (int i = 0; i < 4; ++i)
    {
      new (b.data() + 4 + i) T(i+5);
    }

    test_equal_range(b, {1, 2, 3, 4, 5, 6, 7, 8});
  }

  { // shrink uninitialized
    Devector c; get_range<Devector>(8, c);

    c.unsafe_uninitialized_resize_back(16);
    c.unsafe_uninitialized_resize_back(8);

    test_equal_range(c, {1, 2, 3, 4, 5, 6, 7, 8});
  }

  if (std::is_trivially_destructible<T>::value)
  {
    // shrink
    Devector d; get_range<Devector>(8, d);

    d.unsafe_uninitialized_resize_back(4);

    test_equal_range(d, {1, 2, 3, 4});
  }
}
*/

template <class Devector> void test_reserve_front()
{
  typedef typename Devector::value_type value_type;
  Devector a;

  a.reserve_front(100);
  for (int i = 0; i < 100; ++i)
  {
    a.push_front(value_type(i));
  }

  BOOST_TEST(a.get_alloc_count() == 1u);

  Devector b;
  b.reserve_front(4);
  b.reserve_front(6);
  b.reserve_front(4);
  b.reserve_front(8);
  b.reserve_front(16);
}

template <class Devector> void test_reserve_back()
{
  Devector a;
  typedef typename Devector::value_type value_type;
  a.reserve_back(100);
  for (int i = 0; i < 100; ++i)
  {
    a.push_back(value_type(i));
  }

  BOOST_TEST(a.get_alloc_count() == 1u);

  Devector b;
  b.reserve_back(4);
  b.reserve_back(6);
  b.reserve_back(4);
  b.reserve_back(8);
  b.reserve_back(16);
}

template <typename Devector>
void test_shrink_to_fit_always()
{
   typedef typename Devector::value_type value_type;
   Devector a;
   a.reserve(100u);

   a.push_back(value_type(1));
   a.push_back(value_type(2));
   a.push_back(value_type(3));

   a.shrink_to_fit();

   boost::container::vector<unsigned> expected;
   expected.push_back(1u);
   expected.push_back(2u);
   expected.push_back(3u);
   test_equal_range(a, expected);

   std::size_t exp_capacity = 3u;
   BOOST_TEST(a.capacity() == exp_capacity);
}

template <typename Devector>
void test_shrink_to_fit_never()
{
   Devector a;
   a.reserve(100);

   a.push_back(1);
   a.push_back(2);
   a.push_back(3);

   a.shrink_to_fit();

   boost::container::vector<unsigned> expected;
   expected.emplace_back(1);
   expected.emplace_back(2);
   expected.emplace_back(3);
   test_equal_range(a, expected);
   BOOST_TEST(a.capacity() == 100u);
}

void shrink_to_fit()
{
   typedef devector<unsigned> devector_u_shr;
   typedef devector<unsigned> small_devector_u_shr;
   test_shrink_to_fit_always<devector_u_shr>();
   test_shrink_to_fit_always<small_devector_u_shr>();
}

template <class Devector> void test_index_operator()
{
  typedef typename Devector::value_type T;

  { // non-const []
    Devector a; get_range<Devector>(5, a);

    BOOST_TEST(a[0] == 1);
    BOOST_TEST(a[4] == 5);
    BOOST_TEST(&a[3] == &a[0] + 3);

    a[0] = T(100);
    BOOST_TEST(a[0] == 100);
  }

  { // const []
    Devector b; get_range<Devector>(5, b);
    const Devector &a = b;

    BOOST_TEST(a[0] == 1);
    BOOST_TEST(a[4] == 5);
    BOOST_TEST(&a[3] == &a[0] + 3);
  }
}

template <class Devector> void test_at()
{
   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::value_type T;

   { // non-const at
      Devector a; get_range<Devector>(3, a);

      BOOST_TEST(a.at(0) == 1);
      a.at(0) = T(100);
      BOOST_TEST(a.at(0) == 100);

      BOOST_TEST_THROWS((void)a.at(3), out_of_range_t);
   }

   { // const at
      Devector b; get_range<Devector>(3, b);
      const Devector &a = b;

      BOOST_TEST(a.at(0) == 1);

      BOOST_TEST_THROWS((void)a.at(3), out_of_range_t);
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_front()
{
  typedef typename Devector::value_type T;

  { // non-const front
    Devector a; get_range<Devector>(3, a);
    BOOST_TEST(a.front() == 1);
    a.front() = T(100);
    BOOST_TEST(a.front() == 100);
  }

  { // const front
    Devector b; get_range<Devector>(3, b); const Devector &a = b;
    BOOST_TEST(a.front() == 1);
  }
}

template <class Devector> void test_back()
{
  typedef typename Devector::value_type T;

  { // non-const back
    Devector a; get_range<Devector>(3, a);
    BOOST_TEST(a.back() == 3);
    a.back() = T(100);
    BOOST_TEST(a.back() == 100);
  }

  { // const back
    Devector b; get_range<Devector>(3, b); const Devector &a = b;
    BOOST_TEST(a.back() == 3);
  }
}

void test_data()
{
  unsigned c_array[] = {1, 2, 3, 4};

  { // non-const data
    devector<unsigned> a(c_array, c_array + 4);
    BOOST_TEST(a.data() == &a.front());

    BOOST_TEST(std::memcmp(c_array, a.data(), 4 * sizeof(unsigned)) == 0);

    *(a.data()) = 100;
    BOOST_TEST(a.front() == 100u);
  }

  { // const data
    const devector<unsigned> a(c_array, c_array + 4);
    BOOST_TEST(a.data() == &a.front());

    BOOST_TEST(std::memcmp(c_array, a.data(), 4 * sizeof(unsigned)) == 0);
  }
}

template <class Devector> void test_emplace_front(dtl::true_)
{
   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::iterator iterator;

   Devector b; get_range<Devector>(4, b);
   iterator origi_begin = b.begin();

   test_elem_throw::on_ctor_after(1);
   BOOST_TEST_THROWS(b.emplace_front(404), test_exception);
   test_elem_throw::do_not_throw();

   iterator new_begin = b.begin();

   BOOST_TEST(origi_begin == new_begin);
   BOOST_TEST(b.size() == 4u);
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_emplace_front(dtl::false_)
{
}

template <class Devector> void test_emplace_front()
{
  typedef typename Devector::value_type T;

  {
    Devector a;

    a.emplace_front(3);
    a.emplace_front(2);
    a.emplace_front(1);

    boost::container::vector<int> expected; get_range<boost::container::vector<int> >(3, expected);

    test_equal_range(a, expected);
  }

   test_emplace_front<Devector>
      (dtl::bool_<!boost::move_detail::is_nothrow_default_constructible<T>::value>());
}

template <class Devector> void test_push_front()
{
   typedef typename Devector::value_type T;
   {
      boost::container::vector<int> expected; get_range<boost::container::vector<int> >(16, expected);
      std::reverse(expected.begin(), expected.end());
      Devector a;

      for (int i = 1; i <= 16; ++i)
      {
      T elem(i);
      a.push_front(elem);
      }

      test_equal_range(a, expected);
   }

   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::iterator iterator;
   if (! boost::move_detail::is_nothrow_copy_constructible<T>::value)
   {
      Devector b; get_range<Devector>(4, b);
      iterator origi_begin = b.begin();

      const T elem(404);

      test_elem_throw::on_copy_after(1);
      BOOST_TEST_THROWS(b.push_front(elem), test_exception);
      test_elem_throw::do_not_throw();

      iterator new_begin = b.begin();

      BOOST_TEST(origi_begin == new_begin);
      BOOST_TEST(b.size() == 4u);
   }

   // test when tmp is already inserted
   {
      Devector c; get_range<Devector>(4, c);
      const T& tmp = *(c.begin() + 1);
      c.push_front(tmp);
      const int expected[] = {2, 1, 2, 3, 4};
      test_equal_range(c, expected);
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_push_front_rvalue_throwing(dtl::true_)
{
   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::value_type T;
   typedef typename Devector::iterator iterator;

   Devector b; get_range<Devector>(4, b);
   iterator origi_begin = b.begin();

   test_elem_throw::on_move_after(1);
   BOOST_TEST_THROWS(b.push_front(T(404)), test_exception);
   test_elem_throw::do_not_throw();

   iterator new_begin = b.begin();

   BOOST_TEST(origi_begin == new_begin);
   BOOST_TEST(b.size() == 4u);
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_push_front_rvalue_throwing(dtl::false_)
{}

template <class Devector> void test_push_front_rvalue()
{
  typedef typename Devector::value_type T;

  {
    boost::container::vector<int> expected; get_range<boost::container::vector<int> >(16, expected);
    Devector a;

    for (int i = 16; i > 0; --i)
    {
      T elem(i);
      a.push_front(boost::move(elem));
    }

    test_equal_range(a, expected);
  }

  test_push_front_rvalue_throwing<Devector>(dtl::bool_<! boost::is_nothrow_move_constructible<T>::value>());
}

template <class Devector> void test_pop_front()
{
  {
    Devector a;
    a.emplace_front(1);
    a.pop_front();
    BOOST_TEST(a.empty());
  }

  {
    Devector b;

    b.emplace_back(2);
    b.pop_front();
    BOOST_TEST(b.empty());

    b.emplace_front(3);
    b.pop_front();
    BOOST_TEST(b.empty());
  }

  {
    Devector c; get_range<Devector>(20, c);
    for (int i = 0; i < 20; ++i)
    {
      BOOST_TEST(!c.empty());
      c.pop_front();
    }
    BOOST_TEST(c.empty());
  }
}

template <class Devector> void test_emplace_back_throwing(dtl::true_)
{
   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::iterator iterator;

   Devector b; get_range<Devector>(4, b);
   iterator origi_begin = b.begin();

   test_elem_throw::on_ctor_after(1);
   BOOST_TEST_THROWS(b.emplace_back(404), test_exception);
   test_elem_throw::do_not_throw();

   iterator new_begin = b.begin();

   BOOST_TEST(origi_begin == new_begin);
   BOOST_TEST(b.size() == 4u);
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_emplace_back_throwing(dtl::false_)
{}

template <class Devector> void test_emplace_back()
{
  typedef typename Devector::value_type T;

  {
    Devector a;

    a.emplace_back(1);
    a.emplace_back(2);
    a.emplace_back(3);

    boost::container::vector<int> expected; get_range<boost::container::vector<int> >(3, expected);

    test_equal_range<Devector>(a, expected);
  }

   test_emplace_back_throwing<Devector>
      (dtl::bool_<! boost::move_detail::is_nothrow_default_constructible<T>::value>());
}

template <class Devector> void test_push_back_throwing(dtl::true_)
{
   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::value_type T;
   typedef typename Devector::iterator iterator;

   Devector b; get_range<Devector>(4, b);
   iterator origi_begin = b.begin();

   const T elem(404);

   test_elem_throw::on_copy_after(1);
   BOOST_TEST_THROWS(b.push_back(elem), test_exception);
   test_elem_throw::do_not_throw();

   iterator new_begin = b.begin();

   BOOST_TEST(origi_begin == new_begin);
   BOOST_TEST(b.size() == 4u);
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_push_back_throwing(dtl::false_)
{}

template <class Devector> void test_push_back()
{
   typedef typename Devector::value_type T;
   {
      boost::container::vector<int> expected; get_range<boost::container::vector<int> >(16, expected);
      Devector a;

      for (int i = 1; i <= 16; ++i)
      {
         T elem(i);
         a.push_back(elem);
      }

      test_equal_range(a, expected);
   }

   test_push_back_throwing<Devector>(dtl::bool_<! boost::move_detail::is_nothrow_copy_constructible<T>::value>());

   // test when tmp is already inserted
   {
      Devector c; get_range<Devector>(4, c);
      const T& tmp = *(c.begin() + 1);
      c.push_back(tmp);
      const int expected[] = {1, 2, 3, 4, 2};
      test_equal_range(c, expected);
   }
}

template <class Devector> void test_push_back_rvalue_throwing(dtl::true_)
{
   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::value_type T;
   typedef typename Devector::iterator iterator;

   Devector b; get_range<Devector>(4, b);
   iterator origi_begin = b.begin();

   test_elem_throw::on_move_after(1);
   BOOST_TEST_THROWS(b.push_back(T(404)), test_exception);
   test_elem_throw::do_not_throw();

   iterator new_begin = b.begin();

   BOOST_TEST(origi_begin == new_begin);
   BOOST_TEST(b.size() == 4u);
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_push_back_rvalue_throwing(dtl::false_)
{}

template <class Devector> void test_push_back_rvalue()
{
  typedef typename Devector::value_type T;

  {
    boost::container::vector<int> expected; get_range<boost::container::vector<int> >(16, expected);
    Devector a;

    for (int i = 1; i <= 16; ++i)
    {
      T elem(i);
      a.push_back(boost::move(elem));
    }

    test_equal_range(a, expected);
  }

  test_push_back_rvalue_throwing<Devector>(dtl::bool_<! boost::is_nothrow_move_constructible<T>::value>());
}

/*
template <class Devector> void test_unsafe_push_front()
{
   typedef typename Devector::value_type T;
   typedef typename Devector::iterator iterator;

   {
      boost::container::vector<int> expected; get_range<boost::container::vector<int> >(16, expected);
      std::reverse(expected.begin(), expected.end());
      Devector a;
      a.reserve_front(16);

      for (std::size_t i = 1; i <= 16; ++i)
      {
      T elem(i);
      a.unsafe_push_front(elem);
      }

      test_equal_range(a, expected);
   }

   #ifndef BOOST_NO_EXCEPTIONS
   if (! boost::move_detail::is_nothrow_copy_constructible<T>::value)
   {
      Devector b; get_range<Devector>(4, b);
      b.reserve_front(5);
      iterator origi_begin = b.begin();

      const T elem(404);

      test_elem_throw::on_copy_after(1);
      BOOST_TEST_THROWS(b.unsafe_push_front(elem), test_exception);
      test_elem_throw::do_not_throw();

      iterator new_begin = b.begin();

      BOOST_TEST(origi_begin == new_begin);
      BOOST_TEST(b.size() == 4u);
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_unsafe_push_front_rvalue()
{
  typedef typename Devector::value_type T;

  {
    boost::container::vector<int> expected; get_range<boost::container::vector<int> >(16, expected);
    std::reverse(expected.begin(), expected.end());
    Devector a;
    a.reserve_front(16);

    for (std::size_t i = 1; i <= 16; ++i)
    {
      T elem(i);
      a.unsafe_push_front(boost::move(elem));
    }

    test_equal_range(a, expected);
  }
}
*/
/*
template <class Devector> void test_unsafe_push_back()
{
   typedef typename Devector::value_type T;
   typedef typename Devector::iterator iterator;

   {
      boost::container::vector<int> expected; get_range<boost::container::vector<int> >(16, expected);
      Devector a;
      a.reserve(16);

      for (std::size_t i = 1; i <= 16; ++i)
      {
      T elem(i);
      a.unsafe_push_back(elem);
      }

      test_equal_range(a, expected);
   }

   #ifndef BOOST_NO_EXCEPTIONS
   if (! boost::move_detail::is_nothrow_copy_constructible<T>::value)
   {
      Devector b; get_range<Devector>(4, b);
      b.reserve(5);
      iterator origi_begin = b.begin();

      const T elem(404);

      test_elem_throw::on_copy_after(1);
      BOOST_TEST_THROWS(b.unsafe_push_back(elem), test_exception);
      test_elem_throw::do_not_throw();

      iterator new_begin = b.begin();

      BOOST_TEST(origi_begin == new_begin);
      BOOST_TEST(b.size() == 4u);
   }
   #endif
}

template <class Devector> void test_unsafe_push_back_rvalue()
{
  typedef typename Devector::value_type T;

  {
    boost::container::vector<int> expected; get_range<boost::container::vector<int> >(16, expected);
    Devector a;
    a.reserve(16);

    for (std::size_t i = 1; i <= 16; ++i)
    {
      T elem(i);
      a.unsafe_push_back(boost::move(elem));
    }

    test_equal_range(a, expected);
  }
}
*/
template <class Devector> void test_pop_back()
{
  {
    Devector a;
    a.emplace_back(1);
    a.pop_back();
    BOOST_TEST(a.empty());
  }

  {
    Devector b;

    b.emplace_front(2);
    b.pop_back();
    BOOST_TEST(b.empty());

    b.emplace_back(3);
    b.pop_back();
    BOOST_TEST(b.empty());
  }

  {
    Devector c; get_range<Devector>(20, c);
    for (int i = 0; i < 20; ++i)
    {
      BOOST_TEST(!c.empty());
      c.pop_back();
    }
    BOOST_TEST(c.empty());
  }
}

template <class Devector> void test_emplace_throwing(dtl::true_)
{
   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::iterator iterator;

   Devector j; get_range<Devector>(4, j);
   iterator origi_begin = j.begin();

   test_elem_throw::on_ctor_after(1);
   BOOST_TEST_THROWS(j.emplace(j.begin() + 2, 404), test_exception);
   test_elem_throw::do_not_throw();

   const int expected[] = {1, 2, 3, 4};
   test_equal_range(j, expected);
   BOOST_TEST(origi_begin == j.begin());
   #endif
}

template <class Devector> void test_emplace_throwing(dtl::false_)
{}


template <class Devector> void test_emplace()
{
   typedef typename Devector::iterator iterator;

  {
    Devector a; get_range<Devector>(16, a);
    typename Devector::iterator it = a.emplace(a.begin(), 123);
    const int expected[] = {123, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    test_equal_range(a, expected);
    BOOST_TEST(*it == 123);
  }

  {
    Devector b; get_range<Devector>(16, b);
    typename Devector::iterator it = b.emplace(b.end(), 123);
    const int expected [] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 123};
    test_equal_range(b, expected);
    BOOST_TEST(*it == 123);
  }

  {
    Devector c; get_range<Devector>(16, c);
    c.pop_front();
    typename Devector::iterator it = c.emplace(c.begin(), 123);
    const int expected [] = {123, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    test_equal_range(c, expected);
    BOOST_TEST(*it == 123);
  }

  {
    Devector d; get_range<Devector>(16, d);
    d.pop_back();
    typename Devector::iterator it = d.emplace(d.end(), 123);
    const int expected [] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 123};
    test_equal_range(d, expected);
    BOOST_TEST(*it == 123);
  }

  {
    Devector e; get_range<Devector>(16, e);
    typename Devector::iterator it = e.emplace(e.begin() + 5, 123);
    const int expected [] = {1, 2, 3, 4, 5, 123, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    test_equal_range(e, expected);
    BOOST_TEST(*it == 123);
  }

  {
    Devector f; get_range<Devector>(16, f);
    f.pop_front();
    f.pop_back();
    iterator valid = f.begin() + 1;
    typename Devector::iterator it = f.emplace(f.begin() + 1, 123);
    const int expected [] = {2, 123, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    test_equal_range(f, expected);
    BOOST_TEST(*it == 123);
    BOOST_TEST(*valid == 3);
  }

  {
    Devector g; get_range<Devector>(16, g);
    g.pop_front();
    g.pop_back();
    iterator valid = g.end() - 2;
    typename Devector::iterator it = g.emplace(g.end() - 1, 123);
    const int expected[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 123, 15};
    test_equal_range(g, expected);
    BOOST_TEST(*it == 123);
    BOOST_TEST(*valid == 14);
  }

  {
    Devector h; get_range<Devector>(16, h);
    h.pop_front();
    h.pop_back();
    iterator valid = h.begin() + 7;
    typename Devector::iterator it = h.emplace(h.begin() + 7, 123);
    const int expected[] = {2, 3, 4, 5, 6, 7, 8, 123, 9, 10, 11, 12, 13, 14, 15};
    test_equal_range(h, expected);
    BOOST_TEST(*it == 123);
    BOOST_TEST(*valid == 9);
  }

  {
    Devector i;
    i.emplace(i.begin(), 1);
    i.emplace(i.end(), 10);
    for (int j = 2; j < 10; ++j)
    {
      i.emplace(i.begin() + (j-1), j);
    }
    const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    test_equal_range(i, expected);
  }

   typedef typename Devector::value_type T;
   test_emplace_throwing<Devector>
      (dtl::bool_<! boost::move_detail::is_nothrow_default_constructible<T>::value>());
}

template <class Devector> void test_insert_throwing(dtl::true_)
{
   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::value_type T;
   typedef typename Devector::iterator iterator;

   T test_elem(123);

   Devector j; get_range<Devector>(4, j);
   iterator origi_begin = j.begin();

   test_elem_throw::on_copy_after(1);
   BOOST_TEST_THROWS(j.insert(j.begin() + 2, test_elem), test_exception);
   test_elem_throw::do_not_throw();

   const int expected[] = {1, 2, 3, 4};
   test_equal_range(j, expected);
   BOOST_TEST(origi_begin == j.begin());
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_insert_throwing(dtl::false_)
{}

template <class Devector> void test_insert()
{
   typedef typename Devector::value_type T;
   typedef typename Devector::iterator iterator;

   T test_elem(123);

   {
      Devector a; get_range<Devector>(16, a);
      typename Devector::iterator it = a.insert(a.begin(), test_elem);
      const int expected[] = {123, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
      test_equal_range(a, expected);
      BOOST_TEST(*it == 123);
   }

   {
      Devector b; get_range<Devector>(16, b);
      typename Devector::iterator it = b.insert(b.end(), test_elem);
      const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 123};
      test_equal_range(b, expected);
      BOOST_TEST(*it == 123);
   }

   {
      Devector c; get_range<Devector>(16, c);
      c.pop_front();
      typename Devector::iterator it = c.insert(c.begin(), test_elem);
      const int expected[] = {123, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
      test_equal_range(c, expected);
      BOOST_TEST(*it == 123);
   }

   {
      Devector d; get_range<Devector>(16, d);
      d.pop_back();
      typename Devector::iterator it = d.insert(d.end(), test_elem);
      const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 123};
      test_equal_range(d, expected);
      BOOST_TEST(*it == 123);
   }

   {
      Devector e; get_range<Devector>(16, e);
      typename Devector::iterator it = e.insert(e.begin() + 5, test_elem);
      const int expected[] = {1, 2, 3, 4, 5, 123, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
      test_equal_range(e, expected);
      BOOST_TEST(*it == 123);
   }

   {
      Devector f; get_range<Devector>(16, f);
      f.pop_front();
      f.pop_back();
      iterator valid = f.begin() + 1;
      typename Devector::iterator it = f.insert(f.begin() + 1, test_elem);
      const int expected[] = {2, 123, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
      test_equal_range(f, expected);
      BOOST_TEST(*it == 123);
      BOOST_TEST(*valid == 3);
   }

   {
      Devector g; get_range<Devector>(16, g);
      g.pop_front();
      g.pop_back();
      iterator valid = g.end() - 2;
      typename Devector::iterator it = g.insert(g.end() - 1, test_elem);
      const int expected[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 123, 15};
      test_equal_range(g, expected);
      BOOST_TEST(*it == 123);
      BOOST_TEST(*valid == 14);
   }

   {
      Devector h; get_range<Devector>(16, h);
      h.pop_front();
      h.pop_back();
      iterator valid = h.begin() + 7;
      typename Devector::iterator it = h.insert(h.begin() + 7, test_elem);
      const int expected[] = {2, 3, 4, 5, 6, 7, 8, 123, 9, 10, 11, 12, 13, 14, 15};
      test_equal_range(h, expected);
      BOOST_TEST(*it == 123);
      BOOST_TEST(*valid == 9);
   }

   {
      Devector i;
      i.insert(i.begin(), T(1));
      i.insert(i.end(), T(10));
      for (int j = 2; j < 10; ++j)
      {
         i.insert(i.begin() + (j-1), T(j));
      }
      const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
      test_equal_range(i, expected);
   }

   test_insert_throwing<Devector>
      (dtl::bool_<! boost::move_detail::is_nothrow_copy_constructible<T>::value>());

   // test when tmp is already inserted and there's free capacity
   {
      Devector c; get_range<Devector>(6, c);
      c.pop_back();
      const T& tmp = *(c.begin() + 2);
      c.insert(c.begin() + 1, tmp);
      const int expected[] = {1, 3, 2, 3, 4, 5};
      test_equal_range(c, expected);
   }

   // test when tmp is already inserted and maybe there's no free capacity
   {
      Devector c; get_range<Devector>(6, c);
      const T& tmp = *(c.begin() + 2);
      c.insert(c.begin() + 1, tmp);
      const int expected[] = {1, 3, 2, 3, 4, 5, 6};
      test_equal_range(c, expected);
   }
}

template <class Devector> void test_insert_rvalue_throwing(dtl::true_)
{
   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::value_type T;
   typedef typename Devector::iterator iterator;

   Devector j; get_range<Devector>(4, j);
   iterator origi_begin = j.begin();

   test_elem_throw::on_ctor_after(1);
   BOOST_TEST_THROWS(j.insert(j.begin() + 2, T(404)), test_exception);
   test_elem_throw::do_not_throw();

   const int expected[] = {1, 2, 3, 4};
   test_equal_range(j, expected);
   BOOST_TEST(origi_begin == j.begin());
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_insert_rvalue_throwing(dtl::false_)
{}


template <class Devector> void test_insert_rvalue()
{
   typedef typename Devector::value_type T;
   typedef typename Devector::iterator iterator;

   {
      Devector a; get_range<Devector>(16, a);
      typename Devector::iterator it = a.insert(a.begin(), T(123));
      const int expected[] = {123, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
      test_equal_range(a, expected);
      BOOST_TEST(*it == 123);
   }

   {
      Devector b; get_range<Devector>(16, b);
      typename Devector::iterator it = b.insert(b.end(), T(123));
      const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 123};
      test_equal_range(b, expected);
      BOOST_TEST(*it == 123);
   }

  {
    Devector c; get_range<Devector>(16, c);
    c.pop_front();
    typename Devector::iterator it = c.insert(c.begin(), T(123));
    const int expected[] = {123, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    test_equal_range(c, expected);
    BOOST_TEST(*it == 123);
  }

  {
    Devector d; get_range<Devector>(16, d);
    d.pop_back();
    typename Devector::iterator it = d.insert(d.end(), T(123));
    const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 123};
    test_equal_range(d, expected);
    BOOST_TEST(*it == 123);
  }

  {
    Devector e; get_range<Devector>(16, e);
    typename Devector::iterator it = e.insert(e.begin() + 5, T(123));
    const int expected[] = {1, 2, 3, 4, 5, 123, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    test_equal_range(e, expected);
    BOOST_TEST(*it == 123);
  }

  {
    Devector f; get_range<Devector>(16, f);
    f.pop_front();
    f.pop_back();
    iterator valid = f.begin() + 1;
    typename Devector::iterator it = f.insert(f.begin() + 1, T(123));
    const int expected[] = {2, 123, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    test_equal_range(f, expected);
    BOOST_TEST(*it == 123);
    BOOST_TEST(*valid == 3);
  }

  {
    Devector g; get_range<Devector>(16, g);
    g.pop_front();
    g.pop_back();
    iterator valid = g.end() - 2;
    typename Devector::iterator it = g.insert(g.end() - 1, T(123));
    const int expected[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 123, 15};
    test_equal_range(g, expected);
    BOOST_TEST(*it == 123);
    BOOST_TEST(*valid == 14);
  }

  {
    Devector h; get_range<Devector>(16, h);
    h.pop_front();
    h.pop_back();
    iterator valid = h.begin() + 7;
    typename Devector::iterator it = h.insert(h.begin() + 7, T(123));
    const int expected[] = {2, 3, 4, 5, 6, 7, 8, 123, 9, 10, 11, 12, 13, 14, 15};
    test_equal_range(h, expected);
    BOOST_TEST(*it == 123);
    BOOST_TEST(*valid == 9);
  }

  {
    Devector i;
    i.insert(i.begin(), T(1));
    i.insert(i.end(), T(10));
    for (int j = 2; j < 10; ++j)
    {
      i.insert(i.begin() + (j-1), T(j));
    }
    const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    test_equal_range(i, expected);
  }

   test_insert_rvalue_throwing<Devector>
      (dtl::bool_<! boost::move_detail::is_nothrow_default_constructible<T>::value>());
}

template <class Devector> void test_insert_n_throwing(dtl::true_)
{
   #ifndef BOOST_NO_EXCEPTIONS
   typedef typename Devector::value_type T;
   // insert at begin
   {
      Devector j; get_range<Devector>(4, j);

      const T x(404);

      test_elem_throw::on_copy_after(3);
      BOOST_TEST_THROWS(j.insert(j.begin(), 4, x), test_exception);
      test_elem_throw::do_not_throw();
   }

   // insert at end
   {
      Devector k; get_range<Devector>(4, k);

      const T x(404);

      test_elem_throw::on_copy_after(3);
      BOOST_TEST_THROWS(k.insert(k.end(), 4, x), test_exception);
      test_elem_throw::do_not_throw();
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_insert_n_throwing(dtl::false_)
{}

template <class Devector> void test_insert_n()
{
   typedef typename Devector::value_type T;
   typedef typename Devector::iterator iterator;

   {
      Devector a;
      const T x(123);
      iterator ret = a.insert(a.end(), 5, x);
      const int expected[] = {123, 123, 123, 123, 123};
      test_equal_range(a, expected);
      BOOST_TEST(ret == a.begin());
   }

   {
      Devector b; get_range<Devector>(8, b);
      const T x(9);
      iterator ret = b.insert(b.begin(), 3, x);
      const int expected[] = {9, 9, 9, 1, 2, 3, 4, 5, 6, 7, 8};
      test_equal_range(b, expected);
      BOOST_TEST(ret == b.begin());
   }

   {
      Devector c; get_range<Devector>(8, c);
      const T x(9);
      iterator ret = c.insert(c.end(), 3, x);
      const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 9};
      test_equal_range(c, expected);
      BOOST_TEST(ret == c.begin() + 8);
   }

   {
      Devector d; get_range<Devector>(8, d);

      d.pop_front();
      d.pop_front();
      d.pop_front();

      const T x(9);
      iterator origi_end = d.end();
      iterator ret = d.insert(d.begin(), 3, x);

      const int expected[] = {9, 9, 9, 4, 5, 6, 7, 8};
      test_equal_range(d, expected);
      BOOST_TEST(origi_end == d.end());
      BOOST_TEST(ret == d.begin());
   }

   {
      Devector e; get_range<Devector>(8, e);

      e.pop_back();
      e.pop_back();
      e.pop_back();

      const T x(9);
      iterator origi_begin = e.begin();
      iterator ret = e.insert(e.end(), 3, x);

      const int expected[] = {1, 2, 3, 4, 5, 9, 9, 9};
      test_equal_range(e, expected);
      BOOST_TEST(origi_begin == e.begin());
      BOOST_TEST(ret == e.begin() + 5);
   }

   {
      Devector f; get_range<Devector>(8, f);
      f.reset_alloc_stats();

      f.pop_front();
      f.pop_front();
      f.pop_back();
      f.pop_back();

      const T x(9);
      iterator ret = f.insert(f.begin() + 2, 4, x);

      const int expected[] = {3, 4, 9, 9, 9, 9, 5, 6};
      test_equal_range(f, expected);
      BOOST_TEST(f.get_alloc_count() == 0u);
      BOOST_TEST(ret == f.begin() + 2);
   }

   {
      Devector g; get_range<Devector>(8, g);
      g.reset_alloc_stats();

      g.pop_front();
      g.pop_front();
      g.pop_back();
      g.pop_back();
      g.pop_back();

      const T x(9);
      iterator ret = g.insert(g.begin() + 2, 5, x);

      const int expected[] = {3, 4, 9, 9, 9, 9, 9, 5};
      test_equal_range(g, expected);
      BOOST_TEST(g.get_alloc_count() == 0u);
      BOOST_TEST(ret == g.begin() + 2);
   }

   {
      Devector g; get_range<Devector>(8, g);

      const T x(9);
      iterator ret = g.insert(g.begin() + 2, 5, x);

      const int expected[] = {1, 2, 9, 9, 9, 9, 9, 3, 4, 5, 6, 7, 8};
      test_equal_range(g, expected);
      BOOST_TEST(ret == g.begin() + 2);
   }

   { // n == 0
      Devector h; get_range<Devector>(8, h);
      h.reset_alloc_stats();

      const T x(9);

      iterator ret = h.insert(h.begin(), 0, x);
      BOOST_TEST(ret == h.begin());

      ret = h.insert(h.begin() + 4, 0, x);
      BOOST_TEST(ret == h.begin() + 4);

      ret = h.insert(h.end(), 0, x);
      BOOST_TEST(ret == h.end());

      const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8};
      test_equal_range(h, expected);
      BOOST_TEST(h.get_alloc_count() == 0u);
   }

   { // test insert already inserted
      Devector i; get_range<Devector>(8, i);
      i.reset_alloc_stats();

      i.pop_front();
      i.pop_front();

      iterator ret = i.insert(i.end() - 1, 2, *i.begin());

      const int expected[] = {3, 4, 5, 6, 7, 3, 3, 8};
      test_equal_range(i, expected);
      BOOST_TEST(i.get_alloc_count() == 0u);
      BOOST_TEST(ret == i.begin() + 5);
   }

   test_insert_n_throwing<Devector>
      (dtl::bool_<! boost::move_detail::is_nothrow_copy_constructible<T>::value>());
}

template <class Devector> void test_insert_input_range()
{
   typedef typename Devector::value_type T;
   typedef typename Devector::iterator iterator;

   devector<T> x;
   x.emplace_back(9);
   x.emplace_back(9);
   x.emplace_back(9);
   x.emplace_back(9);
   x.emplace_back(9);

   {
      devector<T> input = x;

      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
      input_iterator<Devector> input_end   = make_input_iterator(input, input.begin() + 5);

      Devector a;
      iterator ret = a.insert(a.end(), input_begin, input_end);
      const int expected[] = {9, 9, 9, 9, 9};
      test_equal_range(a, expected);
      BOOST_TEST(ret == a.begin());
   }

   {
      devector<T> input = x;

      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
      input_iterator<Devector> input_end   = make_input_iterator(input, input.begin() + 3);

      Devector b; get_range<Devector>(8, b);
      iterator ret = b.insert(b.begin(), input_begin, input_end);
      const int expected[] = {9, 9, 9, 1, 2, 3, 4, 5, 6, 7, 8};
      test_equal_range(b, expected);
      BOOST_TEST(ret == b.begin());
   }

   {
      devector<T> input = x;

      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
      input_iterator<Devector> input_end   = make_input_iterator(input, input.begin() + 3);

      Devector c; get_range<Devector>(8, c);
      iterator ret = c.insert(c.end(), input_begin, input_end);
      const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 9, 9};
      test_equal_range(c, expected);
      BOOST_TEST(ret == c.begin() + 8);
   }

   {
      devector<T> input = x;

      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
      input_iterator<Devector> input_end   = make_input_iterator(input, input.begin() + 3);

      Devector d; get_range<Devector>(8, d);

      d.pop_front();
      d.pop_front();
      d.pop_front();

      iterator ret = d.insert(d.begin(), input_begin, input_end);
      const int expected[] = {9, 9, 9, 4, 5, 6, 7, 8};
      test_equal_range(d, expected);
      BOOST_TEST(ret == d.begin());
   }

   {
      devector<T> input = x;

      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
      input_iterator<Devector> input_end   = make_input_iterator(input, input.begin() + 3);

      Devector e; get_range<Devector>(8, e);

      e.pop_back();
      e.pop_back();
      e.pop_back();

      iterator origi_begin = e.begin();
      iterator ret = e.insert(e.end(), input_begin, input_end);
      const int expected[] = {1, 2, 3, 4, 5, 9, 9, 9};
      test_equal_range(e, expected);
      BOOST_TEST(origi_begin == e.begin());
      BOOST_TEST(ret == e.begin() + 5);
   }

   {
      devector<T> input = x;

      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
      input_iterator<Devector> input_end   = make_input_iterator(input, input.begin() + 4);

      Devector f; get_range<Devector>(8, f);
      f.reset_alloc_stats();

      f.pop_front();
      f.pop_front();
      f.pop_back();
      f.pop_back();

      iterator ret = f.insert(f.begin() + 2, input_begin, input_end);

      const int expected[] = {3, 4, 9, 9, 9, 9, 5, 6};
      test_equal_range(f, expected);
      BOOST_TEST(ret == f.begin() + 2);
   }

   {
      devector<T> input = x;

      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
      input_iterator<Devector> input_end   = make_input_iterator(input, input.begin() + 5);

      Devector g; get_range<Devector>(8, g);
      g.reset_alloc_stats();

      g.pop_front();
      g.pop_front();
      g.pop_back();
      g.pop_back();
      g.pop_back();

      iterator ret = g.insert(g.begin() + 2, input_begin, input_end);

      const int expected [] = {3, 4, 9, 9, 9, 9, 9, 5};
      test_equal_range(g, expected);
      BOOST_TEST(ret == g.begin() + 2);
   }

   {
      devector<T> input = x;

      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
      input_iterator<Devector> input_end   = make_input_iterator(input, input.begin() + 5);

      Devector g; get_range<Devector>(8, g);

      iterator ret = g.insert(g.begin() + 2, input_begin, input_end);

      const int expected [] = {1, 2, 9, 9, 9, 9, 9, 3, 4, 5, 6, 7, 8};
      test_equal_range(g, expected);
      BOOST_TEST(ret == g.begin() + 2);
   }

   { // n == 0
      devector<T> input = x;

      input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());

      Devector h; get_range<Devector>(8, h);
      h.reset_alloc_stats();

      iterator ret = h.insert(h.begin(), input_begin, input_begin);
      BOOST_TEST(ret == h.begin());

      ret = h.insert(h.begin() + 4, input_begin, input_begin);
      BOOST_TEST(ret == h.begin() + 4);

      ret = h.insert(h.end(), input_begin, input_begin);
      BOOST_TEST(ret == h.end());

      const int expected [] = {1, 2, 3, 4, 5, 6, 7, 8};
      test_equal_range(h, expected);
   }

   #ifndef BOOST_NO_EXCEPTIONS
   if (! boost::move_detail::is_nothrow_copy_constructible<T>::value)
   {
      // insert at begin
      {
         devector<T> input = x;

         input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
         input_iterator<Devector> input_end   = make_input_iterator(input, input.begin() + 4);

         Devector j; get_range<Devector>(4, j);

         test_elem_throw::on_copy_after(3);
         BOOST_TEST_THROWS(j.insert(j.begin(), input_begin, input_end), test_exception);
         test_elem_throw::do_not_throw();
      }

      // insert at end
      {
         devector<T> input = x;

         input_iterator<Devector> input_begin = make_input_iterator(input, input.begin());
         input_iterator<Devector> input_end   = make_input_iterator(input, input.begin() + 4);

         Devector k; get_range<Devector>(4, k);

         test_elem_throw::on_copy_after(3);
         BOOST_TEST_THROWS(k.insert(k.end(), input_begin, input_end), test_exception);
         test_elem_throw::do_not_throw();
      }
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_insert_range()
{
   typedef typename Devector::value_type T;
   typedef typename Devector::iterator iterator;
   typedef boost::container::vector<T> Vector;

   Vector x;
   x.emplace_back(9);
   x.emplace_back(10);
   x.emplace_back(11);
   x.emplace_back(12);
   x.emplace_back(13);

   typename Vector::iterator xb = x.begin();

   {
      Devector a;
      iterator ret = a.insert(a.end(), xb, xb+5);
      const int expected [] = {9, 10, 11, 12, 13};
      test_equal_range(a, expected);
      BOOST_TEST(ret == a.begin());
   }

   {
      Devector b; get_range<Devector>(8, b);
      iterator ret = b.insert(b.begin(), xb, xb+3);
      const int expected [] = {9, 10, 11, 1, 2, 3, 4, 5, 6, 7, 8};
      test_equal_range(b, expected);
      BOOST_TEST(ret == b.begin());
   }

   {
      Devector c; get_range<Devector>(8, c);
      iterator ret = c.insert(c.end(), xb, xb+3);
      const int expected [] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
      test_equal_range(c, expected);
      BOOST_TEST(ret == c.begin() + 8);
   }

   {
      Devector d; get_range<Devector>(8, d);

      d.pop_front();
      d.pop_front();
      d.pop_front();

      iterator origi_end = d.end();
      iterator ret = d.insert(d.begin(), xb, xb+3);

      const int expected [] = {9, 10, 11, 4, 5, 6, 7, 8};
      test_equal_range(d, expected);

      BOOST_TEST(origi_end == d.end());
      BOOST_TEST(ret == d.begin());
   }

   {
      Devector e; get_range<Devector>(8, e);

      e.pop_back();
      e.pop_back();
      e.pop_back();

      iterator origi_begin = e.begin();
      iterator ret = e.insert(e.end(), xb, xb+3);

      const int expected [] = {1, 2, 3, 4, 5, 9, 10, 11};
      test_equal_range(e, expected);

      BOOST_TEST(origi_begin == e.begin());
      BOOST_TEST(ret == e.begin() + 5);
   }

   {
      Devector f; get_range<Devector>(8, f);
      f.reset_alloc_stats();

      f.pop_front();
      f.pop_front();
      f.pop_back();
      f.pop_back();

      iterator ret = f.insert(f.begin() + 2, xb, xb+4);

      const int expected [] = {3, 4, 9, 10, 11, 12, 5, 6};
      test_equal_range(f, expected);

      BOOST_TEST(f.get_alloc_count() == 0u);
      BOOST_TEST(ret == f.begin() + 2);
   }

   {
      Devector g; get_range<Devector>(8, g);
      g.reset_alloc_stats();

      g.pop_front();
      g.pop_front();
      g.pop_back();
      g.pop_back();
      g.pop_back();

      iterator ret = g.insert(g.begin() + 2, xb, xb+5);

      const int expected [] = {3, 4, 9, 10, 11, 12, 13, 5};
      test_equal_range(g, expected);

      BOOST_TEST(g.get_alloc_count() == 0u);
      BOOST_TEST(ret == g.begin() + 2);
   }

   {
      Devector g; get_range<Devector>(8, g);

      iterator ret = g.insert(g.begin() + 2, xb, xb+5);

      const int expected [] = {1, 2, 9, 10, 11, 12, 13, 3, 4, 5, 6, 7, 8};
      test_equal_range(g, expected);

      BOOST_TEST(ret == g.begin() + 2);
   }

   { // n == 0
      Devector h; get_range<Devector>(8, h);
      h.reset_alloc_stats();

      iterator ret = h.insert(h.begin(), xb, xb);
      BOOST_TEST(ret == h.begin());

      ret = h.insert(h.begin() + 4, xb, xb);
      BOOST_TEST(ret == h.begin() + 4);

      ret = h.insert(h.end(), xb, xb);
      BOOST_TEST(ret == h.end());

      const int expected [] = {1, 2, 3, 4, 5, 6, 7, 8};
      test_equal_range(h, expected);

      BOOST_TEST(h.get_alloc_count() == 0u);
   }

   #ifndef BOOST_NO_EXCEPTIONS
   if (! boost::move_detail::is_nothrow_copy_constructible<T>::value)
   {
      // insert at begin
      {
         Devector j; get_range<Devector>(4, j);

         test_elem_throw::on_copy_after(3);
         BOOST_TEST_THROWS(j.insert(j.begin(), xb, xb+4), test_exception);
         test_elem_throw::do_not_throw();
      }

      // insert at end
      {
         Devector k; get_range<Devector>(4, k);

         test_elem_throw::on_copy_after(3);
         BOOST_TEST_THROWS(k.insert(k.end(), xb, xb+4), test_exception);
         test_elem_throw::do_not_throw();
      }
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
}

template <class Devector> void test_insert_init_list()
{
   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   typedef typename Devector::value_type T;
   typedef typename Devector::iterator iterator;

   {
      Devector a;
      iterator ret = a.insert(a.end(), {T(123), T(124), T(125), T(126), T(127)});
      test_equal_range(a, {123, 124, 125, 126, 127});
      BOOST_TEST(ret == a.begin());
   }

   {
      Devector b; get_range<Devector>(8, b);
      iterator ret = b.insert(b.begin(), {T(9), T(10), T(11)});
      test_equal_range(b, {9, 10, 11, 1, 2, 3, 4, 5, 6, 7, 8});
      BOOST_TEST(ret == b.begin());
   }

   {
      Devector c; get_range<Devector>(8, c);
      iterator ret = c.insert(c.end(), {T(9), T(10), T(11)});
      test_equal_range(c, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});
      BOOST_TEST(ret == c.begin() + 8);
   }

   {
      Devector d; get_range<Devector>(8, d);

      d.pop_front();
      d.pop_front();
      d.pop_front();

      iterator origi_end = d.end();
      iterator ret = d.insert(d.begin(), {T(9), T(10), T(11)});

      test_equal_range(d, {9, 10, 11, 4, 5, 6, 7, 8});
      BOOST_TEST(origi_end == d.end());
      BOOST_TEST(ret == d.begin());
   }

   {
      Devector e; get_range<Devector>(8, e);

      e.pop_back();
      e.pop_back();
      e.pop_back();

      iterator origi_begin = e.begin();
      iterator ret = e.insert(e.end(), {T(9), T(10), T(11)});

      test_equal_range(e, {1, 2, 3, 4, 5, 9, 10, 11});
      BOOST_TEST(origi_begin == e.begin());
      BOOST_TEST(ret == e.begin() + 5);
   }

   {
      Devector f; get_range<Devector>(8, f);
      f.reset_alloc_stats();

      f.pop_front();
      f.pop_front();
      f.pop_back();
      f.pop_back();

      iterator ret = f.insert(f.begin() + 2, {T(9), T(10), T(11), T(12)});

      test_equal_range(f, {3, 4, 9, 10, 11, 12, 5, 6});
      BOOST_TEST(f.get_alloc_count() == 0u);
      BOOST_TEST(ret == f.begin() + 2);
   }

   {
      Devector g; get_range<Devector>(8, g);
      g.reset_alloc_stats();

      g.pop_front();
      g.pop_front();
      g.pop_back();
      g.pop_back();
      g.pop_back();

      iterator ret = g.insert(g.begin() + 2, {T(9), T(10), T(11), T(12), T(13)});

      test_equal_range(g, {3, 4, 9, 10, 11, 12, 13, 5});
      BOOST_TEST(g.get_alloc_count() == 0u);
      BOOST_TEST(ret == g.begin() + 2);
   }

   {
      Devector g; get_range<Devector>(8, g);

      iterator ret = g.insert(g.begin() + 2, {T(9), T(10), T(11), T(12), T(13)});

      test_equal_range(g, {1, 2, 9, 10, 11, 12, 13, 3, 4, 5, 6, 7, 8});
      BOOST_TEST(ret == g.begin() + 2);
   }

   #ifndef BOOST_NO_EXCEPTIONS
   BOOST_IF_CONSTEXPR (! boost::move_detail::is_nothrow_copy_constructible<T>::value)
   {
      // insert at begin
      {
         Devector j; get_range<Devector>(4, j);

         test_elem_throw::on_copy_after(3);
         BOOST_TEST_THROWS(j.insert(j.begin(), {T(9), T(9), T(9), T(9), T(9)}), test_exception);
         test_elem_throw::do_not_throw();
      }

      // insert at end
      {
         Devector k; get_range<Devector>(4, k);
         test_elem_throw::on_copy_after(3);
         BOOST_TEST_THROWS(k.insert(k.end(), {T(9), T(9), T(9), T(9), T(9)}), test_exception);
         test_elem_throw::do_not_throw();
      }
   }
   #endif   //#ifndef BOOST_NO_EXCEPTIONS
   #endif   //   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
}

template <class Devector> void test_erase()
{
   typedef typename Devector::iterator iterator;
   {
      Devector a; get_range<Devector>(4, a);
      iterator ret = a.erase(a.begin());
      const int expected[] = {2, 3, 4};
      test_equal_range(a, expected);
      BOOST_TEST(ret == a.begin());
   }

   {
      Devector b; get_range<Devector>(4, b);
      iterator ret = b.erase(b.end() - 1);
      const int expected[] = {1, 2, 3};
      test_equal_range(b, expected);
      BOOST_TEST(ret == b.end());
   }

   {
      Devector c; get_range<Devector>(6, c);
      iterator ret = c.erase(c.begin() + 2);
      const int expected [] = {1, 2, 4, 5, 6};
      test_equal_range(c, expected);
      BOOST_TEST(ret == c.begin() + 2);
      BOOST_TEST(c.front_free_capacity() > 0u);
   }

   {
      Devector d; get_range<Devector>(6, d);
      iterator ret = d.erase(d.begin() + 4);
      const int expected [] = {1, 2, 3, 4, 6};
      test_equal_range(d, expected);
      BOOST_TEST(ret == d.begin() + 4);
      BOOST_TEST(d.back_free_capacity() > 0u);
   }
}

template <class Devector> void test_erase_range()
{
   typedef typename Devector::iterator iterator;
   {
      Devector a; get_range<Devector>(4, a);
      a.erase(a.end(), a.end());
      a.erase(a.begin(), a.begin());
   }

   {
      Devector b; get_range<Devector>(8, b);
      iterator ret = b.erase(b.begin(), b.begin() + 2);
      const int expected [] = {3, 4, 5, 6, 7, 8};
      test_equal_range(b, expected);
      BOOST_TEST(ret == b.begin());
      BOOST_TEST(b.front_free_capacity() > 0u);
   }

   {
      Devector c; get_range<Devector>(8, c);
      iterator ret = c.erase(c.begin() + 1, c.begin() + 3);
      const int expected [] = {1, 4, 5, 6, 7, 8};
      test_equal_range(c, expected);
      BOOST_TEST(ret == c.begin() + 1);
      BOOST_TEST(c.front_free_capacity() > 0u);
   }

   {
      Devector d; get_range<Devector>(8, d);
      iterator ret = d.erase(d.end() - 2, d.end());
      const int expected [] = {1, 2, 3, 4, 5, 6};
      test_equal_range(d, expected);
      BOOST_TEST(ret == d.end());
      BOOST_TEST(d.back_free_capacity() > 0u);
   }

   {
      Devector e; get_range<Devector>(8, e);
      iterator ret = e.erase(e.end() - 3, e.end() - 1);
      const int expected [] = {1, 2, 3, 4, 5, 8};
      test_equal_range(e, expected);
      BOOST_TEST(ret == e.end() - 1);
      BOOST_TEST(e.back_free_capacity() > 0u);
   }

   {
      Devector f; get_range<Devector>(8, f);
      iterator ret = f.erase(f.begin(), f.end());
      test_equal_range(f);
      BOOST_TEST(ret == f.end());
   }
}

template <class Devector> void test_swap()
{
   using std::swap; // test if ADL works

   // empty-empty
   {
      Devector a;
      Devector b;

      swap(a, b);

      BOOST_TEST(a.empty());
      BOOST_TEST(b.empty());
   }

   // empty-not empty
   {
      Devector a;
      Devector b; get_range<Devector>(4, b);

      swap(a, b);

      const int expected [] = {1, 2, 3, 4};
   {
      BOOST_TEST(b.empty());
      test_equal_range(a, expected);
   }

      swap(a, b);
   {
      BOOST_TEST(a.empty());
      test_equal_range(b, expected);
   }
   }

   // small-small / big-big
   {
      Devector a; get_range<Devector>(1, 5, 5, 7, a);
      Devector b; get_range<Devector>(13, 15, 15, 19, b);

      swap(a, b);

      const int expected [] = {13, 14, 15, 16, 17, 18};
      test_equal_range(a, expected);
      const int expected2 [] = {1, 2, 3, 4, 5, 6};
      test_equal_range(b, expected2);

      swap(a, b);

      const int expected3 [] = {13, 14, 15, 16, 17, 18};
      test_equal_range(b, expected3);
      const int expected4 [] = {1, 2, 3, 4, 5, 6};
      test_equal_range(a, expected4);
   }

   // big-small + small-big
   {
      Devector a; get_range<Devector>(10, a);
      Devector b; get_range<Devector>(9, 11, 11, 17, b);

      swap(a, b);

      const int expected [] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
      test_equal_range(b, expected);
      const int expected2 [] = {9, 10, 11, 12, 13, 14, 15, 16};
      test_equal_range(a, expected2);

      swap(a, b);

      const int expected3 [] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
      test_equal_range(a, expected3);
      const int expected4 [] = {9, 10, 11, 12, 13, 14, 15, 16};
      test_equal_range(b, expected4);
   }

   // self swap
   {
      Devector a; get_range<Devector>(10, a);

      swap(a, a);

      const int expected [] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
      test_equal_range(a, expected);
   }

   // no overlap
   {
      Devector a; get_range<Devector>(1, 9, 0, 0, a);
      Devector b; get_range<Devector>(0, 0, 11, 17, b);

      a.pop_back();
      a.pop_back();

      b.pop_front();
      b.pop_front();

      swap(a, b);

      const int expected [] = {13, 14, 15, 16};
      test_equal_range(a, expected);
      const int expected2 [] = {1, 2, 3, 4, 5, 6};
      test_equal_range(b, expected2);
   }

   // big-big does not copy or move
   {
      Devector a; get_range<Devector>(32, a);
      Devector b; get_range<Devector>(32, b);
      boost::container::vector<int> c; get_range<boost::container::vector<int> >(32, c);

      test_elem_throw::on_copy_after(1);
      test_elem_throw::on_move_after(1);

      swap(a, b);

      test_elem_throw::do_not_throw();

      test_equal_range(a, c);
      test_equal_range(b, c);
   }
}

template <class Devector> void test_clear()
{
   {
      Devector a;
      a.clear();
      BOOST_TEST(a.empty());
   }

   {
      Devector a; get_range<Devector>(8, a);
      typename Devector::size_type cp = a.capacity();
      a.clear();
      BOOST_TEST(a.empty());
      BOOST_TEST(cp == a.capacity());
   }
}

template <class Devector> void test_op_eq()
{
   { // equal
      Devector a; get_range<Devector>(8, a);
      Devector b; get_range<Devector>(8, b);

      BOOST_TEST(a == b);
   }

   { // diff size
      Devector a; get_range<Devector>(8, a);
      Devector b; get_range<Devector>(9, b);

      BOOST_TEST(!(a == b));
   }

   { // diff content
      Devector a; get_range<Devector>(8, a);
      Devector b; get_range<Devector>(2,6,6,10, b);

      BOOST_TEST(!(a == b));
   }
}

template <class Devector> void test_op_lt()
{
   { // little than
      Devector a; get_range<Devector>(7, a);
      Devector b; get_range<Devector>(8, b);

      BOOST_TEST((a < b));
   }

   { // equal
      Devector a; get_range<Devector>(8, a);
      Devector b; get_range<Devector>(8, b);

      BOOST_TEST(!(a < b));
   }

   { // greater than
      Devector a; get_range<Devector>(8, a);
      Devector b; get_range<Devector>(7, b);

      BOOST_TEST(!(a < b));
   }
}

template <class Devector> void test_op_ne()
{
  { // equal
    Devector a; get_range<Devector>(8, a);
    Devector b; get_range<Devector>(8, b);

    BOOST_TEST(!(a != b));
  }

  { // diff size
    Devector a; get_range<Devector>(8, a);
    Devector b; get_range<Devector>(9, b);

    BOOST_TEST((a != b));
  }

  { // diff content
    Devector a; get_range<Devector>(8, a);
    Devector b; get_range<Devector>(2,6,6,10, b);

    BOOST_TEST((a != b));
  }
}


template <class Devector> void test_op_gt()
{
   { // little than
      Devector a; get_range<Devector>(7, a);
      Devector b; get_range<Devector>(8, b);

      BOOST_TEST(!(a > b));
   }

   { // equal
      Devector a; get_range<Devector>(8, a);
      Devector b; get_range<Devector>(8, b);

      BOOST_TEST(!(a > b));
   }

   { // greater than
      Devector a; get_range<Devector>(8, a);
      Devector b; get_range<Devector>(7, b);

      BOOST_TEST((a > b));
   }
}

template <class Devector> void test_op_ge()
{
   { // little than
      Devector a; get_range<Devector>(7, a);
      Devector b; get_range<Devector>(8, b);

      BOOST_TEST(!(a >= b));
   }

   { // equal
      Devector a; get_range<Devector>(8, a);
      Devector b; get_range<Devector>(8, b);

      BOOST_TEST((a >= b));
   }

   { // greater than
      Devector a; get_range<Devector>(8, a);
      Devector b; get_range<Devector>(7, b);

      BOOST_TEST((a >= b));
   }
}

template <class Devector> void test_op_le()
{
   { // little than
      Devector a; get_range<Devector>(7, a);
      Devector b; get_range<Devector>(8, b);

      BOOST_TEST((a <= b));
   }

   { // equal
      Devector a; get_range<Devector>(8, a);
      Devector b; get_range<Devector>(8, b);

      BOOST_TEST((a <= b));
   }

   { // greater than
      Devector a; get_range<Devector>(8, a);
      Devector b; get_range<Devector>(7, b);

      BOOST_TEST(!(a <= b));
   }
}


template <class Devector>
void test_devector_default_constructible(dtl::true_)
{
   test_constructor_n<Devector>();
   test_resize_front<Devector>();
   test_resize_back<Devector>();
}

template <class Devector>
void test_devector_default_constructible(dtl::false_)
{}

template <class Devector>
void test_devector_copy_constructible(dtl::false_)
{}


template <class Devector>
void test_devector_copy_constructible(dtl::true_)
{
   test_constructor_n_copy<Devector>();
   test_constructor_input_range<Devector>();
   test_constructor_forward_range<Devector>();
   test_constructor_pointer_range<Devector>();
   test_copy_constructor<Devector>();
   test_assignment<Devector>();
   test_assign_input_range<Devector>();
   test_assign_pointer_range<Devector>();
   test_assign_n<Devector>();
   test_resize_front_copy<Devector>();
   test_push_back<Devector>();
   //test_unsafe_push_back<Devector>();
   test_push_front<Devector>();
   //test_unsafe_push_front<Devector>();
   test_resize_back_copy<Devector>();
   test_insert<Devector>();
   test_insert_n<Devector>();
   test_insert_input_range<Devector>();
   test_insert_range<Devector>();
   test_insert_init_list<Devector>();
}

template <class Devector>
void test_devector()
{
   test_devector_default_constructible<Devector>(dtl::bool_<boost::is_default_constructible<typename Devector::value_type>::value>());
   test_devector_copy_constructible<Devector>(dtl::bool_<boost::move_detail::is_copy_constructible<typename Devector::value_type>::value>());

   test_constructor_default<Devector>();
   test_constructor_allocator<Devector>();

   test_constructor_reserve_only<Devector>();
   test_constructor_reserve_only_front_back<Devector>();
   //test_constructor_unsafe_uninitialized<Devector>();

   test_move_constructor<Devector>();
   test_destructor<Devector>();

   test_move_assignment<Devector>();
   test_get_allocator<Devector>();
   test_begin_end<Devector>();
   test_empty<Devector>();
   test_size<Devector>();
   test_capacity<Devector>();

   //test_unsafe_uninitialized_resize_front<Devector>();
   //test_unsafe_uninitialized_resize_back<Devector>();
   test_reserve_front<Devector>();
   test_reserve_back<Devector>();
   test_index_operator<Devector>();
   test_at<Devector>();
   test_front<Devector>();
   test_back<Devector>();
   test_emplace_front<Devector>();
   test_push_front_rvalue<Devector>();

   //test_unsafe_push_front_rvalue<Devector>();
   test_pop_front<Devector>();
   test_emplace_back<Devector>();
   test_push_back_rvalue<Devector>();

   //test_unsafe_push_back_rvalue<Devector>();
   test_pop_back<Devector>();
   test_emplace<Devector>();
   test_insert_rvalue<Devector>();

   test_erase<Devector>();
   test_erase_range<Devector>();
   test_swap<Devector>();
   test_clear<Devector>();
   test_op_eq<Devector>();
   test_op_lt<Devector>();
   test_op_ne<Devector>();
   test_op_gt<Devector>();
   test_op_ge<Devector>();
   test_op_le<Devector>();
}

class recursive_devector
{
   public:
   recursive_devector(const recursive_devector &x)
      : devector_(x.devector_)
   {}

   recursive_devector & operator=(const recursive_devector &x)
   {  this->devector_ = x.devector_;   return *this; }

   int id_;
   devector<recursive_devector> devector_;
   devector<recursive_devector>::iterator it_;
   devector<recursive_devector>::const_iterator cit_;
   devector<recursive_devector>::reverse_iterator rit_;
   devector<recursive_devector>::const_reverse_iterator crit_;
};

void test_recursive_devector()//Test for recursive types
{
   devector<recursive_devector> rdv;
   BOOST_TEST(rdv.empty());
   BOOST_TEST(rdv.get_alloc_count() == 0u);
   BOOST_TEST(rdv.capacity() == 0u);
}

template<class VoidAllocator>
struct GetAllocatorCont
{
   template<class ValueType>
   struct apply
   {
      typedef vector< ValueType
                    , typename allocator_traits<VoidAllocator>
                        ::template portable_rebind_alloc<ValueType>::type
                    > type;
   };
};

#ifdef _MSC_VER
   #pragma warning (pop)
#endif


void test_all()
{/*
   test_recursive_devector();
   test_max_size();
   test_exceeding_max_size();
   shrink_to_fit();
   test_data();
   test_il_assignment< devector<int> >();
   test_assign_forward_range< devector<int> >();
   test_assign_il<devector<int> >();
*/
   //test_devector< devector<int> >();
   test_devector< devector<regular_elem> >();
   test_devector< devector<noex_move> >();
   test_devector< devector<noex_copy> >();
   test_devector< devector<only_movable> >();
   test_devector< devector<no_default_ctor> >();

   ////////////////////////////////////
   //    Allocator propagation testing
   ////////////////////////////////////
   (void)boost::container::test::test_propagate_allocator<boost_container_devector>();
}


boost::container::vector<only_movable> getom()
{
   typedef boost::container::vector<only_movable> V;
   V v;
   return BOOST_MOVE_RET(V, v);
}


boost::container::vector<boost::container::test::movable_int> get()
{
   typedef boost::container::vector<boost::container::test::movable_int> V;
   V v;
   return BOOST_MOVE_RET(V, v);
}

int main()
{
//   boost::container::vector<boost::container::test::movable_int>a(get());
   //boost::container::vector<only_movable> b(getom());
   //boost::container::vector<only_movable> c(get_range< boost::container::vector<only_movable> >(1, 5, 5, 9));
   test_all();
   return boost::report_errors();
}
