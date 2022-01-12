//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/list.hpp>
#include <boost/container/adaptive_pool.hpp>

#include "dummy_test_allocator.hpp"
#include <memory>
#include "movable_int.hpp"
#include "list_test.hpp"
#include "propagate_allocator_test.hpp"
#include "emplace_test.hpp"
#include "../../intrusive/test/iterator_test.hpp"

using namespace boost::container;

namespace boost {
namespace container {

//Explicit instantiation to detect compilation errors
template class boost::container::list
   < test::movable_and_copyable_int
   , test::simple_allocator<test::movable_and_copyable_int> >;

template class boost::container::list
   < test::movable_and_copyable_int
   , adaptive_pool<test::movable_and_copyable_int> >;

namespace dtl {

template class iterator_from_iiterator
   <intrusive_list_type< std::allocator<int> >::container_type::iterator, true >;
template class iterator_from_iiterator
   <intrusive_list_type< std::allocator<int> >::container_type::iterator, false>;

}

}}

class recursive_list
{
public:
   int id_;
   list<recursive_list> list_;
   list<recursive_list>::iterator it_;
   list<recursive_list>::const_iterator cit_;
   list<recursive_list>::reverse_iterator rit_;
   list<recursive_list>::const_reverse_iterator crit_;

   recursive_list(const recursive_list &o)
      : list_(o.list_)
   {}

   recursive_list &operator=(const recursive_list &o)
   { list_ = o.list_;  return *this; }
};

void recursive_list_test()//Test for recursive types
{
   list<recursive_list> recursive, copy;
   //Test to test both move emulations
   if(!copy.size()){
      copy = recursive;
   }
}

template<class VoidAllocator>
struct GetAllocatorCont
{
   template<class ValueType>
   struct apply
   {
      typedef list< ValueType
                  , typename allocator_traits<VoidAllocator>
                        ::template portable_rebind_alloc<ValueType>::type
                  > type;
   };
};

template<class VoidAllocator>
int test_cont_variants()
{
   typedef typename GetAllocatorCont<VoidAllocator>::template apply<int>::type MyCont;
   typedef typename GetAllocatorCont<VoidAllocator>::template apply<test::movable_int>::type MyMoveCont;
   typedef typename GetAllocatorCont<VoidAllocator>::template apply<test::movable_and_copyable_int>::type MyCopyMoveCont;
   typedef typename GetAllocatorCont<VoidAllocator>::template apply<test::copyable_int>::type MyCopyCont;

   if(test::list_test<MyCont, true>())
      return 1;
   if(test::list_test<MyMoveCont, true>())
      return 1;
   if(test::list_test<MyCopyMoveCont, true>())
      return 1;
   if(test::list_test<MyCopyMoveCont, true>())
      return 1;
   if(test::list_test<MyCopyCont, true>())
      return 1;

   return 0;
}

bool test_support_for_initializer_list()
{
#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   const std::initializer_list<int> il = {1, 10};
   const list<int> expectedList(il.begin(), il.end());

   const list<int> testConstructor((il));
   if(testConstructor != expectedList)
      return false;

   const list<int> testConstructorAllocator(il, list<int>::allocator_type());
   if (testConstructorAllocator != expectedList)
      return false;

   list<int> testAssignOperator = {10, 11};
   testAssignOperator = il;
   if(testAssignOperator != expectedList)
      return false;

   list<int> testAssignMethod = {99};
   testAssignMethod = il;
   if(testAssignMethod != expectedList)
      return false;

   list<int> testInsertMethod;
   testInsertMethod.insert(testInsertMethod.cbegin(), il);
   if(testInsertMethod != testInsertMethod)
      return false;

   return true;
#endif
   return true;
}

struct boost_container_list;

namespace boost { namespace container {   namespace test {

template<>
struct alloc_propagate_base<boost_container_list>
{
   template <class T, class Allocator>
   struct apply
   {
      typedef boost::container::list<T, Allocator> type;
   };
};

}}}   //namespace boost::container::test

int main ()
{
   recursive_list_test();
   {
      //Now test move semantics
      list<recursive_list> original;
      list<recursive_list> move_ctor(boost::move(original));
      list<recursive_list> move_assign;
      move_assign = boost::move(move_ctor);
      move_assign.swap(original);
   }

   ////////////////////////////////////
   //    Testing allocator implementations
   ////////////////////////////////////
   //       int variants
   if (test::list_test<list<int, std::allocator<int> >, true>())
      return 1;
   if (test::list_test<list<int>, true>())
      return 1;
   if (test::list_test<list<int, adaptive_pool<int> >, true>())
      return 1;
   if (test::list_test<list<test::movable_int>, true>())
      return 1;
   if (test::list_test<list<test::movable_and_copyable_int>, true>())
      return 1;
   if (test::list_test<list<test::copyable_int>, true>())
      return 1;

   ////////////////////////////////////
   //    Emplace testing
   ////////////////////////////////////
   const test::EmplaceOptions Options = (test::EmplaceOptions)(test::EMPLACE_BACK | test::EMPLACE_FRONT | test::EMPLACE_BEFORE);

   if(!boost::container::test::test_emplace<list<test::EmplaceInt>, Options>())
      return 1;

   ////////////////////////////////////
   //    Allocator propagation testing
   ////////////////////////////////////
   if(!boost::container::test::test_propagate_allocator<boost_container_list>())
      return 1;

   ////////////////////////////////////
   //    Initializer lists
   ////////////////////////////////////
   if(!test_support_for_initializer_list())
      return 1;

   ////////////////////////////////////
   //    Iterator testing
   ////////////////////////////////////
   {
      typedef boost::container::list<int> cont_int;
      cont_int a; a.push_back(0); a.push_back(1); a.push_back(2);
      boost::intrusive::test::test_iterator_bidirectional< cont_int >(a);
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
      auto test = boost::container::list(gold.begin(), gold.end());
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
      auto test = boost::container::list(gold.begin(), gold.end(), new_allocator<int>());
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

   ////////////////////////////////////
   //    has_trivial_destructor_after_move testing
   ////////////////////////////////////
   // default allocator
   {
      typedef boost::container::list<int> cont;
      typedef cont::allocator_type allocator_type;
      typedef boost::container::allocator_traits<allocator_type>::pointer pointer;
      BOOST_STATIC_ASSERT_MSG(
        !(boost::has_trivial_destructor_after_move<cont>::value !=
          boost::has_trivial_destructor_after_move<allocator_type>::value &&
          boost::has_trivial_destructor_after_move<pointer>::value)
        , "has_trivial_destructor_after_move(default allocator) test failed");
   }
   // std::allocator
   {
      typedef boost::container::list<int, std::allocator<int> > cont;
      typedef cont::allocator_type allocator_type;
      typedef boost::container::allocator_traits<allocator_type>::pointer pointer;
      BOOST_STATIC_ASSERT_MSG(
        !(boost::has_trivial_destructor_after_move<cont>::value !=
          boost::has_trivial_destructor_after_move<allocator_type>::value &&
          boost::has_trivial_destructor_after_move<pointer>::value)
        , "has_trivial_destructor_after_move(std::allocator) test failed");
   }

   return 0;
}
