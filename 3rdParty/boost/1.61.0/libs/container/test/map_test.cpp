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
#include <boost/container/map.hpp>
#include <boost/container/adaptive_pool.hpp>

#include <map>

#include "print_container.hpp"
#include "movable_int.hpp"
#include "dummy_test_allocator.hpp"
#include "map_test.hpp"
#include "propagate_allocator_test.hpp"
#include "emplace_test.hpp"
#include "../../intrusive/test/iterator_test.hpp"

using namespace boost::container;

namespace boost {
namespace container {

//Explicit instantiation to detect compilation errors

//map
template class map
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , test::simple_allocator
      < std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int> >
   >;

template class map
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , std::allocator
      < std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int> >
   >;

template class map
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , adaptive_pool
      < std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int> >
   >;

//multimap
template class multimap
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , test::simple_allocator
      < std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int> >
   >;

template class multimap
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , std::allocator
      < std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int> >
   >;

template class multimap
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , adaptive_pool
      < std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int> >
   >;

namespace container_detail {

template class tree
   < const test::movable_and_copyable_int
   , std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int>
   , container_detail::select1st< std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int> >
   , std::less<test::movable_and_copyable_int>
   , test::simple_allocator
      < std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int> >
   , tree_assoc_defaults >;

template class tree
   < const test::movable_and_copyable_int
   , std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int>
   , container_detail::select1st< std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int> >
   , std::less<test::movable_and_copyable_int>
   , std::allocator
      < std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int> >
   , tree_assoc_defaults >;

template class tree
   < const test::movable_and_copyable_int
   , std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int>
   , container_detail::select1st< std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int> >
   , std::less<test::movable_and_copyable_int>
   , adaptive_pool
      < std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int> >
   , tree_assoc_defaults >;

}  //container_detail {

}} //boost::container

class recursive_map
{
   public:
   recursive_map & operator=(const recursive_map &x)
   {  id_ = x.id_;  map_ = x.map_; return *this;  }

   int id_;
   map<recursive_map, recursive_map> map_;
   map<recursive_map, recursive_map>::iterator it_;
   map<recursive_map, recursive_map>::const_iterator cit_;
   map<recursive_map, recursive_map>::reverse_iterator rit_;
   map<recursive_map, recursive_map>::const_reverse_iterator crit_;

   friend bool operator< (const recursive_map &a, const recursive_map &b)
   {  return a.id_ < b.id_;   }
};

class recursive_multimap
{
   public:
   recursive_multimap & operator=(const recursive_multimap &x)
   {  id_ = x.id_;  multimap_ = x.multimap_; return *this;  }

   int id_;
   multimap<recursive_multimap, recursive_multimap> multimap_;
   multimap<recursive_multimap, recursive_multimap>::iterator it_;
   multimap<recursive_multimap, recursive_multimap>::const_iterator cit_;
   multimap<recursive_multimap, recursive_multimap>::reverse_iterator rit_;
   multimap<recursive_multimap, recursive_multimap>::const_reverse_iterator crit_;

   friend bool operator< (const recursive_multimap &a, const recursive_multimap &b)
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

template<class VoidAllocator, boost::container::tree_type_enum tree_type_value>
struct GetAllocatorMap
{
   template<class ValueType>
   struct apply
   {
      typedef map< ValueType
                 , ValueType
                 , std::less<ValueType>
                 , typename allocator_traits<VoidAllocator>
                    ::template portable_rebind_alloc< std::pair<const ValueType, ValueType> >::type
                  , typename boost::container::tree_assoc_options
                        < boost::container::tree_type<tree_type_value>
                        >::type
                 > map_type;

      typedef multimap< ValueType
                 , ValueType
                 , std::less<ValueType>
                 , typename allocator_traits<VoidAllocator>
                    ::template portable_rebind_alloc< std::pair<const ValueType, ValueType> >::type
                  , typename boost::container::tree_assoc_options
                        < boost::container::tree_type<tree_type_value>
                        >::type
                 > multimap_type;
   };
};

template<class VoidAllocator, boost::container::tree_type_enum tree_type_value>
int test_map_variants()
{
   typedef typename GetAllocatorMap<VoidAllocator, tree_type_value>::template apply<int>::map_type MyMap;
   typedef typename GetAllocatorMap<VoidAllocator, tree_type_value>::template apply<test::movable_int>::map_type MyMoveMap;
   typedef typename GetAllocatorMap<VoidAllocator, tree_type_value>::template apply<test::movable_and_copyable_int>::map_type MyCopyMoveMap;
   typedef typename GetAllocatorMap<VoidAllocator, tree_type_value>::template apply<test::copyable_int>::map_type MyCopyMap;

   typedef typename GetAllocatorMap<VoidAllocator, tree_type_value>::template apply<int>::multimap_type MyMultiMap;
   typedef typename GetAllocatorMap<VoidAllocator, tree_type_value>::template apply<test::movable_int>::multimap_type MyMoveMultiMap;
   typedef typename GetAllocatorMap<VoidAllocator, tree_type_value>::template apply<test::movable_and_copyable_int>::multimap_type MyCopyMoveMultiMap;
   typedef typename GetAllocatorMap<VoidAllocator, tree_type_value>::template apply<test::copyable_int>::multimap_type MyCopyMultiMap;

   typedef std::map<int, int>                                     MyStdMap;
   typedef std::multimap<int, int>                                MyStdMultiMap;

   if (0 != test::map_test<
                  MyMap
                  ,MyStdMap
                  ,MyMultiMap
                  ,MyStdMultiMap>()){
      std::cout << "Error in map_test<MyBoostMap>" << std::endl;
      return 1;
   }

   if (0 != test::map_test<
                  MyMoveMap
                  ,MyStdMap
                  ,MyMoveMultiMap
                  ,MyStdMultiMap>()){
      std::cout << "Error in map_test<MyBoostMap>" << std::endl;
      return 1;
   }

   if (0 != test::map_test<
                  MyCopyMoveMap
                  ,MyStdMap
                  ,MyCopyMoveMultiMap
                  ,MyStdMultiMap>()){
      std::cout << "Error in map_test<MyBoostMap>" << std::endl;
      return 1;
   }

   if (0 != test::map_test<
                  MyCopyMap
                  ,MyStdMap
                  ,MyCopyMultiMap
                  ,MyStdMultiMap>()){
      std::cout << "Error in map_test<MyBoostMap>" << std::endl;
      return 1;
   }

   return 0;
}

struct boost_container_map;
struct boost_container_multimap;

namespace boost { namespace container {   namespace test {

template<>
struct alloc_propagate_base<boost_container_map>
{
   template <class T, class Allocator>
   struct apply
   {
      typedef typename boost::container::allocator_traits<Allocator>::
         template portable_rebind_alloc<std::pair<const T, T> >::type TypeAllocator;
      typedef boost::container::map<T, T, std::less<T>, TypeAllocator> type;
   };
};

template<>
struct alloc_propagate_base<boost_container_multimap>
{
   template <class T, class Allocator>
   struct apply
   {
      typedef typename boost::container::allocator_traits<Allocator>::
         template portable_rebind_alloc<std::pair<const T, T> >::type TypeAllocator;
      typedef boost::container::multimap<T, T, std::less<T>, TypeAllocator> type;
   };
};

}}}   //namespace boost::container::test

int main ()
{
   //Recursive container instantiation
   {
      map<recursive_map, recursive_map> map_;
      multimap<recursive_multimap, recursive_multimap> multimap_;
   }
   //Allocator argument container
   {
      map<int, int> map_((map<int, int>::allocator_type()));
      multimap<int, int> multimap_((multimap<int, int>::allocator_type()));
   }
   //Now test move semantics
   {
      test_move<map<recursive_map, recursive_map> >();
      test_move<multimap<recursive_multimap, recursive_multimap> >();
   }

   ////////////////////////////////////
   //    Testing allocator implementations
   ////////////////////////////////////
   //       std:allocator
   if(test_map_variants< std::allocator<void>, red_black_tree >()){
      std::cerr << "test_map_variants< std::allocator<void> > failed" << std::endl;
      return 1;
   }
   //       boost::container::adaptive_pool
   if(test_map_variants< adaptive_pool<void>, red_black_tree >()){
      std::cerr << "test_map_variants< adaptive_pool<void> > failed" << std::endl;
      return 1;
   }

   ////////////////////////////////////
   //    Tree implementations
   ////////////////////////////////////
   //       AVL
   if(test_map_variants< std::allocator<void>, avl_tree >()){
      std::cerr << "test_map_variants< std::allocator<void>, avl_tree > failed" << std::endl;
      return 1;
   }
   //    SCAPEGOAT TREE
   if(test_map_variants< std::allocator<void>, scapegoat_tree >()){
      std::cerr << "test_map_variants< std::allocator<void>, scapegoat_tree > failed" << std::endl;
      return 1;
   }
   //    SPLAY TREE
   if(test_map_variants< std::allocator<void>, splay_tree >()){
      std::cerr << "test_map_variants< std::allocator<void>, splay_tree > failed" << std::endl;
      return 1;
   }

   ////////////////////////////////////
   //    Emplace testing
   ////////////////////////////////////
   const test::EmplaceOptions MapOptions = (test::EmplaceOptions)(test::EMPLACE_HINT_PAIR | test::EMPLACE_ASSOC_PAIR);
   if(!boost::container::test::test_emplace<map<test::EmplaceInt, test::EmplaceInt>, MapOptions>())
      return 1;
   if(!boost::container::test::test_emplace<multimap<test::EmplaceInt, test::EmplaceInt>, MapOptions>())
      return 1;

   ////////////////////////////////////
   //    Allocator propagation testing
   ////////////////////////////////////
   if(!boost::container::test::test_propagate_allocator<boost_container_map>())
      return 1;

   if(!boost::container::test::test_propagate_allocator<boost_container_multimap>())
      return 1;

   if (!boost::container::test::test_map_support_for_initialization_list_for<map<int, int> >())
      return 1;

   if (!boost::container::test::test_map_support_for_initialization_list_for<multimap<int, int> >())
      return 1;

   ////////////////////////////////////
   //    Iterator testing
   ////////////////////////////////////
   {
      typedef boost::container::map<int, int> cont_int;
      cont_int a; a.insert(cont_int::value_type(0, 9)); a.insert(cont_int::value_type(1, 9)); a.insert(cont_int::value_type(2, 9));
      boost::intrusive::test::test_iterator_bidirectional< cont_int >(a);
      if(boost::report_errors() != 0) {
         return 1;
      }
   }
   {
      typedef boost::container::multimap<int, int> cont_int;
      cont_int a; a.insert(cont_int::value_type(0, 9)); a.insert(cont_int::value_type(1, 9)); a.insert(cont_int::value_type(2, 9));
      boost::intrusive::test::test_iterator_bidirectional< cont_int >(a);
      if(boost::report_errors() != 0) {
         return 1;
      }
   }

   ////////////////////////////////////
   //    Test optimize_size option
   ////////////////////////////////////
   //
   // map
   //
   typedef map< int*, int*, std::less<int*>, std::allocator< std::pair<int const*, int*> >
              , tree_assoc_options< optimize_size<false>, tree_type<red_black_tree> >::type > rbmap_size_optimized_no;
   typedef map< int*, int*, std::less<int*>, std::allocator< std::pair<int const*, int*> >
              , tree_assoc_options< optimize_size<true>, tree_type<red_black_tree>  >::type > rbmap_size_optimized_yes;
   BOOST_STATIC_ASSERT(sizeof(rbmap_size_optimized_yes) < sizeof(rbmap_size_optimized_no));

   typedef map< int*, int*, std::less<int*>, std::allocator< std::pair<int const*, int*> >
              , tree_assoc_options< optimize_size<false>, tree_type<avl_tree> >::type > avlmap_size_optimized_no;
   typedef map< int*, int*, std::less<int*>, std::allocator< std::pair<int const*, int*> >
              , tree_assoc_options< optimize_size<true>, tree_type<avl_tree>  >::type > avlmap_size_optimized_yes;
   BOOST_STATIC_ASSERT(sizeof(avlmap_size_optimized_yes) < sizeof(avlmap_size_optimized_no));
   //
   // multimap
   //
   typedef multimap< int*, int*, std::less<int*>, std::allocator< std::pair<int const*, int*> >
                   , tree_assoc_options< optimize_size<false>, tree_type<red_black_tree> >::type > rbmmap_size_optimized_no;
   typedef multimap< int*, int*, std::less<int*>, std::allocator< std::pair<int const*, int*> >
                   , tree_assoc_options< optimize_size<true>, tree_type<red_black_tree>  >::type > rbmmap_size_optimized_yes;
   BOOST_STATIC_ASSERT(sizeof(rbmmap_size_optimized_yes) < sizeof(rbmmap_size_optimized_no));

   typedef multimap< int*, int*, std::less<int*>, std::allocator< std::pair<int const*, int*> >
                   , tree_assoc_options< optimize_size<false>, tree_type<avl_tree> >::type > avlmmap_size_optimized_no;
   typedef multimap< int*, int*, std::less<int*>, std::allocator< std::pair<int const*, int*> >
                   , tree_assoc_options< optimize_size<true>, tree_type<avl_tree>  >::type > avlmmap_size_optimized_yes;
   BOOST_STATIC_ASSERT(sizeof(avlmmap_size_optimized_yes) < sizeof(avlmmap_size_optimized_no));

   return 0;
}

#include <boost/container/detail/config_end.hpp>
