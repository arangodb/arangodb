//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/container/detail/config_begin.hpp>
#include <boost/container/slist.hpp>
#include <boost/container/node_allocator.hpp>

#include <memory>
#include "dummy_test_allocator.hpp"
#include "movable_int.hpp"
#include "list_test.hpp"
#include "propagate_allocator_test.hpp"
#include "emplace_test.hpp"
#include "../../intrusive/test/iterator_test.hpp"

using namespace boost::container;

class recursive_slist
{
public:
   int id_;
   slist<recursive_slist> slist_;
   slist<recursive_slist>::iterator it_;
   slist<recursive_slist>::const_iterator cit_;

   recursive_slist &operator=(const recursive_slist &o)
   { slist_ = o.slist_;  return *this; }
};

void recursive_slist_test()//Test for recursive types
{
   slist<recursive_slist> recursive_list_list;
}

template<class VoidAllocator>
struct GetAllocatorCont
{
   template<class ValueType>
   struct apply
   {
      typedef slist< ValueType
                   , typename allocator_traits<VoidAllocator>
                        ::template portable_rebind_alloc<ValueType>::type
                   > type;
   };
};

bool test_support_for_initializer_list()
{
#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   const std::initializer_list<int> il = {5, 10, 15};
   const slist<int> expected_list(il.begin(), il.end());
   {
      slist<int> sl = il;
      if(sl != expected_list)
         return false;
   }

   {
      slist<int> sl = {1, 2};
      sl = il;
      if(sl != expected_list)
         return false;
   }
   {
      slist<int> sl({ 1, 2 }, slist<int>::allocator_type());
      sl = il;
      if (sl != expected_list)
         return false;
   }
   {
      slist<int> sl = {4, 5};
      sl.assign(il);
      if(sl != expected_list)
         return false;
   }

   {
      slist<int> sl = {15};
      sl.insert(sl.cbegin(), {5, 10});
      if(sl != expected_list)
         return false;
   }

   {
       slist<int> sl = {5};
       sl.insert_after(sl.cbegin(), {10, 15});
       if(sl != expected_list)
          return false;
   }
   return true;
#endif
   return true;
}

bool test_for_splice()
{
   {
      slist<int> list1; list1.push_front(3); list1.push_front(2); list1.push_front(1); list1.push_front(0);
      slist<int> list2;
      slist<int> expected1; expected1.push_front(3); expected1.push_front(2);  expected1.push_front(0);
      slist<int> expected2; expected2.push_front(1);

      list2.splice(list2.begin(), list1, ++list1.begin());

      if (!(expected1 == list1 && expected2 == list2))
         return false;
   }
   {
      slist<int> list1; list1.push_front(3); list1.push_front(2); list1.push_front(1); list1.push_front(0);
      slist<int> list2;
      slist<int> expected1;
      slist<int> expected2; expected2.push_front(3); expected2.push_front(2); expected2.push_front(1); expected2.push_front(0);

      list2.splice(list2.begin(), list1, list1.begin(), list1.end());

      if (!(expected1 == list1 && expected2 == list2))
         return false;
   }
   return true;
}

struct boost_container_slist;

namespace boost {
namespace container {
namespace test {

template<>
struct alloc_propagate_base<boost_container_slist>
{
   template <class T, class Allocator>
   struct apply
   {
      typedef boost::container::slist<T, Allocator> type;
   };
};

}}}

int main ()
{
   recursive_slist_test();
   {
      //Now test move semantics
      slist<recursive_slist> original;
      slist<recursive_slist> move_ctor(boost::move(original));
      slist<recursive_slist> move_assign;
      move_assign = boost::move(move_ctor);
      move_assign.swap(original);
      {
         slist<recursive_slist> recursive, copy;
         //Test to test both move emulations
         if(!copy.size()){
            copy = recursive;
         }
      }
   }
   ////////////////////////////////////
   //    Testing allocator implementations
   ////////////////////////////////////
   if (test::list_test<slist<int, std::allocator<int> >, false>())
      return 1;
   if (test::list_test<slist<int>, false>())
      return 1;
   if (test::list_test<slist<int, node_allocator<int> >, false>())
      return 1;
   if (test::list_test<slist<test::movable_int>, false>())
      return 1;
   if (test::list_test<slist<test::movable_and_copyable_int>, false>())
      return 1;
   if (test::list_test<slist<test::copyable_int>, false>())
      return 1;

   ////////////////////////////////////
   //    Emplace testing
   ////////////////////////////////////
   const test::EmplaceOptions Options = (test::EmplaceOptions)
      (test::EMPLACE_FRONT | test::EMPLACE_AFTER | test::EMPLACE_BEFORE  | test::EMPLACE_AFTER);

   if(!boost::container::test::test_emplace
      < slist<test::EmplaceInt>, Options>())
      return 1;

   ////////////////////////////////////
   //    Allocator propagation testing
   ////////////////////////////////////
   if(!boost::container::test::test_propagate_allocator<boost_container_slist>())
      return 1;

   ////////////////////////////////////
   //    Initializer lists
   ////////////////////////////////////
   if(!test_support_for_initializer_list())
      return 1;

   ////////////////////////////////////
   //    Splice testing
   ////////////////////////////////////
   if(!test_for_splice())
      return 1;

   ////////////////////////////////////
   //    Iterator testing
   ////////////////////////////////////
   {
      typedef boost::container::slist<int> vector_int;
      vector_int a; a.push_front(2); a.push_front(1); a.push_front(0);
      boost::intrusive::test::test_iterator_forward< boost::container::slist<int> >(a);
      if(boost::report_errors() != 0) {
         return 1;
      }
   }
#ifndef BOOST_CONTAINER_NO_CXX17_CTAD
   ////////////////////////////////////
   //    Constructor Template Auto Deduction Tests
   ////////////////////////////////////
   {
      auto gold = std::list{ 1, 2, 3 };
      auto test = boost::container::slist(gold.begin(), gold.end());
      if (test.size() != 3) {
         return 1;
      }
      if (test.front() != 1)
         return 1;
      test.pop_front();
      if (test.front() != 2)
         return 1;
      test.pop_front();
      if (test.front() != 3)
         return 1;
      test.pop_front();
   }
   {
      auto gold = std::list{ 1, 2, 3 };
      auto test = boost::container::slist(gold.begin(), gold.end(), new_allocator<int>());
      if (test.size() != 3) {
         return 1;
      }
      if (test.front() != 1)
         return 1;
      test.pop_front();
      if (test.front() != 2)
         return 1;
      test.pop_front();
      if (test.front() != 3)
         return 1;
      test.pop_front();
   }
#endif

   return 0;
}

#include <boost/container/detail/config_end.hpp>

