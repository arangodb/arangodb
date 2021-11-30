//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <set>
#include <utility>
#include <vector>

#include <boost/container/flat_set.hpp>
#include <boost/container/detail/container_or_allocator_rebind.hpp>

#include "print_container.hpp"
#include "dummy_test_allocator.hpp"
#include "movable_int.hpp"
#include "set_test.hpp"
#include "propagate_allocator_test.hpp"
#include "emplace_test.hpp"
#include "container_common_tests.hpp"
#include "../../intrusive/test/iterator_test.hpp"

using namespace boost::container;

//Test recursive structures
class recursive_flat_set
{
   public:
   recursive_flat_set(const recursive_flat_set &c)
      : id_(c.id_), flat_set_(c.flat_set_)
   {}

   recursive_flat_set & operator =(const recursive_flat_set &c)
   {
      id_ = c.id_;
      flat_set_= c.flat_set_;
      return *this;
   }
   int id_;
   flat_set<recursive_flat_set> flat_set_;
   flat_set<recursive_flat_set>::iterator it_;
   flat_set<recursive_flat_set>::const_iterator cit_;
   flat_set<recursive_flat_set>::reverse_iterator rit_;
   flat_set<recursive_flat_set>::const_reverse_iterator crit_;

   friend bool operator< (const recursive_flat_set &a, const recursive_flat_set &b)
   {  return a.id_ < b.id_;   }
};


//Test recursive structures
class recursive_flat_multiset
{
   public:
   recursive_flat_multiset(const recursive_flat_multiset &c)
      : id_(c.id_), flat_multiset_(c.flat_multiset_)
   {}

   recursive_flat_multiset & operator =(const recursive_flat_multiset &c)
   {
      id_ = c.id_;
      flat_multiset_= c.flat_multiset_;
      return *this;
   }
   int id_;
   flat_multiset<recursive_flat_multiset> flat_multiset_;
   flat_multiset<recursive_flat_multiset>::iterator it_;
   flat_multiset<recursive_flat_multiset>::const_iterator cit_;
   flat_multiset<recursive_flat_multiset>::reverse_iterator rit_;
   flat_multiset<recursive_flat_multiset>::const_reverse_iterator crit_;

   friend bool operator< (const recursive_flat_multiset &a, const recursive_flat_multiset &b)
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

   //Ordered insertion multiset
   {
      std::multiset<int> int_mset;
      for(std::size_t i = 0; i != NumElements; ++i){
         int_mset.insert(static_cast<int>(i));
      }
      //Construction insertion
      flat_multiset<int> fmset(ordered_range, int_mset.begin(), int_mset.end());
      if(!CheckEqualContainers(int_mset, fmset))
         return false;
      //Insertion when empty
      fmset.clear();
      fmset.insert(ordered_range, int_mset.begin(), int_mset.end());
      if(!CheckEqualContainers(int_mset, fmset))
         return false;
      //Re-insertion
      fmset.insert(ordered_range, int_mset.begin(), int_mset.end());
      std::multiset<int> int_mset2(int_mset);
      int_mset2.insert(int_mset.begin(), int_mset.end());
      if(!CheckEqualContainers(int_mset2, fmset))
         return false;
      //Re-re-insertion
      fmset.insert(ordered_range, int_mset2.begin(), int_mset2.end());
      std::multiset<int> int_mset4(int_mset2);
      int_mset4.insert(int_mset2.begin(), int_mset2.end());
      if(!CheckEqualContainers(int_mset4, fmset))
         return false;
      //Re-re-insertion of even
      std::multiset<int> int_even_mset;
      for(std::size_t i = 0; i < NumElements; i+=2){
         int_mset.insert(static_cast<int>(i));
      }
      fmset.insert(ordered_range, int_even_mset.begin(), int_even_mset.end());
      int_mset4.insert(int_even_mset.begin(), int_even_mset.end());
      if(!CheckEqualContainers(int_mset4, fmset))
         return false;

      //Re-re-insertion using in-place merge
      fmset.reserve(fmset.size() + int_mset2.size());
      fmset.insert(ordered_range, int_mset2.begin(), int_mset2.end());
      std::multiset<int> int_mset5(int_mset2);
      int_mset4.insert(int_mset5.begin(), int_mset5.end());
      if(!CheckEqualContainers(int_mset4, fmset))
         return false;
      //Re-re-insertion of even
      std::multiset<int> int_even_mset2;
      for(std::size_t i = 0; i < NumElements; i+=2){
         int_even_mset2.insert(static_cast<int>(i));
      }
      fmset.reserve(fmset.size() + int_even_mset2.size());
      fmset.insert(ordered_range, int_even_mset2.begin(), int_even_mset2.end());
      int_mset4.insert(int_even_mset2.begin(), int_even_mset2.end());
      if(!CheckEqualContainers(int_mset4, fmset))
         return false;
   }

   //Ordered insertion set
   {
      std::set<int> int_set;
      for(std::size_t i = 0; i != NumElements; ++i){
         int_set.insert(static_cast<int>(i));
      }
      //Construction insertion
      flat_set<int> fset(ordered_unique_range, int_set.begin(), int_set.end());
      if(!CheckEqualContainers(int_set, fset))
         return false;
      //Insertion when empty
      fset.clear();
      fset.insert(ordered_unique_range, int_set.begin(), int_set.end());
      if(!CheckEqualContainers(int_set, fset))
         return false;
      //Re-insertion
      fset.insert(ordered_unique_range, int_set.begin(), int_set.end());
      std::set<int> int_set2(int_set);
      int_set2.insert(int_set.begin(), int_set.end());
      if(!CheckEqualContainers(int_set2, fset))
         return false;
      //Re-re-insertion
      fset.insert(ordered_unique_range, int_set2.begin(), int_set2.end());
      std::set<int> int_set4(int_set2);
      int_set4.insert(int_set2.begin(), int_set2.end());
      if(!CheckEqualContainers(int_set4, fset))
         return false;
      //Re-re-insertion of even
      std::set<int> int_even_set;
      for(std::size_t i = 0; i < NumElements; i+=2){
         int_even_set.insert(static_cast<int>(i));
      }
      fset.insert(ordered_unique_range, int_even_set.begin(), int_even_set.end());
      int_set4.insert(int_even_set.begin(), int_even_set.end());
      if(!CheckEqualContainers(int_set4, fset))
         return false;
      //Partial Re-re-insertion of even
      int_even_set.clear();
      for(std::size_t i = 0; i < NumElements; i+=4){
         int_even_set.insert(static_cast<int>(i));
      }
      fset.clear();
      int_set4.clear();
      //insert 0,4,8,12...
      fset.insert(ordered_unique_range, int_even_set.begin(), int_even_set.end());
      int_set4.insert(int_even_set.begin(), int_even_set.end());
      if(!CheckEqualContainers(int_set4, fset))
         return false;
      for(std::size_t i = 2; i < NumElements; i+=4){
         int_even_set.insert(static_cast<int>(i));
      }
      //insert 0,2,4,6,8,10,12...
      fset.insert(ordered_unique_range, int_even_set.begin(), int_even_set.end());
      int_set4.insert(int_even_set.begin(), int_even_set.end());
      if(!CheckEqualContainers(int_set4, fset))
         return false;
      int_even_set.clear();
      for(std::size_t i = 0; i < NumElements; i+=8){
         int_even_set.insert(static_cast<int>(i));
      }
      fset.clear();
      int_set4.clear();
      //insert 0,8,16...
      fset.insert(ordered_unique_range, int_even_set.begin(), int_even_set.end());
      int_set4.insert(int_even_set.begin(), int_even_set.end());
      if(!CheckEqualContainers(int_set4, fset))
         return false;
      for(std::size_t i = 0; i < NumElements; i+=2){
         int_even_set.insert(static_cast<int>(i));
      }
      //insert 0,2,4,6,8,10,12...
      fset.insert(ordered_unique_range, int_even_set.begin(), int_even_set.end());
      int_set4.insert(int_even_set.begin(), int_even_set.end());
      if(!CheckEqualContainers(int_set4, fset))
         return false;


      int_even_set.clear();
      for(std::size_t i = 0; i < NumElements; i+=8){
         int_even_set.insert(static_cast<int>(i));
         int_even_set.insert(static_cast<int>(i+2));
      }
      int_even_set.insert(static_cast<int>(NumElements-2));
      fset.clear();
      int_set4.clear();
      //insert 0,2,8,10...
      fset.insert(ordered_unique_range, int_even_set.begin(), int_even_set.end());
      int_set4.insert(int_even_set.begin(), int_even_set.end());
      if(!CheckEqualContainers(int_set4, fset))
         return false;
      for(std::size_t i = 0; i < NumElements; i+=2){
         int_even_set.insert(static_cast<int>(i));
      }
      //insert 0,2,4,6,8,10,12...
      fset.insert(ordered_unique_range, int_even_set.begin(), int_even_set.end());
      int_set4.insert(int_even_set.begin(), int_even_set.end());
      if(!CheckEqualContainers(int_set4, fset))
         return false;

      //add even/odd values with not enough capacity 
      flat_set<int>().swap(fset);
      int_set4.clear();
      int_set.clear();

      fset.reserve(int_even_set.size());
      fset.insert(ordered_unique_range, int_even_set.begin(), int_even_set.end());
      int_set4.insert(int_even_set.begin(), int_even_set.end());

      for(std::size_t i = 0; i < NumElements*2; i+=2){
         int_set.insert(static_cast<int>(i));
         int_set.insert(static_cast<int>(i+1));
      }

      fset.insert(ordered_unique_range, int_set.begin(), int_set.end());
      int_set4.insert(int_set.begin(), int_set.end());
      if(!CheckEqualContainers(int_set4, fset))
         return false;
   }

   return true;
}

bool constructor_template_auto_deduction_test()
{

#ifndef BOOST_CONTAINER_NO_CXX17_CTAD
   using namespace boost::container;
   const std::size_t NumElements = 100;
   {
      std::set<int> int_set;
      for (std::size_t i = 0; i != NumElements; ++i) {
         int_set.insert(static_cast<int>(i));
      }
      std::multiset<int> int_mset;
      for (std::size_t i = 0; i != NumElements; ++i) {
         int_mset.insert(static_cast<int>(i));
      }

      typedef std::less<int> comp_int_t;
      typedef std::allocator<int> alloc_int_t;

      //range
      {
         auto fset = flat_set(int_set.begin(), int_set.end());
         if (!CheckEqualContainers(int_set, fset))
            return false;
         auto fmset = flat_multiset(int_mset.begin(), int_mset.end());
         if (!CheckEqualContainers(int_mset, fmset))
            return false;
      }
      //range+comp
      {
         auto fset = flat_set(int_set.begin(), int_set.end(), comp_int_t());
         if (!CheckEqualContainers(int_set, fset))
            return false;
         auto fmset = flat_multiset(int_mset.begin(), int_mset.end(), comp_int_t());
         if (!CheckEqualContainers(int_mset, fmset))
            return false;
      }
      //range+comp+alloc
      {
         auto fset = flat_set(int_set.begin(), int_set.end(), comp_int_t(), alloc_int_t());
         if (!CheckEqualContainers(int_set, fset))
            return false;
         auto fmset = flat_multiset(int_mset.begin(), int_mset.end(), comp_int_t(), alloc_int_t());
         if (!CheckEqualContainers(int_mset, fmset))
            return false;
      }
      //range+alloc
      {
         auto fset = flat_set(int_set.begin(), int_set.end(), alloc_int_t());
         if (!CheckEqualContainers(int_set, fset))
            return false;
         auto fmset = flat_multiset(int_mset.begin(), int_mset.end(), alloc_int_t());
         if (!CheckEqualContainers(int_mset, fmset))
            return false;
      }

      //ordered_unique_range / ordered_range

      //range
      {
         auto fset = flat_set(ordered_unique_range, int_set.begin(), int_set.end());
         if (!CheckEqualContainers(int_set, fset))
            return false;
         auto fmset = flat_multiset(ordered_range, int_mset.begin(), int_mset.end());
         if (!CheckEqualContainers(int_mset, fmset))
            return false;
      }
      //range+comp
      {
         auto fset = flat_set(ordered_unique_range, int_set.begin(), int_set.end(), comp_int_t());
         if (!CheckEqualContainers(int_set, fset))
            return false;
         auto fmset = flat_multiset(ordered_range, int_mset.begin(), int_mset.end(), comp_int_t());
         if (!CheckEqualContainers(int_mset, fmset))
            return false;
      }
      //range+comp+alloc
      {
         auto fset = flat_set(ordered_unique_range, int_set.begin(), int_set.end(), comp_int_t(), alloc_int_t());
         if (!CheckEqualContainers(int_set, fset))
            return false;
         auto fmset = flat_multiset(ordered_range, int_mset.begin(), int_mset.end(), comp_int_t(), alloc_int_t());
         if (!CheckEqualContainers(int_mset, fmset))
            return false;
      }
      //range+alloc
      {
         auto fset = flat_set(ordered_unique_range, int_set.begin(), int_set.end(), alloc_int_t());
         if (!CheckEqualContainers(int_set, fset))
            return false;
         auto fmset = flat_multiset(ordered_range, int_mset.begin(), int_mset.end(), alloc_int_t());
         if (!CheckEqualContainers(int_mset, fmset))
            return false;
      }
   }
#endif

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

   //extract/adopt set
   {
      //Construction insertion
      flat_set<int> fset;

      for(std::size_t i = 0; i != NumElements; ++i){
         fset.insert(static_cast<int>(i));
      }

      flat_set<int> fset_copy(fset);
      flat_set<int>::sequence_type seq(fset.extract_sequence());
      if(!fset.empty())
         return false;
      if(!CheckEqualContainers(seq, fset_copy))
         return false;

      seq.insert(seq.end(), fset_copy.begin(), fset_copy.end());
      boost::container::test::random_shuffle(seq.begin(), seq.end());
      fset.adopt_sequence(boost::move(seq));
      if(!CheckEqualContainers(fset, fset_copy))
         return false;
   }

   //extract/adopt set, ordered_unique_range
   {
      //Construction insertion
      flat_set<int> fset;

      for(std::size_t i = 0; i != NumElements; ++i){
         fset.insert(static_cast<int>(i));
      }

      flat_set<int> fset_copy(fset);
      flat_set<int>::sequence_type seq(fset.extract_sequence());
      if(!fset.empty())
         return false;
      if(!CheckEqualContainers(seq, fset_copy))
         return false;

      fset.adopt_sequence(ordered_unique_range, boost::move(seq));
      if(!CheckEqualContainers(fset, fset_copy))
         return false;
   }

   //extract/adopt multiset
   {
      //Construction insertion
      flat_multiset<int> fmset;

      for(std::size_t i = 0; i != NumElements; ++i){
         fmset.insert(static_cast<int>(i));
         fmset.insert(static_cast<int>(i));
      }

      flat_multiset<int> fmset_copy(fmset);
      flat_multiset<int>::sequence_type seq(fmset.extract_sequence());
      if(!fmset.empty())
         return false;
      if(!CheckEqualContainers(seq, fmset_copy))
         return false;

      boost::container::test::random_shuffle(seq.begin(), seq.end());
      fmset.adopt_sequence(boost::move(seq));
      if(!CheckEqualContainers(fmset, fmset_copy))
         return false;
   }

   //extract/adopt multiset, ordered_range
   {
      //Construction insertion
      flat_multiset<int> fmset;

      for(std::size_t i = 0; i != NumElements; ++i){
         fmset.insert(static_cast<int>(i));
         fmset.insert(static_cast<int>(i));
      }

      flat_multiset<int> fmset_copy(fmset);
      flat_multiset<int>::sequence_type seq(fmset.extract_sequence());
      if(!fmset.empty())
         return false;
      if(!CheckEqualContainers(seq, fmset_copy))
         return false;

      fmset.adopt_sequence(ordered_range, boost::move(seq));
      if(!CheckEqualContainers(fmset, fmset_copy))
         return false;
   }

   return true;
}

bool test_heterogeneous_lookups()
{
   typedef flat_set<int, test::less_transparent> set_t;
   typedef flat_multiset<int, test::less_transparent> mset_t;

   set_t set1;
   mset_t mset1;

   const set_t &cset1 = set1;
   const mset_t &cmset1 = mset1;

   set1.insert(1);
   set1.insert(1);
   set1.insert(2);
   set1.insert(2);
   set1.insert(3);

   mset1.insert(1);
   mset1.insert(1);
   mset1.insert(2);
   mset1.insert(2);
   mset1.insert(3);

   const test::non_copymovable_int find_me(2);

   //find
   if(*set1.find(find_me) != 2)
      return false;
   if(*cset1.find(find_me) != 2)
      return false;
   if(*mset1.find(find_me) != 2)
      return false;
   if(*cmset1.find(find_me) != 2)
      return false;

   //count
   if(set1.count(find_me) != 1)
      return false;
   if(cset1.count(find_me) != 1)
      return false;
   if(mset1.count(find_me) != 2)
      return false;
   if(cmset1.count(find_me) != 2)
      return false;

   //contains
   if(!set1.contains(find_me))
      return false;
   if(!cset1.contains(find_me))
      return false;
   if(!mset1.contains(find_me))
      return false;
   if(!cmset1.contains(find_me))
      return false;

   //lower_bound
   if(*set1.lower_bound(find_me) != 2)
      return false;
   if(*cset1.lower_bound(find_me) != 2)
      return false;
   if(*mset1.lower_bound(find_me) != 2)
      return false;
   if(*cmset1.lower_bound(find_me) != 2)
      return false;

   //upper_bound
   if(*set1.upper_bound(find_me) != 3)
      return false;
   if(*cset1.upper_bound(find_me) != 3)
      return false;
   if(*mset1.upper_bound(find_me) != 3)
      return false;
   if(*cmset1.upper_bound(find_me) != 3)
      return false;

   //equal_range
   if(*set1.equal_range(find_me).first != 2)
      return false;
   if(*cset1.equal_range(find_me).second != 3)
      return false;
   if(*mset1.equal_range(find_me).first != 2)
      return false;
   if(*cmset1.equal_range(find_me).second != 3)
      return false;

   return true;
}

// An ordered sequence of std:pair is also ordered by std::pair::first.
struct with_lookup_by_first
{
   typedef void is_transparent;
   inline bool operator()(std::pair<int, int> a, std::pair<int, int> b) const
   {
      return a < b;
   }
   inline bool operator()(std::pair<int, int> a, int first) const
   {
      return a.first < first;
   }
   inline bool operator()(int first, std::pair<int, int> b) const
   {
      return first < b.first;
   }
};

bool test_heterogeneous_lookup_by_partial_key()
{
   typedef flat_set<std::pair<int, int>, with_lookup_by_first> set_t;

   set_t set1;
   set1.insert(std::pair<int, int>(0, 1));
   set1.insert(std::pair<int, int>(0, 2));

   std::pair<set_t::iterator, set_t::iterator> const first_0_range = set1.equal_range(0);
   if(2 != (first_0_range.second - first_0_range.first))
      return false;

   if(2 != set1.count(0))
      return false;
   return true;
}

}}}

template<class VoidAllocatorOrContainer>
struct GetSetContainer
{
   template<class ValueType>
   struct apply
   {
      typedef flat_set < ValueType
                       , std::less<ValueType>
                       , typename boost::container::dtl::container_or_allocator_rebind<VoidAllocatorOrContainer, ValueType>::type
                        > set_type;

      typedef flat_multiset < ValueType
                            , std::less<ValueType>
                            , typename boost::container::dtl::container_or_allocator_rebind<VoidAllocatorOrContainer, ValueType>::type
                            > multiset_type;
   };
};

template<typename FlatSetType>
bool test_support_for_initialization_list_for()
{
#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST)
   const std::initializer_list<int> il
      = {1, 2};

   const FlatSetType expected(il.begin(), il.end());
   {
      const FlatSetType sil = il;
      if (sil != expected)
         return false;

      const FlatSetType sil_ordered(ordered_unique_range, il);
      if(sil_ordered != expected)
         return false;

      FlatSetType sil_assign = {99};
      sil_assign = il;
      if(sil_assign != expected)
         return false;
   }
   {
      FlatSetType sil;
      sil.insert(il);
      if(sil != expected)
         return false;
   }
   return true;
#endif
   return true;
}

struct boost_container_flat_set;
struct boost_container_flat_multiset;

namespace boost {
namespace container {
namespace test {

template<>
struct alloc_propagate_base<boost_container_flat_set>
{
   template <class T, class Allocator>
   struct apply
   {
      typedef boost::container::flat_set<T, std::less<T>, Allocator> type;
   };
};

template<>
struct alloc_propagate_base<boost_container_flat_multiset>
{
   template <class T, class Allocator>
   struct apply
   {
      typedef boost::container::flat_multiset<T, std::less<T>, Allocator> type;
   };
};

}}}   //boost::container::test

int main()
{
   using namespace boost::container::test;

   //Allocator argument container
   {
      flat_set<int> set_((flat_set<int>::allocator_type()));
      flat_multiset<int> multiset_((flat_multiset<int>::allocator_type()));
   }
   //Now test move semantics
   {
      test_move<flat_set<recursive_flat_set> >();
      test_move<flat_multiset<recursive_flat_multiset> >();
   }
   //Now test nth/index_of
   {
      flat_set<int> set;
      flat_multiset<int> mset;

      set.insert(0);
      set.insert(1);
      set.insert(2);
      mset.insert(0);
      mset.insert(1);
      mset.insert(2);
      if(!boost::container::test::test_nth_index_of(set))
         return 1;
      if(!boost::container::test::test_nth_index_of(mset))
         return 1;
   }

   ////////////////////////////////////
   //    Ordered insertion test
   ////////////////////////////////////
   if(!flat_tree_ordered_insertion_test()){
      return 1;
   }

   ////////////////////////////////////
   //    Constructor Template Auto Deduction test
   ////////////////////////////////////
   if (!constructor_template_auto_deduction_test()) {
      return 1;
   }

   ////////////////////////////////////
   //    Extract/Adopt test
   ////////////////////////////////////
   if(!flat_tree_extract_adopt_test()){
      return 1;
   }

   if (!boost::container::test::instantiate_constructors<flat_set<int>, flat_multiset<int> >())
      return 1;

   if(!test_heterogeneous_lookups()){
      return 1;
   }

   if(!test_heterogeneous_lookup_by_partial_key()){
      return 1;
   }

   ////////////////////////////////////
   //    Testing allocator implementations
   ////////////////////////////////////
   {
      typedef std::set<int>                                          MyStdSet;
      typedef std::multiset<int>                                     MyStdMultiSet;

      if (0 != test::set_test
         < GetSetContainer<std::allocator<void> >::apply<int>::set_type
         , MyStdSet
         , GetSetContainer<std::allocator<void> >::apply<int>::multiset_type
         , MyStdMultiSet>()) {
         std::cout << "Error in set_test<std::allocator<void> >" << std::endl;
         return 1;
      }

      if (0 != test::set_test
         < GetSetContainer<new_allocator<void> >::apply<int>::set_type
         , MyStdSet
         , GetSetContainer<new_allocator<void> >::apply<int>::multiset_type
         , MyStdMultiSet>()) {
         std::cout << "Error in set_test<new_allocator<void> >" << std::endl;
         return 1;
      }

      if (0 != test::set_test
         < GetSetContainer<new_allocator<void> >::apply<test::movable_int>::set_type
         , MyStdSet
         , GetSetContainer<new_allocator<void> >::apply<test::movable_int>::multiset_type
         , MyStdMultiSet>()) {
         std::cout << "Error in set_test<new_allocator<void> >" << std::endl;
         return 1;
      }

      if (0 != test::set_test
         < GetSetContainer<new_allocator<void> >::apply<test::copyable_int>::set_type
         , MyStdSet
         , GetSetContainer<new_allocator<void> >::apply<test::copyable_int>::multiset_type
         , MyStdMultiSet>()) {
         std::cout << "Error in set_test<new_allocator<void> >" << std::endl;
         return 1;
      }

      if (0 != test::set_test
         < GetSetContainer<new_allocator<void> >::apply<test::movable_and_copyable_int>::set_type
         , MyStdSet
         , GetSetContainer<new_allocator<void> >::apply<test::movable_and_copyable_int>::multiset_type
         , MyStdMultiSet>()) {
         std::cout << "Error in set_test<new_allocator<void> >" << std::endl;
         return 1;
      }
   }

   ////////////////////////////////////
   //    Emplace testing
   ////////////////////////////////////
   const test::EmplaceOptions SetOptions = (test::EmplaceOptions)(test::EMPLACE_HINT | test::EMPLACE_ASSOC);

   if(!boost::container::test::test_emplace<flat_set<test::EmplaceInt>, SetOptions>())
      return 1;
   if(!boost::container::test::test_emplace<flat_multiset<test::EmplaceInt>, SetOptions>())
      return 1;

   if (!boost::container::test::test_set_methods_with_initializer_list_as_argument_for<flat_set<int> >())
      return 1;

   if (!boost::container::test::test_set_methods_with_initializer_list_as_argument_for<flat_multiset<int> >())
      return 1;

   ////////////////////////////////////
   //    Allocator propagation testing
   ////////////////////////////////////
   if(!boost::container::test::test_propagate_allocator<boost_container_flat_set>())
      return 1;

   if(!boost::container::test::test_propagate_allocator<boost_container_flat_multiset>())
      return 1;

   ////////////////////////////////////
   //    Iterator testing
   ////////////////////////////////////
   {
      typedef boost::container::flat_set<int> cont_int;
      cont_int a; a.insert(0); a.insert(1); a.insert(2);
      boost::intrusive::test::test_iterator_random< cont_int >(a);
      if(boost::report_errors() != 0) {
         return 1;
      }
   }
   {
      typedef boost::container::flat_multiset<int> cont_int;
      cont_int a; a.insert(0); a.insert(1); a.insert(2);
      boost::intrusive::test::test_iterator_random< cont_int >(a);
      if(boost::report_errors() != 0) {
         return 1;
      }
   }

   ////////////////////////////////////
   //    has_trivial_destructor_after_move testing
   ////////////////////////////////////
   {
      typedef boost::container::dtl::identity<int> key_of_value_t;
      // flat_set, default
      {
         typedef boost::container::flat_set<int> cont;
         typedef boost::container::dtl::flat_tree<int, key_of_value_t, std::less<int>, void> tree;
         BOOST_STATIC_ASSERT_MSG ( boost::has_trivial_destructor_after_move<cont>::value ==
                                   boost::has_trivial_destructor_after_move<tree>::value
                                 , "has_trivial_destructor_after_move(flat_set, default) test failed");
      }
      // flat_set, vector
      {
         typedef boost::container::vector<int> alloc_or_cont_t;
         typedef boost::container::flat_set<int, std::less<int>, alloc_or_cont_t> cont;
         typedef boost::container::dtl::flat_tree<int, key_of_value_t, std::less<int>, alloc_or_cont_t> tree;
         BOOST_STATIC_ASSERT_MSG ( boost::has_trivial_destructor_after_move<cont>::value ==
                                   boost::has_trivial_destructor_after_move<tree>::value
                                 , "has_trivial_destructor_after_move(flat_set, vector) test failed");
      }
      // flat_set, std::vector
      {
         typedef std::vector<int> alloc_or_cont_t;
         typedef boost::container::flat_set<int, std::less<int>, alloc_or_cont_t> cont;
         typedef boost::container::dtl::flat_tree<int, key_of_value_t, std::less<int>, alloc_or_cont_t> tree;
         BOOST_STATIC_ASSERT_MSG ( boost::has_trivial_destructor_after_move<cont>::value ==
                                   boost::has_trivial_destructor_after_move<tree>::value
                                 , "has_trivial_destructor_after_move(flat_set, std::vector) test failed");
      }
      // flat_multiset, default
      {
         typedef boost::container::flat_multiset<int> cont;
         typedef boost::container::dtl::flat_tree<int, key_of_value_t, std::less<int>, void> tree;
         BOOST_STATIC_ASSERT_MSG ( boost::has_trivial_destructor_after_move<cont>::value ==
                                   boost::has_trivial_destructor_after_move<tree>::value
                                 , "has_trivial_destructor_after_move(flat_multiset, default) test failed");
      }
      // flat_multiset, vector
      {
         typedef boost::container::vector<int> alloc_or_cont_t;
         typedef boost::container::flat_multiset<int, std::less<int>, alloc_or_cont_t> cont;
         typedef boost::container::dtl::flat_tree<int, key_of_value_t, std::less<int>, alloc_or_cont_t> tree;
         BOOST_STATIC_ASSERT_MSG ( boost::has_trivial_destructor_after_move<cont>::value ==
                                   boost::has_trivial_destructor_after_move<tree>::value
                                 , "has_trivial_destructor_after_move(flat_multiset, vector) test failed");
      }
      // flat_multiset, std::vector
      {
         typedef std::vector<int> alloc_or_cont_t;
         typedef boost::container::flat_multiset<int, std::less<int>, alloc_or_cont_t> cont;
         typedef boost::container::dtl::flat_tree<int, key_of_value_t, std::less<int>, alloc_or_cont_t> tree;
         BOOST_STATIC_ASSERT_MSG ( boost::has_trivial_destructor_after_move<cont>::value ==
                                   boost::has_trivial_destructor_after_move<tree>::value
                                 , "has_trivial_destructor_after_move(flat_multiset, std::vector) test failed");
      }
   }

   return 0;
}
