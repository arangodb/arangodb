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
#include <set>
#include <boost/container/set.hpp>
#include <boost/container/adaptive_pool.hpp>

#include "print_container.hpp"
#include "movable_int.hpp"
#include "dummy_test_allocator.hpp"
#include "set_test.hpp"
#include "propagate_allocator_test.hpp"
#include "emplace_test.hpp"
#include "../../intrusive/test/iterator_test.hpp"

using namespace boost::container;

namespace boost {
namespace container {

//Explicit instantiation to detect compilation errors

//set
template class set
   < test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , test::simple_allocator<test::movable_and_copyable_int>
   >;

template class set
   < test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , adaptive_pool<test::movable_and_copyable_int>
   >;

//multiset
template class multiset
   < test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , test::simple_allocator<test::movable_and_copyable_int>
   >;

template class multiset
   < test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , adaptive_pool<test::movable_and_copyable_int>
   >;

namespace container_detail {

//Instantiate base class as previous instantiations don't instantiate inherited members
template class tree
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , identity<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , test::simple_allocator<test::movable_and_copyable_int>
   , tree_assoc_defaults
   >;

template class tree
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , identity<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , std::allocator<test::movable_and_copyable_int>
   , tree_assoc_defaults
   >;

template class tree
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , identity<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , adaptive_pool<test::movable_and_copyable_int>
   , tree_assoc_defaults
   >;

}  //container_detail {

}} //boost::container

//Test recursive structures
class recursive_set
{
public:
   recursive_set & operator=(const recursive_set &x)
   {  id_ = x.id_;  set_ = x.set_; return *this; }

   int id_;
   set<recursive_set> set_;
   set<recursive_set>::iterator it_;
   set<recursive_set>::const_iterator cit_;
   set<recursive_set>::reverse_iterator rit_;
   set<recursive_set>::const_reverse_iterator crit_;

   friend bool operator< (const recursive_set &a, const recursive_set &b)
   {  return a.id_ < b.id_;   }
};

//Test recursive structures
class recursive_multiset
{
   public:
   recursive_multiset & operator=(const recursive_multiset &x)
   {  id_ = x.id_;  multiset_ = x.multiset_; return *this;  }

   int id_;
   multiset<recursive_multiset> multiset_;
   multiset<recursive_multiset>::iterator it_;
   multiset<recursive_multiset>::const_iterator cit_;
   multiset<recursive_multiset>::reverse_iterator rit_;
   multiset<recursive_multiset>::const_reverse_iterator crit_;

   friend bool operator< (const recursive_multiset &a, const recursive_multiset &b)
   {  return a.id_ < b.id_;   }
};

template<class C>
void test_move()
{
   //Now test move semantics
   C original;
   original.emplace();
   C move_ctor(boost::move(original));
   C move_assign;
   move_assign.emplace();
   move_assign = boost::move(move_ctor);
   move_assign.swap(original);
}

struct boost_container_set;
struct boost_container_multiset;

namespace boost {
namespace container {
namespace test {

template<>
struct alloc_propagate_base<boost_container_set>
{
   template <class T, class Allocator>
   struct apply
   {
      typedef boost::container::set<T, std::less<T>, Allocator> type;
   };
};

template<>
struct alloc_propagate_base<boost_container_multiset>
{
   template <class T, class Allocator>
   struct apply
   {
      typedef boost::container::multiset<T, std::less<T>, Allocator> type;
   };
};

}}}   //boost::container::test

template<class VoidAllocator, boost::container::tree_type_enum tree_type_value>
struct GetAllocatorSet
{
   template<class ValueType>
   struct apply
   {
      typedef set < ValueType
                  , std::less<ValueType>
                  , typename allocator_traits<VoidAllocator>
                     ::template portable_rebind_alloc<ValueType>::type
                  , typename boost::container::tree_assoc_options
                        < boost::container::tree_type<tree_type_value>
                        >::type
                  > set_type;

      typedef multiset < ValueType
                  , std::less<ValueType>
                  , typename allocator_traits<VoidAllocator>
                     ::template portable_rebind_alloc<ValueType>::type
                  , typename boost::container::tree_assoc_options
                        < boost::container::tree_type<tree_type_value>
                        >::type
                  > multiset_type;
   };
};

template<class VoidAllocator, boost::container::tree_type_enum tree_type_value>
int test_set_variants()
{
   typedef typename GetAllocatorSet<VoidAllocator, tree_type_value>::template apply<int>::set_type MySet;
   typedef typename GetAllocatorSet<VoidAllocator, tree_type_value>::template apply<test::movable_int>::set_type MyMoveSet;
   typedef typename GetAllocatorSet<VoidAllocator, tree_type_value>::template apply<test::movable_and_copyable_int>::set_type MyCopyMoveSet;
   typedef typename GetAllocatorSet<VoidAllocator, tree_type_value>::template apply<test::copyable_int>::set_type MyCopySet;

   typedef typename GetAllocatorSet<VoidAllocator, tree_type_value>::template apply<int>::multiset_type MyMultiSet;
   typedef typename GetAllocatorSet<VoidAllocator, tree_type_value>::template apply<test::movable_int>::multiset_type MyMoveMultiSet;
   typedef typename GetAllocatorSet<VoidAllocator, tree_type_value>::template apply<test::movable_and_copyable_int>::multiset_type MyCopyMoveMultiSet;
   typedef typename GetAllocatorSet<VoidAllocator, tree_type_value>::template apply<test::copyable_int>::multiset_type MyCopyMultiSet;

   typedef std::set<int>                                          MyStdSet;
   typedef std::multiset<int>                                     MyStdMultiSet;

   if (0 != test::set_test<
                  MySet
                  ,MyStdSet
                  ,MyMultiSet
                  ,MyStdMultiSet>()){
      std::cout << "Error in set_test<MyBoostSet>" << std::endl;
      return 1;
   }

   if (0 != test::set_test<
                  MyMoveSet
                  ,MyStdSet
                  ,MyMoveMultiSet
                  ,MyStdMultiSet>()){
      std::cout << "Error in set_test<MyBoostSet>" << std::endl;
      return 1;
   }

   if (0 != test::set_test<
                  MyCopyMoveSet
                  ,MyStdSet
                  ,MyCopyMoveMultiSet
                  ,MyStdMultiSet>()){
      std::cout << "Error in set_test<MyBoostSet>" << std::endl;
      return 1;
   }

   if (0 != test::set_test<
                  MyCopySet
                  ,MyStdSet
                  ,MyCopyMultiSet
                  ,MyStdMultiSet>()){
      std::cout << "Error in set_test<MyBoostSet>" << std::endl;
      return 1;
   }

   return 0;
}


int main ()
{
   //Recursive container instantiation
   {
      set<recursive_set> set_;
      multiset<recursive_multiset> multiset_;
   }
   //Allocator argument container
   {
      set<int> set_((set<int>::allocator_type()));
      multiset<int> multiset_((multiset<int>::allocator_type()));
   }
   //Now test move semantics
   {
      test_move<set<recursive_set> >();
      test_move<multiset<recursive_multiset> >();
   }

   ////////////////////////////////////
   //    Testing allocator implementations
   ////////////////////////////////////
   //       std:allocator
   if(test_set_variants< std::allocator<void>, red_black_tree >()){
      std::cerr << "test_set_variants< std::allocator<void> > failed" << std::endl;
      return 1;
   }
   //       boost::container::adaptive_pool
   if(test_set_variants< adaptive_pool<void>, red_black_tree>()){
      std::cerr << "test_set_variants< adaptive_pool<void> > failed" << std::endl;
      return 1;
   }

   ////////////////////////////////////
   //    Tree implementations
   ////////////////////////////////////
   //       AVL
   if(test_set_variants< std::allocator<void>, avl_tree >()){
      std::cerr << "test_set_variants< std::allocator<void>, avl_tree > failed" << std::endl;
      return 1;
   }
   //    SCAPEGOAT TREE
   if(test_set_variants< std::allocator<void>, scapegoat_tree >()){
      std::cerr << "test_set_variants< std::allocator<void>, scapegoat_tree > failed" << std::endl;
      return 1;
   }
   //    SPLAY TREE
   if(test_set_variants< std::allocator<void>, splay_tree >()){
      std::cerr << "test_set_variants< std::allocator<void>, splay_tree > failed" << std::endl;
      return 1;
   }

   ////////////////////////////////////
   //    Emplace testing
   ////////////////////////////////////
   const test::EmplaceOptions SetOptions = (test::EmplaceOptions)(test::EMPLACE_HINT | test::EMPLACE_ASSOC);
   if(!boost::container::test::test_emplace<set<test::EmplaceInt>, SetOptions>())
      return 1;
   if(!boost::container::test::test_emplace<multiset<test::EmplaceInt>, SetOptions>())
      return 1;

   ////////////////////////////////////
   //    Allocator propagation testing
   ////////////////////////////////////
   if(!boost::container::test::test_propagate_allocator<boost_container_set>())
      return 1;

   if(!boost::container::test::test_propagate_allocator<boost_container_multiset>())
      return 1;

   if (!boost::container::test::test_set_methods_with_initializer_list_as_argument_for<set<int> >())
      return 1;

   if (!boost::container::test::test_set_methods_with_initializer_list_as_argument_for<multiset<int> >())
      return 1;

   ////////////////////////////////////
   //    Test optimize_size option
   ////////////////////////////////////
   //
   // set
   //
   typedef set< int*, std::less<int*>, std::allocator<int*>
              , tree_assoc_options< optimize_size<false>, tree_type<red_black_tree> >::type > rbset_size_optimized_no;
   typedef set< int*, std::less<int*>, std::allocator<int*>
              , tree_assoc_options< optimize_size<true>, tree_type<red_black_tree>  >::type > rbset_size_optimized_yes;
   BOOST_STATIC_ASSERT(sizeof(rbset_size_optimized_yes) < sizeof(rbset_size_optimized_no));

   typedef set< int*, std::less<int*>, std::allocator<int*>
              , tree_assoc_options< optimize_size<false>, tree_type<avl_tree> >::type > avlset_size_optimized_no;
   typedef set< int*, std::less<int*>, std::allocator<int*>
              , tree_assoc_options< optimize_size<true>, tree_type<avl_tree>  >::type > avlset_size_optimized_yes;
   BOOST_STATIC_ASSERT(sizeof(avlset_size_optimized_yes) < sizeof(avlset_size_optimized_no));
   //
   // multiset
   //
   typedef multiset< int*, std::less<int*>, std::allocator<int*>
                   , tree_assoc_options< optimize_size<false>, tree_type<red_black_tree> >::type > rbmset_size_optimized_no;
   typedef multiset< int*, std::less<int*>, std::allocator<int*>
                   , tree_assoc_options< optimize_size<true>, tree_type<red_black_tree>  >::type > rbmset_size_optimized_yes;
   BOOST_STATIC_ASSERT(sizeof(rbmset_size_optimized_yes) < sizeof(rbmset_size_optimized_no));

   typedef multiset< int*, std::less<int*>, std::allocator<int*>
                   , tree_assoc_options< optimize_size<false>, tree_type<avl_tree> >::type > avlmset_size_optimized_no;
   typedef multiset< int*, std::less<int*>, std::allocator<int*>
                   , tree_assoc_options< optimize_size<true>, tree_type<avl_tree>  >::type > avlmset_size_optimized_yes;
   BOOST_STATIC_ASSERT(sizeof(avlmset_size_optimized_yes) < sizeof(avlmset_size_optimized_no));

   ////////////////////////////////////
   //    Iterator testing
   ////////////////////////////////////
   {
      typedef boost::container::set<int> cont_int;
      cont_int a; a.insert(0); a.insert(1); a.insert(2);
      boost::intrusive::test::test_iterator_bidirectional< cont_int >(a);
      if(boost::report_errors() != 0) {
         return 1;
      }
   }
   {
      typedef boost::container::multiset<int> cont_int;
      cont_int a; a.insert(0); a.insert(1); a.insert(2);
      boost::intrusive::test::test_iterator_bidirectional< cont_int >(a);
      if(boost::report_errors() != 0) {
         return 1;
      }
   }
   return 0;
}

#include <boost/container/detail/config_end.hpp>
