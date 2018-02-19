//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2008. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_CONTAINER_PROPAGATE_ALLOCATOR_TEST_HPP
#define BOOST_CONTAINER_PROPAGATE_ALLOCATOR_TEST_HPP

#include <boost/container/detail/config_begin.hpp>
#include <boost/core/lightweight_test.hpp>
#include "dummy_test_allocator.hpp"

#include <iostream>

namespace boost{
namespace container {
namespace test{

template<class Selector>
struct alloc_propagate_base;

template<class T, class Allocator, class Selector>
class alloc_propagate_wrapper
   : public alloc_propagate_base<Selector>::template apply<T, Allocator>::type
{
   BOOST_COPYABLE_AND_MOVABLE(alloc_propagate_wrapper)

   public:   
   typedef typename alloc_propagate_base
      <Selector>::template apply<T, Allocator>::type  Base;

   typedef typename Base::allocator_type  allocator_type;
   typedef typename Base::value_type      value_type;
   typedef typename Base::size_type       size_type;

   alloc_propagate_wrapper()
      : Base()
   {}

   explicit alloc_propagate_wrapper(const allocator_type &a)
      : Base(a)
   {}
/*
   //sequence containers only
   explicit alloc_propagate_wrapper(size_type n, const value_type &v, const allocator_type &a)
      : Base(n, v, a)
   {}

   alloc_propagate_wrapper(size_type n, const allocator_type &a)
      : Base(n, a)
   {}*/

   template<class Iterator>
   alloc_propagate_wrapper(Iterator b, Iterator e, const allocator_type &a)
      : Base(b, e, a)
   {}

   #if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   alloc_propagate_wrapper(std::initializer_list<value_type> il, const allocator_type& a)
      : Base(il, a)
   {}
/*
   //associative containers only
   alloc_propagate_wrapper(std::initializer_list<value_type> il, const Compare& comp, const allocator_type& a)
      : Base(il, comp, a)
   {}*/

   #endif

   alloc_propagate_wrapper(const alloc_propagate_wrapper &x)
      : Base(x)
   {}

   alloc_propagate_wrapper(const alloc_propagate_wrapper &x, const allocator_type &a)
      : Base(x, a)
   {}

   alloc_propagate_wrapper(BOOST_RV_REF(alloc_propagate_wrapper) x)
      : Base(boost::move(static_cast<Base&>(x)))
   {}

   alloc_propagate_wrapper(BOOST_RV_REF(alloc_propagate_wrapper) x, const allocator_type &a)
      : Base(boost::move(static_cast<Base&>(x)), a)
   {}

   alloc_propagate_wrapper &operator=(BOOST_COPY_ASSIGN_REF(alloc_propagate_wrapper) x)
   {  this->Base::operator=((const Base &)x);  return *this; }

   alloc_propagate_wrapper &operator=(BOOST_RV_REF(alloc_propagate_wrapper) x)
   {  this->Base::operator=(boost::move(static_cast<Base&>(x)));  return *this; }

   void swap(alloc_propagate_wrapper &x)
   {  this->Base::swap(x);  }
};

template<class T>
struct get_real_stored_allocator
{
   typedef typename T::stored_allocator_type type;
};

template<class Container>
void test_propagate_allocator_allocator_arg();

template<class Selector>
bool test_propagate_allocator()
{
   {
      typedef propagation_test_allocator<char, true, true, true, true>  AlwaysPropagate;
      typedef alloc_propagate_wrapper<char, AlwaysPropagate, Selector>  PropagateCont;
      typedef typename get_real_stored_allocator<typename PropagateCont::Base>::type StoredAllocator;
      {
         //////////////////////////////////////////
         //Test AlwaysPropagate allocator propagation
         //////////////////////////////////////////
         
         //default constructor
         StoredAllocator::reset_unique_id(111);
         PropagateCont c;  //stored 112
         BOOST_TEST (c.get_stored_allocator().id_ == 112);
         BOOST_TEST (c.get_stored_allocator().ctr_copies_ == 0);
         BOOST_TEST (c.get_stored_allocator().ctr_moves_ == 0);
         BOOST_TEST (c.get_stored_allocator().assign_copies_ == 0);
         BOOST_TEST (c.get_stored_allocator().assign_moves_ == 0);
         BOOST_TEST (c.get_stored_allocator().swaps_ == 0);
      }
      {
         //copy constructor
         StoredAllocator::reset_unique_id(222);
         PropagateCont c; //stored 223
         BOOST_TEST (c.get_stored_allocator().id_ == 223);
         //propagate_on_copy_constructor produces copies, moves or RVO (depending on the compiler).
         //For allocators that copy in select_on_container_copy_construction, at least we must have a copy
         PropagateCont c2(c); //should propagate 223
         BOOST_TEST (c2.get_stored_allocator().id_ == 223);
         BOOST_TEST (c2.get_stored_allocator().ctr_copies_ >= 1);
         BOOST_TEST (c2.get_stored_allocator().ctr_moves_ >= 0);
         BOOST_TEST (c2.get_stored_allocator().assign_copies_ == 0);
         BOOST_TEST (c2.get_stored_allocator().assign_moves_ == 0);
         BOOST_TEST (c2.get_stored_allocator().swaps_ == 0);
      }
      {
         //move constructor
         StoredAllocator::reset_unique_id(333);
         PropagateCont c; //stored 334
         BOOST_TEST (c.get_stored_allocator().id_ == 334);
         PropagateCont c2(boost::move(c)); //should propagate 334
         BOOST_TEST (c2.get_stored_allocator().id_ == 334);
         BOOST_TEST (c2.get_stored_allocator().ctr_copies_ == 0);
         BOOST_TEST (c2.get_stored_allocator().ctr_moves_ > 0);
         BOOST_TEST (c2.get_stored_allocator().assign_copies_ == 0);
         BOOST_TEST (c2.get_stored_allocator().assign_moves_ == 0);
         BOOST_TEST (c2.get_stored_allocator().swaps_ == 0);
      }
      {
         //copy assign
         StoredAllocator::reset_unique_id(444);
         PropagateCont c; //stored 445
         BOOST_TEST (c.get_stored_allocator().id_ == 445);
         PropagateCont c2; //stored 446
         BOOST_TEST (c2.get_stored_allocator().id_ == 446);
         c2 = c; //should propagate 445
         BOOST_TEST (c2.get_stored_allocator().id_ == 445);
         BOOST_TEST (c2.get_stored_allocator().ctr_copies_ == 0);
         BOOST_TEST (c2.get_stored_allocator().ctr_moves_  == 0);
         BOOST_TEST (c2.get_stored_allocator().assign_copies_ == 1);
         BOOST_TEST (c2.get_stored_allocator().assign_moves_ == 0);
         BOOST_TEST (c2.get_stored_allocator().swaps_ == 0);
      }
      {
         //move assign
         StoredAllocator::reset_unique_id(555);
         PropagateCont c; //stored 556
         BOOST_TEST (c.get_stored_allocator().id_ == 556);
         PropagateCont c2; //stored 557
         BOOST_TEST (c2.get_stored_allocator().id_ == 557);
         c = boost::move(c2); //should propagate 557
         BOOST_TEST (c.get_stored_allocator().id_ == 557);
         BOOST_TEST (c.get_stored_allocator().ctr_copies_    == 0);
         BOOST_TEST (c.get_stored_allocator().ctr_moves_     == 0);
         BOOST_TEST (c.get_stored_allocator().assign_copies_ == 0);
         BOOST_TEST (c.get_stored_allocator().assign_moves_ == 1);
         BOOST_TEST (c.get_stored_allocator().swaps_ == 0);
      }
      {
         //swap
         StoredAllocator::reset_unique_id(666);
         PropagateCont c; //stored 667
         BOOST_TEST (c.get_stored_allocator().id_ == 667);
         PropagateCont c2; //stored 668
         BOOST_TEST (c2.get_stored_allocator().id_ == 668);
         c.swap(c2);
         BOOST_TEST (c2.get_stored_allocator().ctr_copies_    == 0);
         BOOST_TEST (c.get_stored_allocator().ctr_copies_    == 0);
         BOOST_TEST (c2.get_stored_allocator().ctr_moves_     == 0);
         BOOST_TEST (c.get_stored_allocator().ctr_moves_     == 0);
         BOOST_TEST (c2.get_stored_allocator().assign_copies_ == 0);
         BOOST_TEST (c.get_stored_allocator().assign_copies_ == 0);
         BOOST_TEST (c2.get_stored_allocator().assign_moves_  == 0);
         BOOST_TEST (c.get_stored_allocator().assign_moves_  == 0);
         BOOST_TEST (c2.get_stored_allocator().swaps_  == 1);
         BOOST_TEST (c.get_stored_allocator().swaps_  == 1);
      }
      //And now allocator argument constructors
      test_propagate_allocator_allocator_arg<PropagateCont>();
   }

   //////////////////////////////////////////
   //Test NeverPropagate allocator propagation
   //////////////////////////////////////////
   {
      typedef propagation_test_allocator<char, false, false, false, false> NeverPropagate;
      typedef alloc_propagate_wrapper<char, NeverPropagate, Selector>      NoPropagateCont;
      typedef typename get_real_stored_allocator<typename NoPropagateCont::Base>::type StoredAllocator;
      {
         //default constructor
         StoredAllocator::reset_unique_id(111);
         NoPropagateCont c; //stored 112
         BOOST_TEST (c.get_stored_allocator().id_ == 112);
         BOOST_TEST (c.get_stored_allocator().ctr_copies_ == 0);
         BOOST_TEST (c.get_stored_allocator().ctr_moves_ == 0);
         BOOST_TEST (c.get_stored_allocator().assign_copies_ == 0);
         BOOST_TEST (c.get_stored_allocator().assign_moves_ == 0);
         BOOST_TEST (c.get_stored_allocator().swaps_ == 0);
      }
      {
         //copy constructor
         //propagate_on_copy_constructor produces copies, moves or RVO (depending on the compiler)
         //For allocators that don't copy in select_on_container_copy_construction we must have a default
         //construction
         StoredAllocator::reset_unique_id(222);
         NoPropagateCont c; //stored 223
         BOOST_TEST (c.get_stored_allocator().id_ == 223);
         NoPropagateCont c2(c); //should NOT propagate 223
         BOOST_TEST (c2.get_stored_allocator().id_ == 224);
         BOOST_TEST (c2.get_stored_allocator().ctr_copies_ >= 0);
         BOOST_TEST (c2.get_stored_allocator().ctr_moves_ >= 0);
         BOOST_TEST (c2.get_stored_allocator().assign_copies_ == 0);
         BOOST_TEST (c2.get_stored_allocator().assign_moves_ == 0);
         BOOST_TEST (c2.get_stored_allocator().swaps_ == 0);
      }
      {
         //move constructor
         StoredAllocator::reset_unique_id(333);
         NoPropagateCont c; //stored 334
         BOOST_TEST (c.get_stored_allocator().id_ == 334);
         NoPropagateCont c2(boost::move(c)); // should NOT propagate 334
         BOOST_TEST (c2.get_stored_allocator().ctr_copies_ >= 0);
         BOOST_TEST (c2.get_stored_allocator().ctr_moves_ >= 0);
         BOOST_TEST (c2.get_stored_allocator().assign_copies_ == 0);
         BOOST_TEST (c2.get_stored_allocator().assign_moves_ == 0);
         BOOST_TEST (c2.get_stored_allocator().swaps_ == 0);
      }
      {
         //copy assign
         StoredAllocator::reset_unique_id(444);
         NoPropagateCont c; //stored 445
         NoPropagateCont c2; //stored 446
         c2 = c; // should NOT propagate 445
         BOOST_TEST (c2.get_stored_allocator().id_ == 446);
         BOOST_TEST (c2.get_stored_allocator().ctr_copies_ == 0);
         BOOST_TEST (c2.get_stored_allocator().ctr_moves_  == 0);
         BOOST_TEST (c2.get_stored_allocator().assign_copies_ == 0);
         BOOST_TEST (c2.get_stored_allocator().assign_moves_ == 0);
         BOOST_TEST (c2.get_stored_allocator().swaps_ == 0);
      }
      {
         //move assign
         StoredAllocator::reset_unique_id(555);
         NoPropagateCont c; //stored 556
         NoPropagateCont c2; //stored 557
         c2 = c; // should NOT propagate 556
         BOOST_TEST (c2.get_stored_allocator().id_ == 557);
         BOOST_TEST (c2.get_stored_allocator().ctr_copies_ == 0);
         BOOST_TEST (c2.get_stored_allocator().ctr_moves_  == 0);
         BOOST_TEST (c2.get_stored_allocator().assign_copies_ == 0);
         BOOST_TEST (c2.get_stored_allocator().assign_moves_ == 0);
         BOOST_TEST (c2.get_stored_allocator().swaps_ == 0);
      }
      {
         //swap
         StoredAllocator::reset_unique_id(666);
         NoPropagateCont c; //stored 667
         BOOST_TEST (c.get_stored_allocator().id_ == 667);
         NoPropagateCont c2; //stored 668
         BOOST_TEST (c2.get_stored_allocator().id_ == 668);
         c2.swap(c); // should NOT swap 667 and 668
         BOOST_TEST (c2.get_stored_allocator().id_ == 668);
         BOOST_TEST (c2.get_stored_allocator().ctr_copies_ == 0);
         BOOST_TEST (c2.get_stored_allocator().ctr_moves_  == 0);
         BOOST_TEST (c2.get_stored_allocator().assign_copies_ == 0);
         BOOST_TEST (c2.get_stored_allocator().assign_moves_ == 0);
         BOOST_TEST (c2.get_stored_allocator().swaps_ == 0);
         BOOST_TEST (c.get_stored_allocator().id_ == 667);
         BOOST_TEST (c.get_stored_allocator().ctr_copies_ == 0);
         BOOST_TEST (c.get_stored_allocator().ctr_moves_  == 0);
         BOOST_TEST (c.get_stored_allocator().assign_copies_ == 0);
         BOOST_TEST (c.get_stored_allocator().assign_moves_ == 0);
         BOOST_TEST (c.get_stored_allocator().swaps_ == 0);
      }
      //And now allocator argument constructors
      test_propagate_allocator_allocator_arg<NoPropagateCont>();
   }

   return report_errors() == 0;
}

template<class Container>
void test_propagate_allocator_allocator_arg()
{
   typedef typename Container::allocator_type         allocator_type;
   typedef typename get_real_stored_allocator<typename Container::Base>::type StoredAllocator;

   {  //The allocator must be always propagated
      //allocator constructor
      allocator_type::reset_unique_id(111);
      const allocator_type & a = allocator_type(); //stored 112
      Container c(a); //should propagate 112
      BOOST_TEST (c.get_stored_allocator().id_ == 112);
      BOOST_TEST (c.get_stored_allocator().ctr_copies_ > 0);
      BOOST_TEST (c.get_stored_allocator().ctr_moves_ == 0);
      BOOST_TEST (c.get_stored_allocator().assign_copies_ == 0);
      BOOST_TEST (c.get_stored_allocator().assign_moves_ == 0);
      BOOST_TEST (c.get_stored_allocator().swaps_ == 0);
   }
   {
      //copy allocator constructor
      StoredAllocator::reset_unique_id(999);
      Container c;
      //stored_allocator_type could be the same type as allocator_type
      //so reset it again to get a predictable result
      allocator_type::reset_unique_id(222);
      Container c2(c, allocator_type()); //should propagate 223
      BOOST_TEST (c2.get_stored_allocator().id_ == 223);
      BOOST_TEST (c2.get_stored_allocator().ctr_copies_ > 0);
      BOOST_TEST (c2.get_stored_allocator().ctr_moves_ == 0);
      BOOST_TEST (c2.get_stored_allocator().assign_copies_ == 0);
      BOOST_TEST (c2.get_stored_allocator().assign_moves_ == 0);
      BOOST_TEST (c2.get_stored_allocator().swaps_ == 0);
   }
   {
      //move allocator constructor
      StoredAllocator::reset_unique_id(999);
      Container c;
      //stored_allocator_type could be the same type as allocator_type
      //so reset it again to get a predictable result
      allocator_type::reset_unique_id(333);
      Container c2(boost::move(c), allocator_type()); //should propagate 334
      BOOST_TEST (c2.get_stored_allocator().id_ == 334);
      BOOST_TEST (c2.get_stored_allocator().ctr_copies_ > 0);
      BOOST_TEST (c2.get_stored_allocator().ctr_moves_ == 0);
      BOOST_TEST (c2.get_stored_allocator().assign_copies_ == 0);
      BOOST_TEST (c2.get_stored_allocator().assign_moves_ == 0);
      BOOST_TEST (c2.get_stored_allocator().swaps_ == 0);
   }
}

}  //namespace test{
}  //namespace container {
}  //namespace boost{

#include <boost/container/detail/config_end.hpp>

#endif   //#ifndef BOOST_CONTAINER_PROPAGATE_ALLOCATOR_TEST_HPP
