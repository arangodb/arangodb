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

namespace boost {
namespace container {

//Explicit instantiation to detect compilation errors
template class boost::container::slist
   < test::movable_and_copyable_int
   , test::simple_allocator<test::movable_and_copyable_int> >;

template class boost::container::slist
   < test::movable_and_copyable_int
   , node_allocator<test::movable_and_copyable_int> >;

}}

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

template<class VoidAllocator>
int test_cont_variants()
{
   typedef typename GetAllocatorCont<VoidAllocator>::template apply<int>::type MyCont;
   typedef typename GetAllocatorCont<VoidAllocator>::template apply<test::movable_int>::type MyMoveCont;
   typedef typename GetAllocatorCont<VoidAllocator>::template apply<test::movable_and_copyable_int>::type MyCopyMoveCont;
   typedef typename GetAllocatorCont<VoidAllocator>::template apply<test::copyable_int>::type MyCopyCont;

   if(test::list_test<MyCont, false>())
      return 1;
   if(test::list_test<MyMoveCont, false>())
      return 1;
   if(test::list_test<MyCopyMoveCont, false>())
      return 1;
   if(test::list_test<MyCopyMoveCont, false>())
      return 1;
   if(test::list_test<MyCopyCont, false>())
      return 1;

   return 0;
}

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
   //       std:allocator
   if(test_cont_variants< std::allocator<void> >()){
      std::cerr << "test_cont_variants< std::allocator<void> > failed" << std::endl;
      return 1;
   }
   //       boost::container::node_allocator
   if(test_cont_variants< node_allocator<void> >()){
      std::cerr << "test_cont_variants< node_allocator<void> > failed" << std::endl;
      return 1;
   }

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
}

#include <boost/container/detail/config_end.hpp>

