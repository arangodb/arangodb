//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

// the tests trigger deprecation warnings when compiled with msvc in C++17 mode
#if defined(_MSVC_LANG) && _MSVC_LANG > 201402
// warning STL4009: std::allocator<void> is deprecated in C++17
# define _SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING
#endif

#include <memory>
#include <iostream>

#include <boost/container/vector.hpp>
#include <boost/container/allocator.hpp>

#include <boost/move/utility_core.hpp>
#include "check_equal_containers.hpp"
#include "movable_int.hpp"
#include "expand_bwd_test_allocator.hpp"
#include "expand_bwd_test_template.hpp"
#include "dummy_test_allocator.hpp"
#include "propagate_allocator_test.hpp"
#include "vector_test.hpp"
#include "default_init_test.hpp"
#include "../../intrusive/test/iterator_test.hpp"

using namespace boost::container;

int test_expand_bwd()
{
   //Now test all back insertion possibilities

   //First raw ints
   typedef test::expand_bwd_test_allocator<int>
      int_allocator_type;
   typedef vector<int, int_allocator_type>
      int_vector;
   if(!test::test_all_expand_bwd<int_vector>())
      return 1;

   //Now user defined copyable int
   typedef test::expand_bwd_test_allocator<test::copyable_int>
      copyable_int_allocator_type;
   typedef vector<test::copyable_int, copyable_int_allocator_type>
      copyable_int_vector;
   if(!test::test_all_expand_bwd<copyable_int_vector>())
      return 1;

   return 0;
}

struct X;

template<typename T>
struct XRef
{
   explicit XRef(T* ptr)  : ptr(ptr) {}
   operator T*() const { return ptr; }
   T* ptr;
};

struct X
{
   XRef<X const> operator&() const { return XRef<X const>(this); }
   XRef<X>       operator&()       { return XRef<X>(this); }
};


bool test_smart_ref_type()
{
   boost::container::vector<X> x(5);
   return x.empty();
}

class recursive_vector
{
   public:
   recursive_vector & operator=(const recursive_vector &x)
   {  this->vector_ = x.vector_;   return *this; }

   int id_;
   vector<recursive_vector> vector_;
   vector<recursive_vector>::iterator it_;
   vector<recursive_vector>::const_iterator cit_;
   vector<recursive_vector>::reverse_iterator rit_;
   vector<recursive_vector>::const_reverse_iterator crit_;
};

void recursive_vector_test()//Test for recursive types
{
   vector<recursive_vector> recursive_vector_vector;
}

enum Test
{
   zero, one, two, three, four, five, six
};

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

template<class VoidAllocator>
int test_cont_variants()
{
   typedef typename GetAllocatorCont<VoidAllocator>::template apply<int>::type MyCont;
   typedef typename GetAllocatorCont<VoidAllocator>::template apply<test::movable_int>::type MyMoveCont;
   typedef typename GetAllocatorCont<VoidAllocator>::template apply<test::movable_and_copyable_int>::type MyCopyMoveCont;
   typedef typename GetAllocatorCont<VoidAllocator>::template apply<test::copyable_int>::type MyCopyCont;

   if(test::vector_test<MyCont>())
      return 1;
   if(test::vector_test<MyMoveCont>())
      return 1;
   if(test::vector_test<MyCopyMoveCont>())
      return 1;
   if(test::vector_test<MyCopyCont>())
      return 1;

   return 0;
}

struct boost_container_vector;

namespace boost { namespace container {   namespace test {

template<>
struct alloc_propagate_base<boost_container_vector>
{
   template <class T, class Allocator>
   struct apply
   {
      typedef boost::container::vector<T, Allocator> type;
   };
};

}}}   //namespace boost::container::test

int main()
{
   {
      const std::size_t positions_length = 10;
      std::size_t positions[positions_length];
      vector<int> vector_int;
      vector<int> vector_int2(positions_length);
      for(std::size_t i = 0; i != positions_length; ++i){
         positions[i] = 0u;
      }
      for(std::size_t i = 0, max = vector_int2.size(); i != max; ++i){
         vector_int2[i] = (int)i;
      }

      vector_int.insert(vector_int.begin(), 999);

      vector_int.insert_ordered_at(positions_length, positions + positions_length, vector_int2.end());

      for(std::size_t i = 0, max = vector_int.size(); i != max; ++i){
         std::cout << vector_int[i] << std::endl;
      }
   }
   recursive_vector_test();
   {
      //Now test move semantics
      vector<recursive_vector> original;
      vector<recursive_vector> move_ctor(boost::move(original));
      vector<recursive_vector> move_assign;
      move_assign = boost::move(move_ctor);
      move_assign.swap(original);
   }

   ////////////////////////////////////
   //    Testing allocator implementations
   ////////////////////////////////////
   //       std:allocator
   if(test_cont_variants< std::allocator<void> >()){
      std::cerr << "test_cont_variants< std::allocator<void> > failed" << std::endl;
      return 1;
   }
   //       boost::container::allocator
   if(test_cont_variants< allocator<void> >()){
      std::cerr << "test_cont_variants< allocator<void> > failed" << std::endl;
      return 1;
   }

   {
      typedef vector<Test, std::allocator<Test> > MyEnumCont;
      MyEnumCont v;
      Test t;
      v.push_back(t);
      v.push_back(::boost::move(t));
      v.push_back(Test());
   }

   if (test_smart_ref_type())
      return 1;

   ////////////////////////////////////
   //    Backwards expansion test
   ////////////////////////////////////
   if(test_expand_bwd())
      return 1;

   ////////////////////////////////////
   //    Default init test
   ////////////////////////////////////
   if(!test::default_init_test< vector<int, test::default_init_allocator<int> > >()){
      std::cerr << "Default init test failed" << std::endl;
      return 1;
   }

   ////////////////////////////////////
   //    Emplace testing
   ////////////////////////////////////
   const test::EmplaceOptions Options = (test::EmplaceOptions)(test::EMPLACE_BACK | test::EMPLACE_BEFORE);
   if(!boost::container::test::test_emplace< vector<test::EmplaceInt>, Options>()){
      return 1;
   }

   ////////////////////////////////////
   //    Allocator propagation testing
   ////////////////////////////////////
   if(!boost::container::test::test_propagate_allocator<boost_container_vector>()){
      return 1;
   }

   ////////////////////////////////////
   //    Initializer lists testing
   ////////////////////////////////////
   if(!boost::container::test::test_vector_methods_with_initializer_list_as_argument_for<
       boost::container::vector<int>
   >()) {
      return 1;
   }

   ////////////////////////////////////
   //    Iterator testing
   ////////////////////////////////////
   {
      typedef boost::container::vector<int> cont_int;
      cont_int a; a.push_back(0); a.push_back(1); a.push_back(2);
      boost::intrusive::test::test_iterator_random< cont_int >(a);
      if(boost::report_errors() != 0) {
         return 1;
      }
   }

#ifndef BOOST_CONTAINER_NO_CXX17_CTAD
   ////////////////////////////////////
   //    Constructor Template Auto Deduction testing
   ////////////////////////////////////
   {
      auto gold = std::vector{ 1, 2, 3 };
      auto test = boost::container::vector(gold.begin(), gold.end());
      if (test.size() != 3) {
         return 1;
      }
      if (!(test[0] == 1 && test[1] == 2 && test[2] == 3)) {
         return 1;
      }
   }
   {
      auto gold = std::vector{ 1, 2, 3 };
      auto test = boost::container::vector(gold.begin(), gold.end(), boost::container::new_allocator<int>());
      if (test.size() != 3) {
         return 1;
      }
      if (!(test[0] == 1 && test[1] == 2 && test[2] == 3)) {
         return 1;
      }
   }
#endif
   return 0;
}
