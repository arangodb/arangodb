/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Olaf Krzikalla 2004-2006.
// (C) Copyright Ion Gaztanaga  2006-2013.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/pointer_traits.hpp>
#include "itestvalue.hpp"
#include "bptr_value.hpp"
#include "smart_ptr.hpp"
#include "common_functors.hpp"
#include <vector>
#include <boost/detail/lightweight_test.hpp>
#include "test_macros.hpp"
#include "test_container.hpp"
#include <typeinfo>

using namespace boost::intrusive;

template<class VoidPointer>
struct hooks
{
   typedef slist_base_hook<void_pointer<VoidPointer> >                  base_hook_type;
   typedef slist_base_hook< link_mode<auto_unlink>
                         , void_pointer<VoidPointer>, tag<void> >       auto_base_hook_type;
   typedef slist_member_hook<void_pointer<VoidPointer>, tag<void> >     member_hook_type;
   typedef slist_member_hook< link_mode<auto_unlink>
                           , void_pointer<VoidPointer> >                auto_member_hook_type;
   typedef nonhook_node_member< slist_node_traits< VoidPointer >,
                                circular_slist_algorithms
                              > nonhook_node_member_type;
};

template < typename ListType, typename ValueContainer >
struct test_slist
{
   typedef ListType list_type;
   typedef typename list_type::value_traits value_traits;
   typedef typename value_traits::value_type value_type;
   typedef typename list_type::node_algorithms node_algorithms;

   static void test_all(ValueContainer&);
   static void test_front(ValueContainer&);
   static void test_back(ValueContainer&, detail::true_type);
   static void test_back(ValueContainer&, detail::false_type) {}
   static void test_sort(ValueContainer&);
   static void test_merge(ValueContainer&);
   static void test_remove_unique(ValueContainer&);
   static void test_insert(ValueContainer&);
   static void test_shift(ValueContainer&);
   static void test_swap(ValueContainer&);
   static void test_slow_insert(ValueContainer&);
   static void test_clone(ValueContainer&);
   static void test_container_from_end(ValueContainer&, detail::true_type);
   static void test_container_from_end(ValueContainer&, detail::false_type) {}
};

template < typename ListType, typename ValueContainer >
void test_slist< ListType, ValueContainer >
   ::test_all (ValueContainer& values)
{
   {
      list_type list(values.begin(), values.end());
      test::test_container(list);
      list.clear();
      list.insert(list.end(), values.begin(), values.end());
      test::test_sequence_container(list, values);
   }
   {
      list_type list(values.begin(), values.end());
      test::test_iterator_forward(list);
   }
   test_front(values);
   test_back(values, detail::bool_< list_type::cache_last >());
   test_sort(values);
   test_merge (values);
   test_remove_unique(values);
   test_insert(values);
   test_shift(values);
   test_slow_insert (values);
   test_swap(values);
   test_clone(values);
   test_container_from_end(values, detail::bool_< !list_type::linear && list_type::has_container_from_iterator >());
}

//test: push_front, pop_front, front, size, empty:
template < typename ListType, typename ValueContainer >
void test_slist< ListType, ValueContainer >
   ::test_front(ValueContainer& values)
{
   list_type testlist;
   BOOST_TEST (testlist.empty());

   testlist.push_front (values[0]);
   BOOST_TEST (testlist.size() == 1);
   BOOST_TEST (&testlist.front() == &values[0]);

   testlist.push_front (values[1]);
   BOOST_TEST (testlist.size() == 2);
   BOOST_TEST (&testlist.front() == &values[1]);

   testlist.pop_front();
   BOOST_TEST (testlist.size() == 1);
   BOOST_TEST (&testlist.front() == &values[0]);

   testlist.pop_front();
   BOOST_TEST (testlist.empty());
}

//test: push_front, pop_front, front, size, empty:
template < typename ListType, typename ValueContainer >
void test_slist< ListType, ValueContainer >
   ::test_back(ValueContainer& values, detail::true_type)
{
   list_type testlist;
   BOOST_TEST (testlist.empty());

   testlist.push_back (values[0]);
   BOOST_TEST (testlist.size() == 1);
   BOOST_TEST (&testlist.front() == &values[0]);
   BOOST_TEST (&testlist.back() == &values[0]);
   testlist.push_back(values[1]);
   BOOST_TEST(*testlist.previous(testlist.end()) == values[1]);
   BOOST_TEST (&testlist.front() == &values[0]);
   BOOST_TEST (&testlist.back() == &values[1]);
}

//test: merge due to error in merge implementation:
template < typename ListType, typename ValueContainer >
void test_slist< ListType, ValueContainer >
   ::test_merge (ValueContainer& values)
{
   list_type testlist1, testlist2;
   testlist1.push_front (values[0]);
   testlist2.push_front (values[4]);
   testlist2.push_front (values[3]);
   testlist2.push_front (values[2]);
   testlist1.merge (testlist2);

   int init_values [] = { 1, 3, 4, 5 };
   TEST_INTRUSIVE_SEQUENCE( init_values, testlist1.begin() );
}

//test: merge due to error in merge implementation:
template < typename ListType, typename ValueContainer >
void test_slist< ListType, ValueContainer >
   ::test_remove_unique (ValueContainer& values)
{
   {
      list_type list(values.begin(), values.end());
      list.remove_if(is_even());
      int init_values [] = { 1, 3, 5 };
      TEST_INTRUSIVE_SEQUENCE( init_values, list.begin() );
   }
   {
      list_type list(values.begin(), values.end());
      list.remove_if(is_odd());
      int init_values [] = { 2, 4 };
      TEST_INTRUSIVE_SEQUENCE( init_values, list.begin() );
   }
   {
      list_type list(values.begin(), values.end());
      list.remove_and_dispose_if(is_even(), test::empty_disposer());
      int init_values [] = { 1, 3, 5 };
      TEST_INTRUSIVE_SEQUENCE( init_values, list.begin() );
   }
   {
      list_type list(values.begin(), values.end());
      list.remove_and_dispose_if(is_odd(), test::empty_disposer());
      int init_values [] = { 2, 4 };
      TEST_INTRUSIVE_SEQUENCE( init_values, list.begin() );
   }
   {
      ValueContainer values2(values);
      list_type list(values.begin(), values.end());
      list.insert_after(list.before_begin(), values2.begin(), values2.end());
      list.sort();
      int init_values [] = { 1, 1, 2, 2, 3, 3, 4, 4, 5, 5 };
      TEST_INTRUSIVE_SEQUENCE( init_values, list.begin() );
      list.unique();
      int init_values2 [] = { 1, 2, 3, 4, 5 };
      TEST_INTRUSIVE_SEQUENCE( init_values2, list.begin() );
   }
}

//test: constructor, iterator, sort, reverse:
template < typename ListType, typename ValueContainer >
void test_slist< ListType, ValueContainer >
   ::test_sort(ValueContainer& values)
{
   list_type testlist (values.begin(), values.end());

   {  int init_values [] = { 1, 2, 3, 4, 5 };
      TEST_INTRUSIVE_SEQUENCE( init_values, testlist.begin() );  }

   testlist.sort (even_odd());
   {  int init_values [] = { 2, 4, 1, 3, 5 };
      TEST_INTRUSIVE_SEQUENCE( init_values, testlist.begin() );  }

   testlist.reverse();
   {  int init_values [] = { 5, 3, 1, 4, 2 };
      TEST_INTRUSIVE_SEQUENCE( init_values, testlist.begin() );  }
}

//test: assign, insert_after, const_iterator, erase_after, s_iterator_to, previous:
template < typename ListType, typename ValueContainer >
void test_slist< ListType, ValueContainer >
   ::test_insert(ValueContainer& values)
{
   list_type testlist;
   testlist.assign (values.begin() + 2, values.begin() + 5);

   const list_type& const_testlist = testlist;
   {  int init_values [] = { 3, 4, 5 };
      TEST_INTRUSIVE_SEQUENCE( init_values, const_testlist.begin() );  }

   typename list_type::iterator i = ++testlist.begin();
   BOOST_TEST (i->value_ == 4);

   testlist.insert_after (i, values[0]);
   {  int init_values [] = { 3, 4, 1, 5 };
      TEST_INTRUSIVE_SEQUENCE( init_values, const_testlist.begin() );  }

   i = testlist.iterator_to (values[4]);
   BOOST_TEST (&*i == &values[4]);
   i = list_type::s_iterator_to (values[4]);
   BOOST_TEST (&*i == &values[4]);

   typename list_type::const_iterator ic;
   ic = testlist.iterator_to (static_cast< typename list_type::const_reference >(values[4]));
   BOOST_TEST (&*ic == &values[4]);
   ic = list_type::s_iterator_to (static_cast< typename list_type::const_reference >(values[4]));
   BOOST_TEST (&*ic == &values[4]);

   i = testlist.previous (i);
   BOOST_TEST (&*i == &values[0]);

   testlist.erase_after (i);
   BOOST_TEST (&*i == &values[0]);
   {  int init_values [] = { 3, 4, 1 };
      TEST_INTRUSIVE_SEQUENCE( init_values, const_testlist.begin() );  }
}

//test: insert, const_iterator, erase, siterator_to:
template < typename ListType, typename ValueContainer >
void test_slist< ListType, ValueContainer >
   ::test_slow_insert (ValueContainer& values)
{
   list_type testlist;
   testlist.push_front (values[4]);
   testlist.insert (testlist.begin(), values.begin() + 2, values.begin() + 4);

   const list_type& const_testlist = testlist;
   {  int init_values [] = { 3, 4, 5 };
      TEST_INTRUSIVE_SEQUENCE( init_values, const_testlist.begin() );  }

   typename list_type::iterator i = ++testlist.begin();
   BOOST_TEST (i->value_ == 4);

   testlist.insert (i, values[0]);
   {  int init_values [] = { 3, 1, 4, 5 };
      TEST_INTRUSIVE_SEQUENCE( init_values, const_testlist.begin() );  }

   i = testlist.iterator_to (values[4]);
   BOOST_TEST (&*i == &values[4]);

   i = list_type::s_iterator_to (values[4]);
   BOOST_TEST (&*i == &values[4]);

   i = testlist.erase (i);
   BOOST_TEST (i == testlist.end());

   {  int init_values [] = { 3, 1, 4 };
      TEST_INTRUSIVE_SEQUENCE( init_values, const_testlist.begin() );  }

   testlist.erase (++testlist.begin(), testlist.end());
   BOOST_TEST (testlist.size() == 1);
   BOOST_TEST (testlist.begin()->value_ == 3);
}

template < typename ListType, typename ValueContainer >
void test_slist< ListType, ValueContainer >
   ::test_shift(ValueContainer& values)
{
   list_type testlist;
   const int num_values = (int)values.size();
   std::vector<int> expected_values(num_values);

   //Shift forward all possible positions 3 times
   for(int s = 1; s <= num_values; ++s){
      expected_values.resize(s);
      for(int i = 0; i < s*3; ++i){
         testlist.insert_after(testlist.before_begin(), values.begin(), values.begin() + s);
         testlist.shift_forward(i);
         for(int j = 0; j < s; ++j){
            expected_values[(j + s - i%s) % s] = (j + 1);
         }

         TEST_INTRUSIVE_SEQUENCE_EXPECTED(expected_values, testlist.begin())
         testlist.clear();
      }

      //Shift backwards all possible positions
      for(int i = 0; i < s*3; ++i){
         testlist.insert_after(testlist.before_begin(), values.begin(), values.begin() + s);
         testlist.shift_backwards(i);
         for(int j = 0; j < s; ++j){
            expected_values[(j + i) % s] = (j + 1);
         }

         TEST_INTRUSIVE_SEQUENCE_EXPECTED(expected_values, testlist.begin())
         testlist.clear();
      }
   }
}

//test: insert_after (seq-version), swap, splice_after:
template < typename ListType, typename ValueContainer >
void test_slist< ListType, ValueContainer >
   ::test_swap(ValueContainer& values)
{
   {
      list_type testlist1 (values.begin(), values.begin() + 2);
      list_type testlist2;
      testlist2.insert_after (testlist2.before_begin(), values.begin() + 2, values.begin() + 5);
      testlist1.swap(testlist2);
      {  int init_values [] = { 3, 4, 5 };
         TEST_INTRUSIVE_SEQUENCE( init_values, testlist1.begin() );  }
      {  int init_values [] = { 1, 2 };
         TEST_INTRUSIVE_SEQUENCE( init_values, testlist2.begin() );  }
         testlist2.splice_after (testlist2.begin(), testlist1);
      {  int init_values [] = { 1, 3, 4, 5, 2 };
         TEST_INTRUSIVE_SEQUENCE( init_values, testlist2.begin() );  }
      BOOST_TEST (testlist1.empty());

      testlist1.splice_after (testlist1.before_begin(), testlist2, ++testlist2.begin());
      {  int init_values [] = { 4 };
         TEST_INTRUSIVE_SEQUENCE( init_values, testlist1.begin() );  }
      {  int init_values [] = { 1, 3, 5, 2 };
         TEST_INTRUSIVE_SEQUENCE( init_values, testlist2.begin() );  }

      testlist1.splice_after (testlist1.begin(), testlist2,
                              testlist2.before_begin(), ++++testlist2.begin());
      {  int init_values [] = { 4, 1, 3, 5 };
         TEST_INTRUSIVE_SEQUENCE( init_values, testlist1.begin() );  }
      {  int init_values [] = { 2 };
         TEST_INTRUSIVE_SEQUENCE( init_values, testlist2.begin() );  }
   }
   {  //Now test swap when testlist2 is empty
      list_type testlist1 (values.begin(), values.begin() + 2);
      list_type testlist2;
      testlist1.swap(testlist2);
      BOOST_TEST (testlist1.empty());
      {  int init_values [] = { 1, 2 };
         TEST_INTRUSIVE_SEQUENCE( init_values, testlist2.begin() );  }
   }
   {  //Now test swap when testlist1 is empty
      list_type testlist2 (values.begin(), values.begin() + 2);
      list_type testlist1;
      testlist1.swap(testlist2);
      BOOST_TEST (testlist2.empty());
      {  int init_values [] = { 1, 2 };
         TEST_INTRUSIVE_SEQUENCE( init_values, testlist1.begin() );  }
   }
   {  //Now test when both are empty
      list_type testlist1, testlist2;
      testlist2.swap(testlist1);
      BOOST_TEST (testlist1.empty() && testlist2.empty());
   }

   if(!list_type::linear)
   {
      list_type testlist1 (values.begin(), values.begin() + 2);
      list_type testlist2 (values.begin() + 3, values.begin() + 5);

      swap_nodes< node_algorithms >(values[0], values[2]);
      {  int init_values [] = { 3, 2 };
         TEST_INTRUSIVE_SEQUENCE( init_values, testlist1.begin() );  }

      swap_nodes< node_algorithms >(values[2], values[4]);
      {  int init_values [] = { 5, 2 };
         TEST_INTRUSIVE_SEQUENCE( init_values, testlist1.begin() );  }
      {  int init_values [] = { 4, 3 };
         TEST_INTRUSIVE_SEQUENCE( init_values, testlist2.begin() );  }
   }
   if(!list_type::linear)
   {
      list_type testlist1 (values.begin(), values.begin()+1);
      if(testlist1.size() != 1){
         abort();
      }
      {  int init_values [] = { 1 };
         TEST_INTRUSIVE_SEQUENCE( init_values, testlist1.begin() );  }

      swap_nodes< node_algorithms >(values[1], values[2]);

      BOOST_TEST(testlist1.size() == 1);
      BOOST_TEST(!(&values[1])->is_linked());
      BOOST_TEST(!(&values[2])->is_linked());
      {  int init_values [] = { 1 };
         TEST_INTRUSIVE_SEQUENCE( init_values, testlist1.begin() );  }

      swap_nodes< node_algorithms >(values[0], values[2]);
      BOOST_TEST(testlist1.size() == 1);
      BOOST_TEST((&values[2])->is_linked());
      BOOST_TEST(!(&values[0])->is_linked());
      {  int init_values [] = { 3 };
         TEST_INTRUSIVE_SEQUENCE( init_values, testlist1.begin() );  }

      swap_nodes< node_algorithms >(values[0], values[2]);
      BOOST_TEST(testlist1.size() == 1);
      BOOST_TEST(!(&values[2])->is_linked());
      BOOST_TEST((&values[0])->is_linked());
      {  int init_values [] = { 1 };
         TEST_INTRUSIVE_SEQUENCE( init_values, testlist1.begin() );  }
   }
}

template < typename ListType, typename ValueContainer >
void test_slist< ListType, ValueContainer >
   ::test_clone(ValueContainer& values)
{
      list_type testlist1 (values.begin(), values.begin() + values.size());
      list_type testlist2;

      testlist2.clone_from(testlist1, test::new_cloner<value_type>(), test::delete_disposer<value_type>());
      BOOST_TEST (testlist2 == testlist1);
      testlist2.clear_and_dispose(test::delete_disposer<value_type>());
      BOOST_TEST (testlist2.empty());
}

template < typename ListType, typename ValueContainer >
void test_slist< ListType, ValueContainer >
   ::test_container_from_end(ValueContainer& values, detail::true_type)
{
   list_type testlist1 (values.begin(), values.begin() + values.size());
   BOOST_TEST (testlist1 == list_type::container_from_end_iterator(testlist1.end()));
   BOOST_TEST (testlist1 == list_type::container_from_end_iterator(testlist1.cend()));
}

template < typename ValueTraits, bool ConstantTimeSize, bool Linear, bool CacheLast, bool Default_Holder, typename ValueContainer >
struct make_and_test_slist
   : test_slist< slist< typename ValueTraits::value_type,
                        value_traits< ValueTraits >,
                        size_type< std::size_t >,
                        constant_time_size< ConstantTimeSize >,
                        linear<Linear>,
                        cache_last<CacheLast>
                      >,
                  ValueContainer
                >
{};

template < typename ValueTraits, bool ConstantTimeSize, bool Linear, bool CacheLast, typename ValueContainer >
struct make_and_test_slist< ValueTraits, ConstantTimeSize, Linear, CacheLast, false, ValueContainer >
   : test_slist< slist< typename ValueTraits::value_type,
                        value_traits< ValueTraits >,
                        size_type< std::size_t >,
                        constant_time_size< ConstantTimeSize >,
                        linear<Linear>,
                        cache_last<CacheLast>,
                        header_holder_type< heap_node_holder< typename ValueTraits::pointer > >
                      >,
                  ValueContainer
                >
{};

template<class VoidPointer, bool constant_time_size, bool Default_Holder>
class test_main_template
{
   public:
   int operator()()
   {
      typedef testvalue< hooks<VoidPointer> > value_type;
      std::vector< value_type > data (5);
      for (int i = 0; i < 5; ++i)
         data[i].value_ = i + 1;

      make_and_test_slist < typename detail::get_base_value_traits
                  < value_type
                  , typename hooks<VoidPointer>::base_hook_type
                  >::type
                 , constant_time_size
                 , false
                 , false
                 , Default_Holder
                 , std::vector< value_type >
                >::test_all(data);
      make_and_test_slist < nonhook_node_member_value_traits< value_type,
                                                     typename hooks<VoidPointer>::nonhook_node_member_type,
                                                     &value_type::nhn_member_,
                                                     safe_link
                                                   >
                 , constant_time_size
                 , false
                 , false
                 , Default_Holder
                 , std::vector< value_type >
                >::test_all(data);

      //Now linear slists
      make_and_test_slist < typename detail::get_member_value_traits
                  < member_hook< value_type
                               , typename hooks<VoidPointer>::member_hook_type
                               , &value_type::node_
                               >
                  >::type
                 , constant_time_size
                 , true
                 , false
                 , Default_Holder
                 , std::vector< value_type >
                >::test_all(data);

      //Now the same but caching the last node
      make_and_test_slist < typename detail::get_base_value_traits
                  < value_type
                  , typename hooks<VoidPointer>::base_hook_type
                  >::type
                 , constant_time_size
                 , false
                 , true
                 , Default_Holder
                 , std::vector< value_type >
                >::test_all(data);

      //Now linear slists
      make_and_test_slist < typename detail::get_base_value_traits
                  < value_type
                  , typename hooks<VoidPointer>::base_hook_type
                  >::type
                 , constant_time_size
                 , true
                 , true
                 , Default_Holder
                 , std::vector< value_type >
                >::test_all(data);
      return 0;
   }
};

template<class VoidPointer, bool Default_Holder>
class test_main_template<VoidPointer, false, Default_Holder>
{
   public:
   int operator()()
   {
      typedef testvalue< hooks<VoidPointer> > value_type;
      std::vector< value_type > data (5);
      for (int i = 0; i < 5; ++i)
         data[i].value_ = i + 1;

      make_and_test_slist < typename detail::get_base_value_traits
                  < value_type
                  , typename hooks<VoidPointer>::auto_base_hook_type
                  >::type
                 , false
                 , false
                 , false
                 , Default_Holder
                 , std::vector< value_type >
                >::test_all(data);

      make_and_test_slist < nonhook_node_member_value_traits< value_type,
                                                     typename hooks<VoidPointer>::nonhook_node_member_type,
                                                     &value_type::nhn_member_,
                                                     safe_link
                                                   >
                 , false
                 , false
                 , true
                 , Default_Holder
                 , std::vector< value_type >
                >::test_all(data);

      make_and_test_slist < typename detail::get_member_value_traits
                  < member_hook< value_type
                               , typename hooks<VoidPointer>::member_hook_type
                               , &value_type::node_
                               >
                  >::type
                 , false
                 , true
                 , false
                 , Default_Holder
                 , std::vector< value_type >
                >::test_all(data);

      make_and_test_slist < typename detail::get_base_value_traits
                  < value_type
                  , typename hooks<VoidPointer>::base_hook_type
                  >::type
                 , false
                 , true
                 , true
                 , Default_Holder
                 , std::vector< value_type >
                >::test_all(data);

      return 0;
   }
};

template < bool ConstantTimeSize >
struct test_main_template_bptr
{
   int operator()()
   {
      typedef BPtr_Value value_type;
      typedef BPtr_Value_Traits< List_BPtr_Node_Traits > list_value_traits;
      typedef typename list_value_traits::node_ptr node_ptr;
      typedef bounded_allocator< value_type > allocator_type;

      bounded_allocator_scope<allocator_type> bounded_scope; (void)bounded_scope;
      allocator_type allocator;

      {
          bounded_reference_cont< value_type > ref_cont;
          for (int i = 0; i < 5; ++i)
          {
              node_ptr tmp = allocator.allocate(1);
              new (tmp.raw()) value_type(i + 1);
              ref_cont.push_back(*tmp);
          }

          test_slist < slist < value_type,
                               value_traits< list_value_traits >,
                               size_type< std::size_t >,
                               constant_time_size< ConstantTimeSize >,
                               header_holder_type< bounded_pointer_holder< value_type > >
                             >,
                       bounded_reference_cont< value_type >
          >::test_all(ref_cont);
      }
      return 0;
   }
};

int main(int, char* [])
{
   // test (plain/smart pointers) x (nonconst/const size) x (void node allocator)
   test_main_template<void*, false, true>()();
   test_main_template<boost::intrusive::smart_ptr<void>, false, true>()();
   test_main_template<void*, true, true>()();
   test_main_template<boost::intrusive::smart_ptr<void>, true, true>()();
   // test (bounded pointers) x ((nonconst/const size) x (special node allocator)
   test_main_template_bptr< true >()();
   test_main_template_bptr< false >()();


   return boost::report_errors();
}
