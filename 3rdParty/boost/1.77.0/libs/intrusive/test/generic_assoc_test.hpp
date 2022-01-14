/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Olaf Krzikalla 2004-2006.
// (C) Copyright Ion Gaztanaga  2006-2021.
// (C) Copyright Daniel Steck   2021
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////
#include <boost/container/vector.hpp> //vector
#include <boost/intrusive/detail/config_begin.hpp>
#include "common_functors.hpp"
#include <boost/intrusive/options.hpp>
#include <boost/intrusive/detail/mpl.hpp>
#include <boost/core/lightweight_test.hpp>
#include "test_macros.hpp"
#include "test_container.hpp"
#include <boost/next_prior.hpp>

namespace boost{
namespace intrusive{
namespace test{

BOOST_INTRUSIVE_HAS_MEMBER_FUNC_CALLED(has_splay, splay)

BOOST_INTRUSIVE_HAS_MEMBER_FUNC_CALLED(has_rebalance, rebalance)

BOOST_INTRUSIVE_HAS_MEMBER_FUNC_CALLED(has_insert_before, insert_before)

BOOST_INTRUSIVE_HAS_MEMBER_FUNC_CALLED(is_treap, priority_comp)

template<class ContainerDefiner>
struct test_generic_assoc
{
   typedef typename ContainerDefiner::value_cont_type    value_cont_type;

   static void test_all(value_cont_type&);
   static void test_root(value_cont_type&);
   static void test_clone(value_cont_type&);
   static void test_insert_erase_burst();
   static void test_swap_nodes();
   template <class Assoc>
      static void test_perfect_binary_tree_of_height_2(value_cont_type &values, Assoc &assoc);
   static void test_container_from_end(value_cont_type&, detail::true_type);
   static void test_container_from_end(value_cont_type&, detail::false_type) {}
   static void test_splay_up(value_cont_type&, detail::true_type);
   static void test_splay_up(value_cont_type&, detail::false_type) {}
   static void test_splay_down(value_cont_type&, detail::true_type);
   static void test_splay_down(value_cont_type&, detail::false_type) {}
   static void test_rebalance(value_cont_type&, detail::true_type);
   static void test_rebalance(value_cont_type&, detail::false_type) {}
   static void test_insert_before(value_cont_type&, detail::true_type);
   static void test_insert_before(value_cont_type&, detail::false_type) {}
   static void test_container_from_iterator(value_cont_type&, detail::true_type);
   static void test_container_from_iterator(value_cont_type&, detail::false_type) {}
};

template<class ContainerDefiner>
void test_generic_assoc<ContainerDefiner>::
   test_container_from_iterator(value_cont_type& values, detail::true_type)
{
   typedef typename ContainerDefiner::template container
      <>::type assoc_type;
   assoc_type testset(values.begin(), values.end());
   typedef typename assoc_type::iterator        it_type;
   typedef typename assoc_type::const_iterator  cit_type;
   typedef typename assoc_type::size_type       sz_type;
   sz_type sz = testset.size();
   for(it_type b(testset.begin()), e(testset.end()); b != e; ++b)
   {
      assoc_type &s = assoc_type::container_from_iterator(b);
      const assoc_type &cs = assoc_type::container_from_iterator(cit_type(b));
      BOOST_TEST(&s == &cs);
      BOOST_TEST(&s == &testset);
      s.erase(b);
      BOOST_TEST(testset.size() == (sz-1));
      s.insert(*b);
      BOOST_TEST(testset.size() == sz);
   }
}

template<class ContainerDefiner>
void test_generic_assoc<ContainerDefiner>::test_insert_erase_burst()
{
   //value_cont_type values;
   const std::size_t MaxValues = 200;
   value_cont_type values(MaxValues);
   for(std::size_t i = 0; i != MaxValues; ++i){
      (&values[i])->value_ = i;
   }

   typedef typename ContainerDefiner::template container
      <>::type  assoc_type;
   typedef typename assoc_type::iterator iterator;

   {  //Ordered insertion + erasure
      assoc_type testset (values.begin(), values.begin() + values.size());
      TEST_INTRUSIVE_SEQUENCE_EXPECTED(testset, testset.begin());
      testset.check();
      iterator it(testset.begin()), itend(testset.end());
      for(std::size_t i = 0; it != itend; ++i){
         BOOST_TEST(&*it == &values[i]);
         it = testset.erase(it);
         testset.check();
      }
      BOOST_TEST(testset.empty());
   }

   {  //Now random insertions + erasure
      assoc_type testset;
      typedef typename value_cont_type::iterator vec_iterator;
      boost::container::vector<vec_iterator> it_vector;
      //Random insertion
      for(vec_iterator it(values.begin()), itend(values.end())
         ; it != itend
         ; ++it){
         it_vector.push_back(it);
      }
      for(std::size_t i = 0; i != MaxValues; ++i){
         testset.insert(*it_vector[i]);
         testset.check();
      }
      TEST_INTRUSIVE_SEQUENCE_EXPECTED(testset, testset.begin());
      //Random erasure
      random_shuffle(it_vector.begin(), it_vector.end());
      for(std::size_t i = 0; i != MaxValues; ++i){
         testset.erase(testset.iterator_to(*it_vector[i]));
         testset.check();
      }
      BOOST_TEST(testset.empty());
   }
}

// Perfect binary tree of height 2
//            3                  |
//          /   \                |
//         1     5               |
//        / \   / \              |
//       0   2 4   6             |
template<class ContainerDefiner>
template <class Assoc>
void test_generic_assoc<ContainerDefiner>::test_perfect_binary_tree_of_height_2
   (value_cont_type &values, Assoc &assoc)
{
   //value_cont_type values;
   const std::size_t MaxValues = 7;
   BOOST_TEST(values.size() == MaxValues);
   for(std::size_t i = 0; i != MaxValues; ++i){
      (&values[i])->value_ = i;
   }

   typedef typename Assoc::iterator iterator;

   BOOST_TEST( assoc.empty() );
   assoc.clear();

   const iterator it3 = assoc.insert_before(assoc.end(), values[3]);
   const iterator it1 = assoc.insert_before(it3, values[1]);
   const iterator it5 = assoc.insert_before(assoc.end(), values[5]);
   assoc.insert_before(it1, values[0]);
   assoc.insert_before(it3, values[2]);
   assoc.insert_before(it5, values[4]);
   assoc.insert_before(assoc.end(), values[6]);
}


template<class ContainerDefiner>
void test_generic_assoc<ContainerDefiner>::test_swap_nodes()
{
// Perfect binary tree of height 2
//            3                  |
//          /   \                |
//         1     5               |
//        / \   / \              |
//       0   2 4   6             |

   typedef typename ContainerDefiner::template container
      <>::type  assoc_type;
   typedef typename assoc_type::value_traits value_traits_t;
   typedef typename assoc_type::node_algorithms node_algorithms_t;
   const std::size_t MaxValues = 7;

   {  //Unrelated swap
      value_cont_type values(MaxValues);
      assoc_type testset;
      test_perfect_binary_tree_of_height_2(values, testset);

      node_algorithms_t::swap_nodes
         ( value_traits_t::to_node_ptr(values[0])
         , value_traits_t::to_node_ptr(values[4])
         );

      BOOST_TEST( (&*boost::next(testset.begin(), 0) == &values[4]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 1) == &values[1]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 2) == &values[2]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 3) == &values[3]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 4) == &values[0]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 5) == &values[5]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 6) == &values[6]) );

      node_algorithms_t::swap_nodes
         ( value_traits_t::to_node_ptr(values[4])
         , value_traits_t::to_node_ptr(values[0])
         );

      BOOST_TEST( (&*boost::next(testset.begin(), 0) == &values[0]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 1) == &values[1]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 2) == &values[2]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 3) == &values[3]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 4) == &values[4]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 5) == &values[5]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 6) == &values[6]) );
      
      testset.check();
   }

   {  //sibling leaf nodes
      value_cont_type values(MaxValues);
      assoc_type testset;
      test_perfect_binary_tree_of_height_2(values, testset);

      node_algorithms_t::swap_nodes
         ( value_traits_t::to_node_ptr(values[0])
         , value_traits_t::to_node_ptr(values[2])
         );

      BOOST_TEST( (&*boost::next(testset.begin(), 0) == &values[2]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 1) == &values[1]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 2) == &values[0]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 3) == &values[3]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 4) == &values[4]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 5) == &values[5]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 6) == &values[6]) );

      node_algorithms_t::swap_nodes
         ( value_traits_t::to_node_ptr(values[0])
         , value_traits_t::to_node_ptr(values[2])
         );

      BOOST_TEST( (&*boost::next(testset.begin(), 0) == &values[0]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 1) == &values[1]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 2) == &values[2]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 3) == &values[3]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 4) == &values[4]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 5) == &values[5]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 6) == &values[6]) );
      
      testset.check();
   }

   {  //sibling nodes
      value_cont_type values(MaxValues);
      assoc_type testset;
      test_perfect_binary_tree_of_height_2(values, testset);

      node_algorithms_t::swap_nodes
         ( value_traits_t::to_node_ptr(values[1])
         , value_traits_t::to_node_ptr(values[5])
         );

      BOOST_TEST( (&*boost::next(testset.begin(), 0) == &values[0]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 1) == &values[5]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 2) == &values[2]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 3) == &values[3]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 4) == &values[4]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 5) == &values[1]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 6) == &values[6]) );

      node_algorithms_t::swap_nodes
         ( value_traits_t::to_node_ptr(values[1])
         , value_traits_t::to_node_ptr(values[5])
         );

      BOOST_TEST( (&*boost::next(testset.begin(), 0) == &values[0]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 1) == &values[1]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 2) == &values[2]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 3) == &values[3]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 4) == &values[4]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 5) == &values[5]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 6) == &values[6]) );
      
      testset.check();
   }

   {  //left child
      value_cont_type values(MaxValues);
      assoc_type testset;
      test_perfect_binary_tree_of_height_2(values, testset);

      node_algorithms_t::swap_nodes
         ( value_traits_t::to_node_ptr(values[0])
         , value_traits_t::to_node_ptr(values[1])
         );

      BOOST_TEST( (&*boost::next(testset.begin(), 0) == &values[1]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 1) == &values[0]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 2) == &values[2]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 3) == &values[3]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 4) == &values[4]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 5) == &values[5]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 6) == &values[6]) );

      node_algorithms_t::swap_nodes
         ( value_traits_t::to_node_ptr(values[0])
         , value_traits_t::to_node_ptr(values[1])
         );

      BOOST_TEST( (&*boost::next(testset.begin(), 0) == &values[0]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 1) == &values[1]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 2) == &values[2]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 3) == &values[3]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 4) == &values[4]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 5) == &values[5]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 6) == &values[6]) );
      
      testset.check();
   }

   {  //right child
      value_cont_type values(MaxValues);
      assoc_type testset;
      test_perfect_binary_tree_of_height_2(values, testset);

      node_algorithms_t::swap_nodes
         ( value_traits_t::to_node_ptr(values[1])
         , value_traits_t::to_node_ptr(values[2])
         );

      BOOST_TEST( (&*boost::next(testset.begin(), 0) == &values[0]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 1) == &values[2]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 2) == &values[1]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 3) == &values[3]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 4) == &values[4]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 5) == &values[5]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 6) == &values[6]) );

      node_algorithms_t::swap_nodes
         ( value_traits_t::to_node_ptr(values[1])
         , value_traits_t::to_node_ptr(values[2])
         );

      BOOST_TEST( (&*boost::next(testset.begin(), 0) == &values[0]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 1) == &values[1]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 2) == &values[2]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 3) == &values[3]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 4) == &values[4]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 5) == &values[5]) );
      BOOST_TEST( (&*boost::next(testset.begin(), 6) == &values[6]) );
      
      testset.check();
   }
}

template<class ContainerDefiner>
void test_generic_assoc<ContainerDefiner>::test_all(value_cont_type& values)
{
   typedef typename ContainerDefiner::template container
      <>::type assoc_type;
   test_root(values);
   test_clone(values);
   test_container_from_end(values, detail::bool_< assoc_type::has_container_from_iterator >());
   test_splay_up(values, detail::bool_< has_splay< assoc_type >::value >());
   test_splay_down(values, detail::bool_< has_splay< assoc_type >::value >());
   test_rebalance(values, detail::bool_< has_rebalance< assoc_type >::value >());
   test_insert_before(values, detail::bool_< has_insert_before< assoc_type >::value >());
   test_insert_erase_burst();
   test_swap_nodes();
   test_container_from_iterator(values, detail::bool_< assoc_type::has_container_from_iterator >());
}

template<class ContainerDefiner>
void test_generic_assoc<ContainerDefiner>::test_root(value_cont_type& values)
{
   typedef typename ContainerDefiner::template container<>::type  assoc_type;
   typedef typename assoc_type::iterator                          iterator;
   typedef typename assoc_type::const_iterator                    const_iterator;

   assoc_type testset1;
   const assoc_type &ctestset1 = testset1;;

   BOOST_TEST( testset1.root()  ==  testset1.end());
   BOOST_TEST(ctestset1.root()  == ctestset1.cend());
   BOOST_TEST( testset1.croot() == ctestset1.cend());


   testset1.insert(values.begin(), values.begin() + values.size());

   iterator i = testset1.root();
   iterator i2(i);
   BOOST_TEST( i.go_parent().go_parent() == i2);

   const_iterator ci = ctestset1.root();
   const_iterator ci2(ci);
   BOOST_TEST( ci.go_parent().go_parent() == ci2);

   ci = testset1.croot();
   ci2 = ci;
   BOOST_TEST( ci.go_parent().go_parent() == ci2);
}

template<class ContainerDefiner>
void test_generic_assoc<ContainerDefiner>::test_clone(value_cont_type& values)
{
   {
      typedef typename ContainerDefiner::template container
         <>::type assoc_type;
      typedef typename assoc_type::value_type value_type;
      typedef typename assoc_type::size_type size_type;

      assoc_type testset1 (values.begin(), values.begin() + values.size());
      assoc_type testset2;


      size_type const testset1_oldsize = testset1.size();
      testset2.clone_from(testset1, test::new_cloner<value_type>(), test::delete_disposer<value_type>());
      BOOST_TEST (testset1.size() == testset1_oldsize);
      BOOST_TEST (testset2 == testset1);
      testset2.clear_and_dispose(test::delete_disposer<value_type>());
      BOOST_TEST (testset2.empty());

      //Now test move clone
      testset2.clone_from(boost::move(testset1), test::new_nonconst_cloner<value_type>(), test::delete_disposer<value_type>());
      BOOST_TEST (testset2 == testset1);
      testset2.clear_and_dispose(test::delete_disposer<value_type>());
      BOOST_TEST (testset2.empty());
   }
}

template<class ContainerDefiner>
void test_generic_assoc<ContainerDefiner>
   ::test_container_from_end(value_cont_type& values, detail::true_type)
{
   typedef typename ContainerDefiner::template container
      <>::type assoc_type;
   assoc_type testset (values.begin(), values.begin() + values.size());
   BOOST_TEST (testset == assoc_type::container_from_end_iterator(testset.end()));
   BOOST_TEST (testset == assoc_type::container_from_end_iterator(testset.cend()));
}

template<class ContainerDefiner>
void test_generic_assoc<ContainerDefiner>::test_splay_up
(value_cont_type& values, detail::true_type)
{
   typedef typename ContainerDefiner::template container
      <>::type assoc_type;

   typedef typename assoc_type::iterator iterator;
   typedef value_cont_type orig_set_t;
   std::size_t num_values;
   orig_set_t original_testset;
   {
      assoc_type testset (values.begin(), values.end());
      num_values = testset.size();
      original_testset = value_cont_type(testset.begin(), testset.end());
   }

   for(std::size_t i = 0; i != num_values; ++i){
      assoc_type testset (values.begin(), values.end());
      {
         iterator it = testset.begin();
         for(std::size_t j = 0; j != i; ++j, ++it){}
         testset.splay_up(it);
      }
      BOOST_TEST (testset.size() == num_values);
      iterator it = testset.begin();
      for( typename orig_set_t::const_iterator origit    = original_testset.begin()
         , origitend = original_testset.end()
         ; origit != origitend
         ; ++origit, ++it){
         BOOST_TEST(*origit == *it);
      }
   }
}

template<class ContainerDefiner>
void test_generic_assoc<ContainerDefiner>::test_splay_down
(value_cont_type& values, detail::true_type)
{
   typedef typename ContainerDefiner::template container
      <>::type assoc_type;

   typedef typename assoc_type::iterator iterator;
   typedef value_cont_type orig_set_t;
   std::size_t num_values;
   orig_set_t original_testset;
   {
      assoc_type testset (values.begin(), values.end());
      num_values = testset.size();
      original_testset = value_cont_type(testset.begin(), testset.end());
   }

   for(std::size_t i = 0; i != num_values; ++i){
      assoc_type testset (values.begin(), values.end());
      BOOST_TEST(testset.size() == num_values);
      {
         iterator it = testset.begin();
         for(std::size_t j = 0; j != i; ++j, ++it){}
         BOOST_TEST(*it == *testset.splay_down(*it));
      }
      BOOST_TEST (testset.size() == num_values);
      iterator it = testset.begin();
      for( typename orig_set_t::const_iterator origit    = original_testset.begin()
         , origitend = original_testset.end()
         ; origit != origitend
         ; ++origit, ++it){
         BOOST_TEST(*origit == *it);
      }
   }
}

template<class ContainerDefiner>
void test_generic_assoc<ContainerDefiner>::test_rebalance
(value_cont_type& values, detail::true_type)
{
   typedef typename ContainerDefiner::template container
      <>::type assoc_type;
   typedef value_cont_type orig_set_t;
   orig_set_t original_testset;
   {
      assoc_type testset (values.begin(), values.end());
      original_testset.assign(testset.begin(), testset.end());
   }
   {
      assoc_type testset(values.begin(), values.end());
      testset.rebalance();
      TEST_INTRUSIVE_SEQUENCE_EXPECTED(original_testset, testset.begin());
   }

   {
      std::size_t numdata;
      {
         assoc_type testset(values.begin(), values.end());
         numdata = testset.size();
      }

      for(int i = 0; i != (int)numdata; ++i){
         assoc_type testset(values.begin(), values.end());
         typename assoc_type::iterator it = testset.begin();
         for(int j = 0; j  != i; ++j)  ++it;
         testset.rebalance_subtree(it);
         TEST_INTRUSIVE_SEQUENCE_EXPECTED(original_testset, testset.begin());
      }
   }
}

template<class ContainerDefiner>
void test_generic_assoc<ContainerDefiner>::test_insert_before
(value_cont_type& values, detail::true_type)
{
   typedef typename ContainerDefiner::template container
      <>::type assoc_type;
   {
      assoc_type testset;
      typedef typename value_cont_type::iterator vec_iterator;
      for(vec_iterator it(values.begin()), itend(values.end())
         ; it != itend
         ; ++it){
         testset.push_back(*it);
      }
      BOOST_TEST(testset.size() == values.size());
      TEST_INTRUSIVE_SEQUENCE_EXPECTED(values, testset.begin());
   }
   {
      assoc_type testset;
      typedef typename value_cont_type::iterator vec_iterator;

      for(vec_iterator it(--values.end()); true; --it){
         testset.push_front(*it);
       if(it == values.begin()){
            break;
       }
      }
      BOOST_TEST(testset.size() == values.size());
      TEST_INTRUSIVE_SEQUENCE_EXPECTED(values, testset.begin());
   }
   {
      assoc_type testset;
      typedef typename value_cont_type::iterator vec_iterator;
      typename assoc_type::iterator it_pos =
         testset.insert_before(testset.end(), *values.rbegin());
      testset.insert_before(testset.begin(), *values.begin());
      for(vec_iterator it(++values.begin()), itend(--values.end())
         ; it != itend
         ; ++it){
         testset.insert_before(it_pos, *it);
      }
      BOOST_TEST(testset.size() == values.size());
      TEST_INTRUSIVE_SEQUENCE_EXPECTED(values, testset.begin());
   }
}

}}}   //namespace boost::intrusive::test

#include <boost/intrusive/detail/config_end.hpp>
