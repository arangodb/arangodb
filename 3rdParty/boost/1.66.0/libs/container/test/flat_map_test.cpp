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
#include <boost/container/flat_map.hpp>
#include <boost/container/allocator.hpp>
#include <boost/container/detail/flat_tree.hpp>
#include <boost/container/stable_vector.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/deque.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/container/detail/container_or_allocator_rebind.hpp>

#include "print_container.hpp"
#include "dummy_test_allocator.hpp"
#include "movable_int.hpp"
#include "map_test.hpp"
#include "propagate_allocator_test.hpp"
#include "container_common_tests.hpp"
#include "emplace_test.hpp"
#include "../../intrusive/test/iterator_test.hpp"

#include <map>


using namespace boost::container;

namespace boost {
namespace container {

//Explicit instantiation to detect compilation errors

//flat_map
typedef std::pair<test::movable_and_copyable_int, test::movable_and_copyable_int> test_pair_t;

template class flat_map
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , test::simple_allocator< test_pair_t >
   >;

template class flat_map
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , small_vector< test_pair_t, 10, std::allocator< test_pair_t > >
   >;

//flat_multimap
template class flat_multimap
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , stable_vector< test_pair_t, allocator< test_pair_t > >
   >;

template class flat_multimap
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , deque<test_pair_t, test::simple_allocator< test_pair_t > >
   >;

template class flat_multimap
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , static_vector<test_pair_t, 10 >
   >;

//As flat container iterators are typedefs for vector::[const_]iterator,
//no need to explicit instantiate them

}} //boost::container

#if (__cplusplus > 201103L)
#include <vector>

namespace boost{
namespace container{

template class flat_map
   < test::movable_and_copyable_int
   , test::movable_and_copyable_int
   , std::less<test::movable_and_copyable_int>
   , std::vector<test_pair_t>
>;

}} //boost::container

#endif

class recursive_flat_map
{
   public:
   recursive_flat_map(const recursive_flat_map &c)
      : id_(c.id_), map_(c.map_)
   {}

   recursive_flat_map & operator =(const recursive_flat_map &c)
   {
      id_ = c.id_;
      map_= c.map_;
      return *this;
   }

   int id_;
   flat_map<recursive_flat_map, recursive_flat_map> map_;
   flat_map<recursive_flat_map, recursive_flat_map>::iterator it_;
   flat_map<recursive_flat_map, recursive_flat_map>::const_iterator cit_;
   flat_map<recursive_flat_map, recursive_flat_map>::reverse_iterator rit_;
   flat_map<recursive_flat_map, recursive_flat_map>::const_reverse_iterator crit_;

   friend bool operator< (const recursive_flat_map &a, const recursive_flat_map &b)
   {  return a.id_ < b.id_;   }
};


class recursive_flat_multimap
{
public:
   recursive_flat_multimap(const recursive_flat_multimap &c)
      : id_(c.id_), map_(c.map_)
   {}

   recursive_flat_multimap & operator =(const recursive_flat_multimap &c)
   {
      id_ = c.id_;
      map_= c.map_;
      return *this;
   }
   int id_;
   flat_multimap<recursive_flat_multimap, recursive_flat_multimap> map_;
   flat_multimap<recursive_flat_multimap, recursive_flat_multimap>::iterator it_;
   flat_multimap<recursive_flat_multimap, recursive_flat_multimap>::const_iterator cit_;
   flat_multimap<recursive_flat_multimap, recursive_flat_multimap>::reverse_iterator rit_;
   flat_multimap<recursive_flat_multimap, recursive_flat_multimap>::const_reverse_iterator crit_;

   friend bool operator< (const recursive_flat_multimap &a, const recursive_flat_multimap &b)
   {  return a.id_ < b.id_;   }
};

template<class C>
void test_move()
{
   //Now test move semantics
   C original;
   C move_ctor(boost::move(original));
   C move_assign;
   move_assign = boost::move(move_ctor);
   move_assign.swap(original);
}


namespace boost{
namespace container {
namespace test{

bool flat_tree_ordered_insertion_test()
{
   using namespace boost::container;
   const std::size_t NumElements = 100;

   //Ordered insertion multimap
   {
      std::multimap<int, int> int_mmap;
      for(std::size_t i = 0; i != NumElements; ++i){
         int_mmap.insert(std::multimap<int, int>::value_type(static_cast<int>(i), static_cast<int>(i)));
      }
      //Construction insertion
      flat_multimap<int, int> fmmap(ordered_range, int_mmap.begin(), int_mmap.end());
      if(!CheckEqualContainers(int_mmap, fmmap))
         return false;
      //Insertion when empty
      fmmap.clear();
      fmmap.insert(ordered_range, int_mmap.begin(), int_mmap.end());
      if(!CheckEqualContainers(int_mmap, fmmap))
         return false;
      //Re-insertion
      fmmap.insert(ordered_range, int_mmap.begin(), int_mmap.end());
      std::multimap<int, int> int_mmap2(int_mmap);
      int_mmap2.insert(int_mmap.begin(), int_mmap.end());
      if(!CheckEqualContainers(int_mmap2, fmmap))
         return false;
      //Re-re-insertion
      fmmap.insert(ordered_range, int_mmap2.begin(), int_mmap2.end());
      std::multimap<int, int> int_mmap4(int_mmap2);
      int_mmap4.insert(int_mmap2.begin(), int_mmap2.end());
      if(!CheckEqualContainers(int_mmap4, fmmap))
         return false;
      //Re-re-insertion of even
      std::multimap<int, int> int_even_mmap;
      for(std::size_t i = 0; i < NumElements; i+=2){
         int_mmap.insert(std::multimap<int, int>::value_type(static_cast<int>(i), static_cast<int>(i)));
      }
      fmmap.insert(ordered_range, int_even_mmap.begin(), int_even_mmap.end());
      int_mmap4.insert(int_even_mmap.begin(), int_even_mmap.end());
      if(!CheckEqualContainers(int_mmap4, fmmap))
         return false;
   }

   //Ordered insertion map
   {
      std::map<int, int> int_map;
      for(std::size_t i = 0; i != NumElements; ++i){
         int_map.insert(std::map<int, int>::value_type(static_cast<int>(i), static_cast<int>(i)));
      }
      //Construction insertion
      flat_map<int, int> fmap(ordered_unique_range, int_map.begin(), int_map.end());
      if(!CheckEqualContainers(int_map, fmap))
         return false;
      //Insertion when empty
      fmap.clear();
      fmap.insert(ordered_unique_range, int_map.begin(), int_map.end());
      if(!CheckEqualContainers(int_map, fmap))
         return false;
      //Re-insertion
      fmap.insert(ordered_unique_range, int_map.begin(), int_map.end());
      std::map<int, int> int_map2(int_map);
      int_map2.insert(int_map.begin(), int_map.end());
      if(!CheckEqualContainers(int_map2, fmap))
         return false;
      //Re-re-insertion
      fmap.insert(ordered_unique_range, int_map2.begin(), int_map2.end());
      std::map<int, int> int_map4(int_map2);
      int_map4.insert(int_map2.begin(), int_map2.end());
      if(!CheckEqualContainers(int_map4, fmap))
         return false;
      //Re-re-insertion of even
      std::map<int, int> int_even_map;
      for(std::size_t i = 0; i < NumElements; i+=2){
         int_map.insert(std::map<int, int>::value_type(static_cast<int>(i), static_cast<int>(i)));
      }
      fmap.insert(ordered_unique_range, int_even_map.begin(), int_even_map.end());
      int_map4.insert(int_even_map.begin(), int_even_map.end());
      if(!CheckEqualContainers(int_map4, fmap))
         return false;
   }

   return true;
}

template< class RandomIt >
void random_shuffle( RandomIt first, RandomIt last )
{
   typedef typename boost::container::iterator_traits<RandomIt>::difference_type difference_type;
   difference_type n = last - first;
   for (difference_type i = n-1; i > 0; --i) {
      difference_type j = std::rand() % (i+1);
      if(j != i) {
         boost::adl_move_swap(first[i], first[j]);
      }
   }
}

bool flat_tree_extract_adopt_test()
{
   using namespace boost::container;
   const std::size_t NumElements = 100;

   //extract/adopt map
   {
      //Construction insertion
      flat_map<int, int> fmap;

      for(std::size_t i = 0; i != NumElements; ++i){
         fmap.emplace(static_cast<int>(i), -static_cast<int>(i));
      }

      flat_map<int, int> fmap_copy(fmap);
      flat_map<int, int>::sequence_type seq(fmap.extract_sequence());
      if(!fmap.empty())
         return false;
      if(!CheckEqualContainers(seq, fmap_copy))
         return false;

      seq.insert(seq.end(), fmap_copy.begin(), fmap_copy.end());
      boost::container::test::random_shuffle(seq.begin(), seq.end());
      fmap.adopt_sequence(boost::move(seq));
      if(!CheckEqualContainers(fmap, fmap_copy))
         return false;
   }

   //extract/adopt map, ordered_unique_range
   {
      //Construction insertion
      flat_map<int, int> fmap;

      for(std::size_t i = 0; i != NumElements; ++i){
         fmap.emplace(static_cast<int>(i), -static_cast<int>(i));
      }

      flat_map<int, int> fmap_copy(fmap);
      flat_map<int, int>::sequence_type seq(fmap.extract_sequence());
      if(!fmap.empty())
         return false;
      if(!CheckEqualContainers(seq, fmap_copy))
         return false;

      fmap.adopt_sequence(ordered_unique_range, boost::move(seq));
      if(!CheckEqualContainers(fmap, fmap_copy))
         return false;
   }

   //extract/adopt multimap
   {
      //Construction insertion
      flat_multimap<int, int> fmmap;

      for(std::size_t i = 0; i != NumElements; ++i){
         fmmap.emplace(static_cast<int>(i), -static_cast<int>(i));
         fmmap.emplace(static_cast<int>(i), -static_cast<int>(i));
      }

      flat_multimap<int, int> fmmap_copy(fmmap);
      flat_multimap<int, int>::sequence_type seq(fmmap.extract_sequence());
      if(!fmmap.empty())
         return false;
      if(!CheckEqualContainers(seq, fmmap_copy))
         return false;

      boost::container::test::random_shuffle(seq.begin(), seq.end());
      fmmap.adopt_sequence(boost::move(seq));
      if(!CheckEqualContainers(fmmap, fmmap_copy))
         return false;
   }

   //extract/adopt multimap, ordered_range
   {
      //Construction insertion
      flat_multimap<int, int> fmmap;

      for(std::size_t i = 0; i != NumElements; ++i){
         fmmap.emplace(static_cast<int>(i), -static_cast<int>(i));
         fmmap.emplace(static_cast<int>(i), -static_cast<int>(i));
      }

      flat_multimap<int, int> fmmap_copy(fmmap);
      flat_multimap<int, int>::sequence_type seq(fmmap.extract_sequence());
      if(!fmmap.empty())
         return false;
      if(!CheckEqualContainers(seq, fmmap_copy))
         return false;

      fmmap.adopt_sequence(ordered_range, boost::move(seq));
      if(!CheckEqualContainers(fmmap, fmmap_copy))
         return false;
   }

   return true;
}

}}}

template<class VoidAllocatorOrContainer>
struct GetMapContainer
{
   template<class ValueType>
   struct apply
   {
      typedef std::pair<ValueType, ValueType> type_t;
      typedef flat_map< ValueType
                 , ValueType
                 , std::less<ValueType>
                 , typename boost::container::container_detail::container_or_allocator_rebind<VoidAllocatorOrContainer, type_t>::type
                 > map_type;

      typedef flat_multimap< ValueType
                 , ValueType
                 , std::less<ValueType>
                 , typename boost::container::container_detail::container_or_allocator_rebind<VoidAllocatorOrContainer, type_t>::type
                 > multimap_type;
   };
};

struct boost_container_flat_map;
struct boost_container_flat_multimap;

namespace boost { namespace container {   namespace test {

template<>
struct alloc_propagate_base<boost_container_flat_map>
{
   template <class T, class Allocator>
   struct apply
   {
      typedef typename boost::container::allocator_traits<Allocator>::
         template portable_rebind_alloc<std::pair<T, T> >::type TypeAllocator;
      typedef boost::container::flat_map<T, T, std::less<T>, TypeAllocator> type;
   };
};

template<>
struct alloc_propagate_base<boost_container_flat_multimap>
{
   template <class T, class Allocator>
   struct apply
   {
      typedef typename boost::container::allocator_traits<Allocator>::
         template portable_rebind_alloc<std::pair<T, T> >::type TypeAllocator;
      typedef boost::container::flat_multimap<T, T, std::less<T>, TypeAllocator> type;
   };
};

template <class Key, class T, class Compare, class Allocator>
struct get_real_stored_allocator<flat_map<Key, T, Compare, Allocator> >
{
   typedef typename flat_map<Key, T, Compare, Allocator>::impl_stored_allocator_type type;
};

template <class Key, class T, class Compare, class Allocator>
struct get_real_stored_allocator<flat_multimap<Key, T, Compare, Allocator> >
{
   typedef typename flat_multimap<Key, T, Compare, Allocator>::impl_stored_allocator_type type;
};

}}}   //namespace boost::container::test

template<class VoidAllocatorOrContainer>
int test_map_variants()
{
   typedef typename GetMapContainer<VoidAllocatorOrContainer>::template apply<int>::map_type MyMap;
   typedef typename GetMapContainer<VoidAllocatorOrContainer>::template apply<test::movable_int>::map_type MyMoveMap;
   typedef typename GetMapContainer<VoidAllocatorOrContainer>::template apply<test::movable_and_copyable_int>::map_type MyCopyMoveMap;
   typedef typename GetMapContainer<VoidAllocatorOrContainer>::template apply<test::copyable_int>::map_type MyCopyMap;

   typedef typename GetMapContainer<VoidAllocatorOrContainer>::template apply<int>::multimap_type MyMultiMap;
   typedef typename GetMapContainer<VoidAllocatorOrContainer>::template apply<test::movable_int>::multimap_type MyMoveMultiMap;
   typedef typename GetMapContainer<VoidAllocatorOrContainer>::template apply<test::movable_and_copyable_int>::multimap_type MyCopyMoveMultiMap;
   typedef typename GetMapContainer<VoidAllocatorOrContainer>::template apply<test::copyable_int>::multimap_type MyCopyMultiMap;

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

int main()
{
   using namespace boost::container::test;

   //Allocator argument container
   {
      flat_map<int, int> map_((flat_map<int, int>::allocator_type()));
      flat_multimap<int, int> multimap_((flat_multimap<int, int>::allocator_type()));
   }
   //Now test move semantics
   {
      test_move<flat_map<recursive_flat_map, recursive_flat_map> >();
      test_move<flat_multimap<recursive_flat_multimap, recursive_flat_multimap> >();
   }
   //Now test nth/index_of
   {
      flat_map<int, int> map;
      flat_multimap<int, int> mmap;

      map.insert(std::pair<int, int>(0, 0));
      map.insert(std::pair<int, int>(1, 0));
      map.insert(std::pair<int, int>(2, 0));
      mmap.insert(std::pair<int, int>(0, 0));
      mmap.insert(std::pair<int, int>(1, 0));
      mmap.insert(std::pair<int, int>(2, 0));
      if(!boost::container::test::test_nth_index_of(map))
         return 1;
      if(!boost::container::test::test_nth_index_of(mmap))
         return 1;
   }

   ////////////////////////////////////
   //    Ordered insertion test
   ////////////////////////////////////
   if(!flat_tree_ordered_insertion_test()){
      return 1;
   }

   ////////////////////////////////////
   //    Extract/Adopt test
   ////////////////////////////////////
   if(!flat_tree_extract_adopt_test()){
      return 1;
   }

   if (!boost::container::test::instantiate_constructors<flat_map<int, int>, flat_multimap<int, int> >())
      return 1;

   ////////////////////////////////////
   //    Testing allocator implementations
   ////////////////////////////////////
   //       std::allocator
   if(test_map_variants< std::allocator<void> >()){
      std::cerr << "test_map_variants< std::allocator<void> > failed" << std::endl;
      return 1;
   }
   //       boost::container::allocator
   if(test_map_variants< allocator<void> >()){
      std::cerr << "test_map_variants< allocator<void> > failed" << std::endl;
      return 1;
   }

   if(!boost::container::test::test_map_support_for_initialization_list_for<flat_map<int, int> >())
      return 1;

   if (!boost::container::test::test_map_support_for_initialization_list_for<flat_multimap<int, int> >())
      return 1;

   ////////////////////////////////////
   //    Emplace testing
   ////////////////////////////////////
   const test::EmplaceOptions MapOptions = (test::EmplaceOptions)(test::EMPLACE_HINT_PAIR | test::EMPLACE_ASSOC_PAIR);

   if(!boost::container::test::test_emplace<flat_map<test::EmplaceInt, test::EmplaceInt>, MapOptions>())
      return 1;
   if(!boost::container::test::test_emplace<flat_multimap<test::EmplaceInt, test::EmplaceInt>, MapOptions>())
      return 1;

   ////////////////////////////////////
   //    Allocator propagation testing
   ////////////////////////////////////
   if(!boost::container::test::test_propagate_allocator<boost_container_flat_map>())
      return 1;

   if(!boost::container::test::test_propagate_allocator<boost_container_flat_multimap>())
      return 1;

   ////////////////////////////////////
   //    Iterator testing
   ////////////////////////////////////
   {
      typedef boost::container::flat_map<int, int> cont_int;
      cont_int a; a.insert(cont_int::value_type(0, 9)); a.insert(cont_int::value_type(1, 9)); a.insert(cont_int::value_type(2, 9));
      boost::intrusive::test::test_iterator_random< cont_int >(a);
      if(boost::report_errors() != 0) {
         return 1;
      }
   }
   {
      typedef boost::container::flat_multimap<int, int> cont_int;
      cont_int a; a.insert(cont_int::value_type(0, 9)); a.insert(cont_int::value_type(1, 9)); a.insert(cont_int::value_type(2, 9));
      boost::intrusive::test::test_iterator_random< cont_int >(a);
      if(boost::report_errors() != 0) {
         return 1;
      }
   }

   return 0;
}

#include <boost/container/detail/config_end.hpp>

