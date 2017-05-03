/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2007-2013
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_TEST_CONTAINER_HPP
#define BOOST_INTRUSIVE_TEST_CONTAINER_HPP

#include <boost/detail/lightweight_test.hpp>
#include <boost/intrusive/detail/mpl.hpp>
#include <boost/intrusive/detail/simple_disposers.hpp>
#include <boost/intrusive/detail/iterator.hpp>
#include <boost/move/utility_core.hpp>
#include <boost/intrusive/detail/mpl.hpp>
#include <boost/static_assert.hpp>
#include "iterator_test.hpp"

namespace boost {
namespace intrusive {
namespace test {

BOOST_INTRUSIVE_HAS_MEMBER_FUNC_CALLED(is_unordered, hasher)

template<class Container>
struct test_container_typedefs
{
   typedef typename Container::value_type       value_type;
   typedef typename Container::iterator         iterator;
   typedef typename Container::const_iterator   const_iterator;
   typedef typename Container::reference        reference;
   typedef typename Container::const_reference  const_reference;
   typedef typename Container::pointer          pointer;
   typedef typename Container::const_pointer    const_pointer;
   typedef typename Container::difference_type  difference_type;
   typedef typename Container::size_type        size_type;
   typedef typename Container::value_traits     value_traits;
};

template< class Container >
void test_container( Container & c )
{
   typedef typename Container::const_iterator   const_iterator;
   typedef typename Container::iterator         iterator;
   typedef typename Container::size_type        size_type;

   {test_container_typedefs<Container> dummy;  (void)dummy;}
   const size_type num_elem = c.size();
   BOOST_TEST( c.empty() == (num_elem == 0) );
   {
      iterator it(c.begin()), itend(c.end());
      size_type i;
      for(i = 0; i < num_elem; ++i){
         ++it;
      }
      BOOST_TEST( it == itend );
      BOOST_TEST( c.size() == i );
   }

   //Check iterator conversion
   BOOST_TEST(const_iterator(c.begin()) == c.cbegin() );
   {
      const_iterator it(c.cbegin()), itend(c.cend());
      size_type i;
      for(i = 0; i < num_elem; ++i){
         ++it;
      }
      BOOST_TEST( it == itend );
      BOOST_TEST( c.size() == i );
   }
   static_cast<const Container&>(c).check();
   //Very basic test for comparisons
   {
      BOOST_TEST(c == c);
      BOOST_TEST(!(c != c));
      BOOST_TEST(!(c < c));
      BOOST_TEST(c <= c);
      BOOST_TEST(!(c > c));
      BOOST_TEST(c >= c);
   }
}


template< class Container, class Data >
void test_sequence_container(Container & c, Data & d)
{
   assert( d.size() > 2 );
   {
      c.clear();

      BOOST_TEST( c.size() == 0 );
      BOOST_TEST( c.empty() );


      {
      typename Data::iterator i = d.begin();
      c.insert( c.begin(), *i );
      BOOST_TEST( &*c.iterator_to(*c.begin())  == &*i );
      BOOST_TEST( &*c.iterator_to(*c.cbegin()) == &*i );
      BOOST_TEST( &*Container::s_iterator_to(*c.begin())  == &*i );
      BOOST_TEST( &*Container::s_iterator_to(*c.cbegin()) == &*i );
      c.insert( c.end(), *(++i) );
      }
      BOOST_TEST( c.size() == 2 );
      BOOST_TEST( !c.empty() );

      typename Container::iterator i;
      i = c.erase( c.begin() );

      BOOST_TEST( c.size() == 1 );
      BOOST_TEST( !c.empty() );

      i = c.erase( c.begin() );

      BOOST_TEST( c.size() == 0 );
      BOOST_TEST( c.empty() );

      c.insert( c.begin(), *d.begin() );

      BOOST_TEST( c.size() == 1 );
      BOOST_TEST( !c.empty() );

      {
      typename Data::iterator di = d.begin();
      ++++di;
      c.insert( c.begin(), *(di) );
      }

      i = c.erase( c.begin(), c.end() );
      BOOST_TEST( i == c.end() );

      BOOST_TEST( c.empty() );

      c.insert( c.begin(), *d.begin() );

      BOOST_TEST( c.size() == 1 );

      BOOST_TEST( c.begin() != c.end() );

      i = c.erase_and_dispose( c.begin(), detail::null_disposer() );
      BOOST_TEST( i == c.begin() );

      c.assign(d.begin(), d.end());

      BOOST_TEST( c.size() == d.size() );

      c.clear();

      BOOST_TEST( c.size() == 0 );
      BOOST_TEST( c.empty() );
   }
   {
      c.clear();
      c.insert( c.begin(), d.begin(), d.end() );
      Container move_c(::boost::move(c));
      BOOST_TEST( move_c.size() == d.size() );
      BOOST_TEST( c.empty());

      c = ::boost::move(move_c);
      BOOST_TEST( c.size() == d.size() );
      BOOST_TEST( move_c.empty());
   }
}

template< class Container, class Data >
void test_common_unordered_and_associative_container(Container & c, Data & d, boost::intrusive::detail::true_ unordered)
{
   (void)unordered;
   typedef typename Container::size_type        size_type;
   typedef typename Container::key_of_value     key_of_value;
   typedef typename Container::iterator         iterator;
   typedef typename Container::const_iterator   const_iterator;

   assert( d.size() > 2 );

   c.clear();
   c.insert(d.begin(), d.end());

   {
      Container const &cc = c;
      for( typename Data::const_iterator di = d.begin(), de = d.end();
         di != de; ++di )
      {
         BOOST_TEST( cc.find(key_of_value()(*di), c.hash_function(), c.key_eq()) != cc.end() );
         std::pair<const_iterator, const_iterator> rdi = cc.equal_range(key_of_value()(*di), c.hash_function(), c.key_eq());
         BOOST_TEST(rdi.first != rdi.second);
         BOOST_TEST(size_type(boost::intrusive::iterator_distance(rdi.first, rdi.second)) == cc.count(key_of_value()(*di), c.hash_function(), c.key_eq()));
      }

      for( iterator ci = c.begin(), ce = c.end(); ci != ce; )
      {
         BOOST_TEST( c.find(key_of_value()(*ci)) != c.end() );
         std::pair<iterator, iterator> rci = c.equal_range(key_of_value()(*ci), c.hash_function(), c.key_eq());
         BOOST_TEST(rci.first != rci.second);
         size_type const sc = c.count(key_of_value()(*ci), c.hash_function(), c.key_eq());
         BOOST_TEST(size_type(boost::intrusive::iterator_distance(rci.first, rci.second)) == sc);
         BOOST_TEST(sc == c.erase(key_of_value()(*ci), c.hash_function(), c.key_eq()));
         ci = rci.second;
      }
      BOOST_TEST(c.empty());
   }

   c.clear();
   c.insert(d.begin(), d.end());

   typename Data::const_iterator db = d.begin();
   typename Data::const_iterator da = db++;

   size_type old_size = c.size();

   c.erase(key_of_value()(*da), c.hash_function(), c.key_eq());
   BOOST_TEST( c.size() == old_size-1 );
   //This should not eras anyone
   size_type second_erase = c.erase_and_dispose
      ( key_of_value()(*da), c.hash_function(), c.key_eq(), detail::null_disposer() );
   BOOST_TEST( second_erase == 0 );

   BOOST_TEST( c.count(key_of_value()(*da), c.hash_function(), c.key_eq()) == 0 );
   BOOST_TEST( c.count(key_of_value()(*db), c.hash_function(), c.key_eq()) != 0 );

   BOOST_TEST( c.find(key_of_value()(*da), c.hash_function(), c.key_eq()) == c.end() );
   BOOST_TEST( c.find(key_of_value()(*db), c.hash_function(), c.key_eq()) != c.end() );

   BOOST_TEST( c.equal_range(key_of_value()(*db), c.hash_function(), c.key_eq()).first != c.end() );

   c.clear();

   BOOST_TEST( c.equal_range(key_of_value()(*da), c.hash_function(), c.key_eq()).first == c.end() );

   //
   //suggested_upper_bucket_count
   //
   //Maximum fallbacks to the highest possible value
   typename Container::size_type sz = Container::suggested_upper_bucket_count(size_type(-1));
   BOOST_TEST( sz > size_type(-1)/2 );
   //In the rest of cases the upper bound is returned
   sz = Container::suggested_upper_bucket_count(size_type(-1)/2);
   BOOST_TEST( sz >= size_type(-1)/2 );
   sz = Container::suggested_upper_bucket_count(size_type(-1)/4);
   BOOST_TEST( sz >= size_type(-1)/4 );
   sz = Container::suggested_upper_bucket_count(0);
   BOOST_TEST( sz > 0 );
   //
   //suggested_lower_bucket_count
   //
   sz = Container::suggested_lower_bucket_count(size_type(-1));
   BOOST_TEST( sz <= size_type(-1) );
   //In the rest of cases the lower bound is returned
   sz = Container::suggested_lower_bucket_count(size_type(-1)/2);
   BOOST_TEST( sz <= size_type(-1)/2 );
   sz = Container::suggested_lower_bucket_count(size_type(-1)/4);
   BOOST_TEST( sz <= size_type(-1)/4 );
   //Minimum fallbacks to the lowest possible value
   sz = Container::suggested_upper_bucket_count(0);
   BOOST_TEST( sz > 0 );
}

template< class Container, class Data >
void test_common_unordered_and_associative_container(Container & c, Data & d, boost::intrusive::detail::false_ unordered)
{
   (void)unordered;
   typedef typename Container::size_type        size_type;
   typedef typename Container::key_of_value     key_of_value;
   typedef typename Container::iterator         iterator;
   typedef typename Container::const_iterator   const_iterator;

   assert( d.size() > 2 );

   c.clear();
   typename Container::reference r = *d.begin();
   c.insert(d.begin(), ++d.begin());
   BOOST_TEST( &*Container::s_iterator_to(*c.begin())  == &r );
   BOOST_TEST( &*Container::s_iterator_to(*c.cbegin()) == &r );

   c.clear();
   c.insert(d.begin(), d.end());
   {
      Container const &cc = c;
      for( typename Data::const_iterator di = d.begin(), de = d.end();
         di != de; ++di )
      {
         BOOST_TEST( cc.find(key_of_value()(*di), c.key_comp()) != cc.end() );
         std::pair<const_iterator, const_iterator> rdi = cc.equal_range(key_of_value()(*di), c.key_comp());
         BOOST_TEST(rdi.first != rdi.second);
         BOOST_TEST(size_type(boost::intrusive::iterator_distance(rdi.first, rdi.second)) == cc.count(key_of_value()(*di), c.key_comp()));
      }

      for( iterator ci = c.begin(), ce = c.end(); ci != ce; )
      {
         BOOST_TEST( c.find(key_of_value()(*ci)) != c.end() );
         std::pair<iterator, iterator> rci = c.equal_range(key_of_value()(*ci), c.key_comp());
         BOOST_TEST(rci.first != rci.second);
         size_type const sc = c.count(key_of_value()(*ci), c.key_comp());
         BOOST_TEST(size_type(boost::intrusive::iterator_distance(rci.first, rci.second)) == sc);
         BOOST_TEST(sc == c.erase(key_of_value()(*ci), c.key_comp()));
         ci = rci.second;
      }
      BOOST_TEST(c.empty());
   }

   c.clear();
   c.insert(d.begin(), d.end());

   typename Data::const_iterator db = d.begin();
   typename Data::const_iterator da = db++;

   size_type old_size = c.size();

   c.erase(key_of_value()(*da), c.key_comp());
   BOOST_TEST( c.size() == old_size-1 );
   //This should not erase any
   size_type second_erase = c.erase_and_dispose( key_of_value()(*da), c.key_comp(), detail::null_disposer() );
   BOOST_TEST( second_erase == 0 );

   BOOST_TEST( c.count(key_of_value()(*da), c.key_comp()) == 0 );
   BOOST_TEST( c.count(key_of_value()(*db), c.key_comp()) != 0 );
   BOOST_TEST( c.find(key_of_value()(*da), c.key_comp()) == c.end() );
   BOOST_TEST( c.find(key_of_value()(*db), c.key_comp()) != c.end() );
   BOOST_TEST( c.equal_range(key_of_value()(*db), c.key_comp()).first != c.end() );
   c.clear();
   BOOST_TEST( c.equal_range(key_of_value()(*da), c.key_comp()).first == c.end() );
}

template< class Container, class Data >
void test_common_unordered_and_associative_container(Container & c, Data & d)
{
   typedef typename Container::size_type        size_type;
   typedef typename Container::key_of_value     key_of_value;
   typedef typename Container::iterator         iterator;
   typedef typename Container::const_iterator   const_iterator;

   {
      assert( d.size() > 2 );

      c.clear();
      typename Container::reference r = *d.begin();
      c.insert(d.begin(), ++d.begin());
      BOOST_TEST( &*c.iterator_to(*c.begin())  == &r );
      BOOST_TEST( &*c.iterator_to(*c.cbegin()) == &r );

      c.clear();
      c.insert(d.begin(), d.end());

      Container const &cc = c;
      for( typename Data::const_iterator di = d.begin(), de = d.end();
         di != de; ++di )
      {
         BOOST_TEST( cc.find(key_of_value()(*di)) != cc.end() );
         std::pair<const_iterator, const_iterator> rdi = cc.equal_range(key_of_value()(*di));
         BOOST_TEST(rdi.first != rdi.second);
         BOOST_TEST(size_type(boost::intrusive::iterator_distance(rdi.first, rdi.second)) == cc.count(key_of_value()(*di)));
      }

      for( iterator ci = c.begin(), ce = c.end(); ci != ce; )
      {
         BOOST_TEST( c.find(key_of_value()(*ci)) != c.end() );
         std::pair<iterator, iterator> rci = c.equal_range(key_of_value()(*ci));
         BOOST_TEST(rci.first != rci.second);
         size_type const sc = c.count(key_of_value()(*ci));
         BOOST_TEST(size_type(boost::intrusive::iterator_distance(rci.first, rci.second)) == sc);
         BOOST_TEST(sc == c.erase(key_of_value()(*ci)));
         ci = rci.second;
      }
      BOOST_TEST(c.empty());
   }
   {
      c.clear();
      c.insert(d.begin(), d.end());

      typename Data::const_iterator db = d.begin();
      typename Data::const_iterator da = db++;

      size_type old_size = c.size();

      c.erase(key_of_value()(*da));
      BOOST_TEST( c.size() == old_size-1 );
      //This should erase nothing
      size_type second_erase = c.erase_and_dispose( key_of_value()(*da), detail::null_disposer() );
      BOOST_TEST( second_erase == 0 );

      BOOST_TEST( c.count(key_of_value()(*da)) == 0 );
      BOOST_TEST( c.count(key_of_value()(*db)) != 0 );

      BOOST_TEST( c.find(key_of_value()(*da)) == c.end() );
      BOOST_TEST( c.find(key_of_value()(*db)) != c.end() );

      BOOST_TEST( c.equal_range(key_of_value()(*db)).first != c.end() );
      BOOST_TEST( c.equal_range(key_of_value()(*da)).first == c.equal_range(key_of_value()(*da)).second );
   }
   {
      c.clear();
      c.insert( d.begin(), d.end() );
      size_type orig_size = c.size();
      Container move_c(::boost::move(c));
      BOOST_TEST(orig_size == move_c.size());
      BOOST_TEST( c.empty());
      for( typename Data::const_iterator di = d.begin(), de = d.end();
         di != de; ++di )
      {  BOOST_TEST( move_c.find(key_of_value()(*di)) != move_c.end() );   }

      c = ::boost::move(move_c);
      for( typename Data::const_iterator di = d.begin(), de = d.end();
         di != de; ++di )
      {  BOOST_TEST( c.find(key_of_value()(*di)) != c.end() );   }
      BOOST_TEST( move_c.empty());
   }
   typedef detail::bool_<is_unordered<Container>::value> enabler;
   test_common_unordered_and_associative_container(c, d, enabler());
}

template< class Container, class Data >
void test_associative_container_invariants(Container & c, Data & d)
{
   typedef typename Container::const_iterator   const_iterator;
   typedef typename Container::key_of_value     key_of_value;
   for( typename Data::const_iterator di = d.begin(), de = d.end();
      di != de; ++di)
   {
      const_iterator ci = c.find(key_of_value()(*di));
      BOOST_TEST( ci != c.end() );
      BOOST_TEST( ! c.value_comp()(*ci, *di) );
      BOOST_TEST( ! c.key_comp()(key_of_value()(*ci), key_of_value()(*di)) );
      const_iterator cil = c.lower_bound(key_of_value()(*di));
      const_iterator ciu = c.upper_bound(key_of_value()(*di));
      std::pair<const_iterator, const_iterator> er = c.equal_range(key_of_value()(*di));
      BOOST_TEST( cil == er.first );
      BOOST_TEST( ciu == er.second );
      if(ciu != c.end()){
         BOOST_TEST( c.value_comp()(*cil, *ciu) );
         BOOST_TEST( c.key_comp()(key_of_value()(*cil), key_of_value()(*ciu)) );
      }
      if(c.count(key_of_value()(*di)) > 1){
         const_iterator ci_next = cil; ++ci_next;
         for( ; ci_next != ciu; ++cil, ++ci_next){
            BOOST_TEST( !c.value_comp()(*ci_next, *cil) );
            BOOST_TEST( !c.key_comp()(key_of_value()(*ci_next), key_of_value()(*cil)) );
         }
      }
   }
}

template< class Container, class Data >
void test_associative_container(Container & c, Data & d)
{
   assert( d.size() > 2 );

   c.clear();
   c.insert(d.begin(),d.end());

   test_associative_container_invariants(c, d);

   const Container & cr = c;

   test_associative_container_invariants(cr, d);
}

template< class Container, class Data >
void test_unordered_associative_container_invariants(Container & c, Data & d)
{
   typedef typename Container::size_type        size_type;
   typedef typename Container::const_iterator   const_iterator;
   typedef typename Container::key_of_value     key_of_value;

   for( typename Data::const_iterator di = d.begin(), de = d.end() ;
      di != de ; ++di ){
      const_iterator i = c.find(key_of_value()(*di));
      size_type nb = c.bucket(key_of_value()(*i));
      size_type bucket_elem = boost::intrusive::iterator_distance(c.begin(nb), c.end(nb));
      BOOST_TEST( bucket_elem ==  c.bucket_size(nb) );
      BOOST_TEST( &*c.local_iterator_to(*c.find(key_of_value()(*di))) == &*i );
      BOOST_TEST( &*c.local_iterator_to(*const_cast<const Container &>(c).find(key_of_value()(*di))) == &*i );
      BOOST_TEST( &*Container::s_local_iterator_to(*c.find(key_of_value()(*di))) == &*i );
      BOOST_TEST( &*Container::s_local_iterator_to(*const_cast<const Container &>(c).find(key_of_value()(*di))) == &*i );
      std::pair<const_iterator, const_iterator> er = c.equal_range(key_of_value()(*di));
      size_type cnt = boost::intrusive::iterator_distance(er.first, er.second);
      BOOST_TEST( cnt == c.count(key_of_value()(*di)));
      if(cnt > 1){
         const_iterator n = er.first;
         i = n++;
         const_iterator e = er.second;
         for(; n != e; ++i, ++n){
            BOOST_TEST( c.key_eq()(key_of_value()(*i), key_of_value()(*n)) );
            BOOST_TEST( (c.hash_function()(key_of_value()(*i))) == (c.hash_function()(key_of_value()(*n))) );
         }
      }
   }

   size_type blen = c.bucket_count();
   size_type total_objects = 0;
   for(size_type i = 0; i < blen; ++i){
      total_objects += c.bucket_size(i);
   }
   BOOST_TEST( total_objects ==  c.size() );
}

template< class Container, class Data >
void test_unordered_associative_container(Container & c, Data & d)
{
   c.clear();
   c.insert( d.begin(), d.end() );

   test_unordered_associative_container_invariants(c, d);

   const Container & cr = c;

   test_unordered_associative_container_invariants(cr, d);
}

template< class Container, class Data >
void test_unique_container(Container & c, Data & d)
{
   //typedef typename Container::value_type value_type;
   c.clear();
   c.insert(d.begin(),d.end());
   typename Container::size_type old_size = c.size();
   //value_type v(*d.begin());
   //c.insert(v);
   Data d2(1);
   (&d2.front())->value_ = (&d.front())->value_;
   c.insert(d2.front());
   BOOST_TEST( c.size() == old_size );
   c.clear();
}

template< class Container, class Data >
void test_non_unique_container(Container & c, Data & d)
{
   //typedef typename Container::value_type value_type;
   c.clear();
   c.insert(d.begin(),d.end());
   typename Container::size_type old_size = c.size();
   //value_type v(*d.begin());
   //c.insert(v);
   Data d2(1);
   (&d2.front())->value_ = (&d.front())->value_;
   c.insert(d2.front());
   BOOST_TEST( c.size() == (old_size+1) );
   c.clear();
}

template< class Container, class Data >
void test_maybe_unique_container(Container & c, Data & d, detail::false_)//is_unique
{  test_unique_container(c, d);  }

template< class Container, class Data >
void test_maybe_unique_container(Container & c, Data & d, detail::true_)//!is_unique
{  test_non_unique_container(c, d);  }

}}}

#endif   //#ifndef BOOST_INTRUSIVE_TEST_CONTAINER_HPP
