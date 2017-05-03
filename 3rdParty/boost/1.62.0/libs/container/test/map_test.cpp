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

typedef std::pair<const test::movable_and_copyable_int, test::movable_and_copyable_int> pair_t;

namespace boost {
namespace container {

//Explicit instantiation to detect compilation errors

//map
template class map
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , test::simple_allocator< pair_t >
   >;

template class map
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , std::allocator< pair_t >
   >;

template class map
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , adaptive_pool< pair_t >
   >;

template class multimap
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , std::allocator< pair_t >
   >;

namespace container_detail {

//Instantiate base class as previous instantiations don't instantiate inherited members
template class tree
   < pair_t
   , select1st<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , test::simple_allocator<pair_t>
   , tree_assoc_defaults
   >;

template class tree
   < pair_t
   , select1st<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , std::allocator<pair_t>
   , tree_assoc_defaults
   >;

template class tree
   < pair_t
   , select1st<test::movable_and_copyable_int>
   , std::less<test::movable_and_copyable_int>
   , adaptive_pool<pair_t>
   , tree_assoc_defaults
   >;

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

bool node_type_test()
{
   using namespace boost::container;
   {
      typedef map<test::movable_int, test::movable_int> map_type;
      map_type src;
      {
         test::movable_int mv_1(1), mv_2(2), mv_3(3), mv_11(11), mv_12(12), mv_13(13);
         src.try_emplace(boost::move(mv_1), boost::move(mv_11)); 
         src.try_emplace(boost::move(mv_2), boost::move(mv_12)); 
         src.try_emplace(boost::move(mv_3), boost::move(mv_13)); 
      }
      if(src.size() != 3)
         return false;

      map_type dst;
      {
         test::movable_int mv_3(3), mv_33(33);
         dst.try_emplace(boost::move(mv_3), boost::move(mv_33)); 
      }

      if(dst.size() != 1)
         return false;

      const test::movable_int mv_1(1);
      const test::movable_int mv_2(2);
      const test::movable_int mv_3(3);
      const test::movable_int mv_33(33);
      const test::movable_int mv_13(13);
      map_type::insert_return_type r;

      r = dst.insert(src.extract(mv_33)); // Key version, try to insert empty node
      if(! (r.position == dst.end() && r.inserted == false && r.node.empty()) )
         return false;
      r = dst.insert(src.extract(src.find(mv_1))); // Iterator version, successful
      if(! (r.position == dst.find(mv_1) && r.inserted == true && r.node.empty()) )
         return false;
      r = dst.insert(dst.begin(), src.extract(mv_2)); // Key type version, successful
      if(! (r.position == dst.find(mv_2) && r.inserted == true && r.node.empty()) )
         return false;
      r = dst.insert(src.extract(mv_3)); // Key type version, unsuccessful

      if(!src.empty())
         return false;
      if(dst.size() != 3)
         return false;
      if(! (r.position == dst.find(mv_3) && r.inserted == false && r.node.key() == mv_3 && r.node.mapped() == mv_13) )
         return false;
   }

   {
      typedef multimap<test::movable_int, test::movable_int> multimap_type;
      multimap_type src;
      {
         test::movable_int mv_1(1), mv_2(2), mv_3(3), mv_3bis(3), mv_11(11), mv_12(12), mv_13(13), mv_23(23);
         src.emplace(boost::move(mv_1), boost::move(mv_11));
         src.emplace(boost::move(mv_2), boost::move(mv_12));
         src.emplace(boost::move(mv_3), boost::move(mv_13));
         src.emplace_hint(src.begin(), boost::move(mv_3bis), boost::move(mv_23));
      }
      if(src.size() != 4)
         return false;

      multimap_type dst;
      {
         test::movable_int mv_3(3), mv_33(33);
         dst.emplace(boost::move(mv_3), boost::move(mv_33)); 
      }

      if(dst.size() != 1)
         return false;

      const test::movable_int mv_1(1);
      const test::movable_int mv_2(2);
      const test::movable_int mv_3(3);
      const test::movable_int mv_4(4);
      const test::movable_int mv_33(33);
      const test::movable_int mv_13(13);
      const test::movable_int mv_23(23);
      multimap_type::iterator r;

      multimap_type::node_type nt(src.extract(mv_3));
      r = dst.insert(dst.begin(), boost::move(nt));
      if(! (r->first == mv_3 && r->second == mv_23 && dst.find(mv_3) == r && nt.empty()) )
         return false;

      nt = src.extract(src.find(mv_1));
      r = dst.insert(boost::move(nt)); // Iterator version, successful
      if(! (r->first == mv_1 && nt.empty()) )
         return false;

      nt = src.extract(mv_2);
      r = dst.insert(boost::move(nt)); // Key type version, successful
      if(! (r->first == mv_2 && nt.empty()) )
         return false;

      r = dst.insert(src.extract(mv_3)); // Key type version, successful
      if(! (r->first == mv_3 && r->second == mv_13 && r == --multimap_type::iterator(dst.upper_bound(mv_3)) && nt.empty()) )
         return false;

      r = dst.insert(src.extract(mv_4)); // Key type version, unsuccessful
      if(! (r == dst.end()) )
         return false;

      if(!src.empty())
         return false;
      if(dst.size() != 5)
         return false;
   }
   return true;
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

void test_merge_from_different_comparison()
{
   map<int, int> map1;
   map<int, int, std::greater<int> > map2;
   map1.merge(map2);
}

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

   //Test std::pair value type as tree has workarounds to make old std::pair
   //implementations movable that can break things
   {
	   boost::container::map<pair_t, pair_t> s;
	   std::pair<const pair_t,pair_t> p;
	   s.insert(p);
	   s.emplace(p);
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
   //    Node extraction/insertion testing functions
   ////////////////////////////////////
   if(!node_type_test())
      return 1;

   test::test_merge_from_different_comparison();

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
