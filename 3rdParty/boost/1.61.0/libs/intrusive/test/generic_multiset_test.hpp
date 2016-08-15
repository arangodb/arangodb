/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Olaf Krzikalla 2004-2006.
// (C) Copyright Ion Gaztanaga  2006-2013.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////
#include <boost/container/vector.hpp>
#include <boost/intrusive/detail/config_begin.hpp>
#include "common_functors.hpp"
#include <boost/detail/lightweight_test.hpp>
#include <boost/intrusive/options.hpp>
#include <boost/intrusive/detail/iterator.hpp>
#include "test_macros.hpp"
#include "test_container.hpp"
#include "generic_assoc_test.hpp"
#include <typeinfo>

namespace boost{
namespace intrusive{
namespace test{

template<class ValueTraits, class ContainerDefiner>
struct test_generic_multiset
{
   typedef typename ValueTraits::value_type value_type;
   typedef typename ValueTraits::pointer pointer;
   typedef typename ValueTraits::const_pointer const_pointer;
   typedef typename ValueContainer< value_type >::type value_cont_type;
   typedef typename pointer_traits<pointer>::reference      reference;
   typedef typename pointer_traits
      <const_pointer>::reference                            const_reference;

   static void test_all();
   static void test_sort(value_cont_type&);
   static void test_insert(value_cont_type&);
   static void test_swap(value_cont_type&);
   static void test_find(value_cont_type&);
   static void test_impl();
};

template<class ValueTraits, class ContainerDefiner>
void test_generic_multiset<ValueTraits, ContainerDefiner>::test_all ()
{
   static const int random_init[6] = { 3, 2, 4, 1, 5, 2 };
   value_cont_type values (6);
   for (int i = 0; i < 6; ++i)
      (&values[i])->value_ = random_init[i];
   typedef typename ContainerDefiner::template container
      <>::type multiset_type;
   {
      multiset_type testset(values.begin(), values.end());
      test::test_container(testset);
      testset.clear();
      testset.insert(values.begin(), values.end());
      test::test_common_unordered_and_associative_container(testset, values);
      testset.clear();
      testset.insert(values.begin(), values.end());
      test::test_associative_container(testset, values);
      testset.clear();
      testset.insert(values.begin(), values.end());
      test::test_non_unique_container(testset, values);
   }
   test_sort(values);
   test_insert(values);
   test_swap(values);
   test_find(values);
   test_impl();
   test_generic_assoc<ValueTraits, ContainerDefiner>::test_all(values);
}

//test case due to an error in tree implementation:
template<class ValueTraits, class ContainerDefiner>
void test_generic_multiset<ValueTraits, ContainerDefiner>::test_impl()
{
   value_cont_type values (5);
   for (int i = 0; i < 5; ++i)
      (&values[i])->value_ = i;
   typedef typename ContainerDefiner::template container
      <>::type multiset_type;

   multiset_type testset;
   for (int i = 0; i < 5; ++i)
      testset.insert (values[i]);

   testset.erase (testset.iterator_to (values[0]));
   testset.erase (testset.iterator_to (values[1]));
   testset.insert (values[1]);

   testset.erase (testset.iterator_to (values[2]));
   testset.erase (testset.iterator_to (values[3]));
}

//test: constructor, iterator, clear, reverse_iterator, front, back, size:
template<class ValueTraits, class ContainerDefiner>
void test_generic_multiset<ValueTraits, ContainerDefiner>::test_sort(value_cont_type& values)
{
   typedef typename ContainerDefiner::template container
      <>::type multiset_type;

   multiset_type testset1 (values.begin(), values.end());
   {  int init_values [] = { 1, 2, 2, 3, 4, 5 };
      TEST_INTRUSIVE_SEQUENCE( init_values, testset1.begin() );  }

   testset1.clear();
   BOOST_TEST (testset1.empty());

   typedef typename ContainerDefiner::template container
      <compare<even_odd>
      >::type multiset_type2;

   multiset_type2 testset2 (values.begin(), values.begin() + 6);
   {  int init_values [] = { 5, 3, 1, 4, 2, 2 };
      TEST_INTRUSIVE_SEQUENCE( init_values, testset2.rbegin() );  }

   BOOST_TEST (testset2.begin()->value_ == 2);
   BOOST_TEST (testset2.rbegin()->value_ == 5);
}

//test: insert, const_iterator, const_reverse_iterator, erase, iterator_to:
template<class ValueTraits, class ContainerDefiner>
void test_generic_multiset<ValueTraits, ContainerDefiner>::test_insert(value_cont_type& values)
{
   typedef typename ContainerDefiner::template container
      <>::type multiset_type;

   multiset_type testset;
   testset.insert(values.begin() + 2, values.begin() + 5);
   {  int init_values [] = { 1, 4, 5 };
      TEST_INTRUSIVE_SEQUENCE( init_values, testset.begin() );  }

   typename multiset_type::iterator i = testset.begin();
   BOOST_TEST (i->value_ == 1);

   i = testset.insert (i, values[0]);
   BOOST_TEST (&*i == &values[0]);

   {  int init_values [] = { 5, 4, 3, 1 };
      TEST_INTRUSIVE_SEQUENCE( init_values, testset.rbegin() );  }

   i = testset.iterator_to (values[2]);
   BOOST_TEST (&*i == &values[2]);

   i = multiset_type::s_iterator_to (values[2]);
   BOOST_TEST (&*i == &values[2]);

   testset.erase(i);

   {  int init_values [] = { 1, 3, 5 };
      TEST_INTRUSIVE_SEQUENCE( init_values, testset.begin() );  }
}

//test: insert (seq-version), swap, erase (seq-version), size:
template<class ValueTraits, class ContainerDefiner>
void test_generic_multiset<ValueTraits, ContainerDefiner>::test_swap(value_cont_type& values)
{
   typedef typename ContainerDefiner::template container
      <>::type multiset_type;
   multiset_type testset1 (values.begin(), values.begin() + 2);
   multiset_type testset2;
   testset2.insert (values.begin() + 2, values.begin() + 6);
   testset1.swap (testset2);

   {  int init_values [] = { 1, 2, 4, 5 };
      TEST_INTRUSIVE_SEQUENCE( init_values, testset1.begin() );  }
   {  int init_values [] = { 2, 3 };
      TEST_INTRUSIVE_SEQUENCE( init_values, testset2.begin() );  }

   testset1.erase (testset1.iterator_to(values[5]), testset1.end());
   BOOST_TEST (testset1.size() == 1);
   BOOST_TEST (&*testset1.begin() == &values[3]);
}

//test: find, equal_range (lower_bound, upper_bound):
template<class ValueTraits, class ContainerDefiner>
void test_generic_multiset<ValueTraits, ContainerDefiner>::test_find(value_cont_type& values)
{
   typedef typename ContainerDefiner::template container
      <>::type multiset_type;
   typedef typename multiset_type::key_of_value key_of_value;
   multiset_type testset (values.begin(), values.end());
   typedef typename multiset_type::iterator        iterator;
   typedef typename multiset_type::const_iterator  const_iterator;

   {
      value_cont_type cmp_val_cont(1);
      reference cmp_val = cmp_val_cont.front();
      (&cmp_val)->value_ = 2;
      iterator i = testset.find (key_of_value()(cmp_val));
      BOOST_TEST (i->value_ == 2);
      BOOST_TEST ((++i)->value_ == 2);
      std::pair<iterator,iterator> range = testset.equal_range (key_of_value()(cmp_val));

      BOOST_TEST (range.first->value_ == 2);
      BOOST_TEST (range.second->value_ == 3);
      BOOST_TEST (boost::intrusive::iterator_distance (range.first, range.second) == 2);

      (&cmp_val)->value_ = 7;
      BOOST_TEST (testset.find(key_of_value()(cmp_val)) == testset.end());
   }
   {  //1, 2, 2, 3, 4, 5
      const multiset_type &const_testset = testset;
      std::pair<iterator,iterator> range;
      std::pair<const_iterator, const_iterator> const_range;
      value_cont_type cmp_val_cont(2);
      reference cmp_val_lower = cmp_val_cont.front();
      reference cmp_val_upper = cmp_val_cont.back();
      {
      (&cmp_val_lower)->value_ = 1;
      (&cmp_val_upper)->value_ = 2;
      //left-closed, right-closed
      range = testset.bounded_range (key_of_value()(cmp_val_lower), key_of_value()(cmp_val_upper), true, true);
      BOOST_TEST (range.first->value_ == 1);
      BOOST_TEST (range.second->value_ == 3);
      BOOST_TEST (boost::intrusive::iterator_distance (range.first, range.second) == 3);
      }
      {
      (&cmp_val_lower)->value_ = 1;
      (&cmp_val_upper)->value_ = 2;
      const_range = const_testset.bounded_range (key_of_value()(cmp_val_lower), key_of_value()(cmp_val_upper), true, false);
      BOOST_TEST (const_range.first->value_ == 1);
      BOOST_TEST (const_range.second->value_ == 2);
      BOOST_TEST (boost::intrusive::iterator_distance (const_range.first, const_range.second) == 1);

      (&cmp_val_lower)->value_ = 1;
      (&cmp_val_upper)->value_ = 3;
      range = testset.bounded_range (key_of_value()(cmp_val_lower), key_of_value()(cmp_val_upper), true, false);
      BOOST_TEST (range.first->value_ == 1);
      BOOST_TEST (range.second->value_ == 3);
      BOOST_TEST (boost::intrusive::iterator_distance (range.first, range.second) == 3);
      }
      {
      (&cmp_val_lower)->value_ = 1;
      (&cmp_val_upper)->value_ = 2;
      const_range = const_testset.bounded_range (key_of_value()(cmp_val_lower), key_of_value()(cmp_val_upper), false, true);
      BOOST_TEST (const_range.first->value_ == 2);
      BOOST_TEST (const_range.second->value_ == 3);
      BOOST_TEST (boost::intrusive::iterator_distance (const_range.first, const_range.second) == 2);
      }
      {
      (&cmp_val_lower)->value_ = 1;
      (&cmp_val_upper)->value_ = 2;
      range = testset.bounded_range (key_of_value()(cmp_val_lower), key_of_value()(cmp_val_upper), false, false);
      BOOST_TEST (range.first->value_ == 2);
      BOOST_TEST (range.second->value_ == 2);
      BOOST_TEST (boost::intrusive::iterator_distance (range.first, range.second) == 0);
      }
      {
      (&cmp_val_lower)->value_ = 5;
      (&cmp_val_upper)->value_ = 6;
      const_range = const_testset.bounded_range (key_of_value()(cmp_val_lower), key_of_value()(cmp_val_upper), true, false);
      BOOST_TEST (const_range.first->value_ == 5);
      BOOST_TEST (const_range.second == const_testset.end());
      BOOST_TEST (boost::intrusive::iterator_distance (const_range.first, const_range.second) == 1);
      }
   }
}

}}}   //namespace boost::intrusive::test

#include <boost/intrusive/detail/config_end.hpp>
