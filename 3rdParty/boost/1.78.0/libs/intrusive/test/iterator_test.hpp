//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2015-2015. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/intrusive/detail/iterator.hpp>
#include <boost/intrusive/detail/mpl.hpp>
#include <boost/static_assert.hpp>

namespace boost{  namespace intrusive {  namespace test{

//////////////////////////////////////////////
//
// Some traits to avoid special cases with
// containers without bidirectional iterators
//
//////////////////////////////////////////////
template<class C>
struct has_member_reverse_iterator
{
   typedef char yes_type;
   struct no_type{   char _[2];  };

   template<typename D> static no_type test(...);
   template<typename D> static yes_type test(typename D::reverse_iterator const*);
   static const bool value = sizeof(test<C>(0)) == sizeof(yes_type);
};

template<class C>
struct has_member_const_reverse_iterator
{
   typedef char yes_type;
   struct no_type{   char _[2];  };

   template<typename D> static no_type test(...);
   template<typename D> static yes_type test(typename D::const_reverse_iterator const*);
   static const bool value = sizeof(test<C>(0)) == sizeof(yes_type);
};

template<class C, bool = has_member_reverse_iterator<C>::value>
struct get_reverse_iterator
{
   typedef typename C::reverse_iterator type;
   static type begin(C &c) {  return c.rbegin(); }
   static type   end(C &c) {  return c.rend(); }
};

template<class C>
struct get_reverse_iterator<C, false>
{
   typedef typename C::iterator type;
   static type begin(C &c) {  return c.begin(); }
   static type   end(C &c) {  return c.end(); }
};

template<class C, bool = has_member_const_reverse_iterator<C>::value>
struct get_const_reverse_iterator
{
   typedef typename C::const_reverse_iterator type;
   static type begin(C &c) {  return c.crbegin(); }
   static type   end(C &c) {  return c.crend(); }
};

template<class C>
struct get_const_reverse_iterator<C, false>
{
   typedef typename C::const_iterator type;
   static type begin(C &c) {  return c.cbegin(); }
   static type   end(C &c) {  return c.cend(); }
};

//////////////////////////////////////////////
//
//          Iterator tests
//
//////////////////////////////////////////////

template<class I>
void test_iterator_operations(I b, I e)
{
   //Test the range is not empty
   BOOST_TEST(b != e);
   BOOST_TEST(!(b == e));
   {
      I i;
      I i2(b); //CopyConstructible
      i = i2;  //Assignable
      //Destructible
      (void)i;
      (void)i2;
   }

   typedef typename iterator_traits<I>::reference reference;
   reference r = *b;
   (void)r;
   typedef typename iterator_traits<I>::pointer pointer;
   pointer p = (iterator_arrow_result)(b);
   (void)p;
   I &ri= ++b;
   (void)ri;
   const I &cri= b++;
   (void)cri;
}

template<class C>
void test_iterator_compatible(C &c)
{
   typedef typename C::iterator        iterator;
   typedef typename C::const_iterator  const_iterator;
   typedef typename get_reverse_iterator<C>::type        reverse_iterator;
   typedef typename get_const_reverse_iterator<C>::type  const_reverse_iterator;
   //Basic operations
   test_iterator_operations(c. begin(), c. end());
   test_iterator_operations(c.cbegin(), c.cend());
   test_iterator_operations(get_reverse_iterator<C>::begin(c), get_reverse_iterator<C>::end(c));
   test_iterator_operations(get_const_reverse_iterator<C>::begin(c), get_const_reverse_iterator<C>::end(c));
   //Make sure dangeous conversions are not possible
   BOOST_STATIC_ASSERT((!boost::intrusive::detail::is_convertible<const_iterator, iterator>::value));
   BOOST_STATIC_ASSERT((!boost::intrusive::detail::is_convertible<const_reverse_iterator, reverse_iterator>::value));
   //Test iterator conversions
   {  
      const_iterator ci;
      iterator i(c.begin());
      ci = i;
      (void)ci;
      BOOST_ASSERT(ci == i);
      BOOST_ASSERT(*ci == *i);
      const_iterator ci2(i);
      BOOST_ASSERT(ci2 == i);
      BOOST_ASSERT(*ci2 == *i);
      (void)ci2;
   }
   //Test reverse_iterator conversions
   {
      const_reverse_iterator cr;
      reverse_iterator r(get_reverse_iterator<C>::begin(c));
      cr = r;
      BOOST_ASSERT(cr == r);
      BOOST_ASSERT(*cr == *r);
      const_reverse_iterator cr2(r);
      BOOST_ASSERT(cr2 == r);
      BOOST_ASSERT(*cr2 == *r);
      (void)cr2;
   }
}

template<class C>
void test_iterator_input_and_compatible(C &c)
{
   typedef typename C::iterator                          iterator;
   typedef typename C::const_iterator                    const_iterator;
   typedef typename get_reverse_iterator<C>::type        reverse_iterator;
   typedef typename get_const_reverse_iterator<C>::type  const_reverse_iterator;
   typedef iterator_traits<iterator>            nit_traits;
   typedef iterator_traits<const_iterator>      cit_traits;
   typedef iterator_traits<reverse_iterator>         rnit_traits;
   typedef iterator_traits<const_reverse_iterator>   crit_traits;

   using boost::move_detail::is_same;
   //Trivial typedefs
   BOOST_STATIC_ASSERT((!is_same<iterator, const_iterator>::value));
   BOOST_STATIC_ASSERT((!is_same<reverse_iterator, const_reverse_iterator>::value));
   //difference_type
   typedef typename C::difference_type difference_type;
   BOOST_STATIC_ASSERT((is_same<difference_type, typename nit_traits::difference_type>::value));
   BOOST_STATIC_ASSERT((is_same<difference_type, typename cit_traits::difference_type>::value));
   BOOST_STATIC_ASSERT((is_same<difference_type, typename rnit_traits::difference_type>::value));
   BOOST_STATIC_ASSERT((is_same<difference_type, typename crit_traits::difference_type>::value));
   //value_type
   typedef typename C::value_type value_type;
   BOOST_STATIC_ASSERT((is_same<value_type, typename nit_traits::value_type>::value));
   BOOST_STATIC_ASSERT((is_same<value_type, typename cit_traits::value_type>::value));
   BOOST_STATIC_ASSERT((is_same<value_type, typename rnit_traits::value_type>::value));
   BOOST_STATIC_ASSERT((is_same<value_type, typename crit_traits::value_type>::value));
   //pointer
   typedef typename C::pointer pointer;
   typedef typename C::const_pointer const_pointer;
   BOOST_STATIC_ASSERT((is_same<pointer,        typename nit_traits::pointer>::value));
   BOOST_STATIC_ASSERT((is_same<const_pointer,  typename cit_traits::pointer>::value));
   BOOST_STATIC_ASSERT((is_same<pointer,        typename rnit_traits::pointer>::value));
   BOOST_STATIC_ASSERT((is_same<const_pointer,  typename crit_traits::pointer>::value));
   //reference
   typedef typename C::reference reference;
   typedef typename C::const_reference const_reference;
   BOOST_STATIC_ASSERT((is_same<reference,         typename nit_traits::reference>::value));
   BOOST_STATIC_ASSERT((is_same<const_reference,   typename cit_traits::reference>::value));
   BOOST_STATIC_ASSERT((is_same<reference,         typename rnit_traits::reference>::value));
   BOOST_STATIC_ASSERT((is_same<const_reference,   typename crit_traits::reference>::value));
   //Dynamic tests
   test_iterator_compatible(c);
}

template<class C, class I>
void test_iterator_forward_functions(C const &c, I const b, I const e)
{
   typedef typename C::size_type size_type;
   {
      size_type i = 0;
      I it = b;
      for(I it2 = b; i != c.size(); ++it, ++i){
         BOOST_TEST(it == it2++);
         I ittmp(it);
         I *iaddr = &ittmp;
         BOOST_TEST(&(++ittmp) == iaddr);
         BOOST_TEST(ittmp == it2);
      }
      BOOST_TEST(i == c.size());
      BOOST_TEST(it == e);
   }
}

template<class C>
void test_iterator_forward_and_compatible(C &c)
{
   test_iterator_input_and_compatible(c);
   test_iterator_forward_functions(c, c.begin(),   c.end());
   test_iterator_forward_functions(c, c.cbegin(),  c.cend());
   test_iterator_forward_functions(c, get_reverse_iterator<C>::begin(c), get_reverse_iterator<C>::end(c));
   test_iterator_forward_functions(c, get_const_reverse_iterator<C>::begin(c), get_const_reverse_iterator<C>::end(c));
}

template<class C, class I>
void test_iterator_bidirectional_functions(C const &c, I const b, I const e)
{
   typedef typename C::size_type size_type;
   {
      size_type i = 0;
      I it = e;
      for(I it2 = e; i != c.size(); --it, ++i){
         BOOST_TEST(it == it2--);
         I ittmp(it);
         I*iaddr = &ittmp;
         BOOST_TEST(&(--ittmp) == iaddr);
         BOOST_TEST(ittmp == it2);
         BOOST_TEST((++ittmp) == it);
      }
      BOOST_TEST(i == c.size());
      BOOST_TEST(it == b);
   }
}

template<class C>
void test_iterator_bidirectional_and_compatible(C &c)
{
   test_iterator_forward_and_compatible(c);
   test_iterator_bidirectional_functions(c, c.begin(),   c.end());
   test_iterator_bidirectional_functions(c, c.cbegin(),  c.cend());
   test_iterator_bidirectional_functions(c, c.rbegin(),  c.rend());
   test_iterator_bidirectional_functions(c, c.crbegin(), c.crend());
}

template<class C, class I>
void test_iterator_random_functions(C const &c, I const b, I const e)
{
   typedef typename C::size_type size_type;
   typedef typename C::difference_type difference_type;
   {
      I it = b;
      for(difference_type i = 0, m = difference_type(c.size()); i != m; ++i, ++it){
         BOOST_TEST(i == it - b);
         BOOST_TEST(b[i] == *it);
         BOOST_TEST(&b[i] == &*it);
         BOOST_TEST((b + i) == it);
         BOOST_TEST((i + b) == it);
         BOOST_TEST(b == (it - i));
         I tmp(b);
         BOOST_TEST((tmp+=i) == it);
         tmp = it;
         BOOST_TEST(b == (tmp-=i));
      }
      BOOST_TEST(c.size() == size_type(e - b));
   }
   {
      I it(b), itb(b);
      if(b != e){
         for(++it; it != e; ++it){
            BOOST_TEST(itb < it);
            BOOST_TEST(itb <= it);
            BOOST_TEST(!(itb > it));
            BOOST_TEST(!(itb >= it));
            BOOST_TEST(it > itb);
            BOOST_TEST(it >= itb);
            BOOST_TEST(!(it < itb));
            BOOST_TEST(!(it <= itb));
            BOOST_TEST(it >= it);
            BOOST_TEST(it <= it);
            itb = it;
         }
      }
   }
}

template<class C>
void test_iterator_random_and_compatible(C &c)
{
   test_iterator_bidirectional_and_compatible(c);
   test_iterator_random_functions(c, c.begin(),   c.end());
   test_iterator_random_functions(c, c.cbegin(),  c.cend());
   test_iterator_random_functions(c, c.rbegin(),  c.rend());
   test_iterator_random_functions(c, c.crbegin(), c.crend());
}

////////////////////////

template<class C>
void test_iterator_forward(C &c)
{
   typedef typename C::iterator                 iterator;
   typedef typename C::const_iterator           const_iterator;
   typedef typename get_reverse_iterator<C>::type        reverse_iterator;
   typedef typename get_const_reverse_iterator<C>::type  const_reverse_iterator;
   typedef iterator_traits<iterator>         nit_traits;
   typedef iterator_traits<const_iterator>   cit_traits;
   typedef iterator_traits<reverse_iterator>         rnit_traits;
   typedef iterator_traits<const_reverse_iterator>   crit_traits;

   using boost::intrusive::detail::is_same;
   //iterator_category
   BOOST_STATIC_ASSERT((is_same<std::forward_iterator_tag, typename nit_traits::iterator_category>::value));
   BOOST_STATIC_ASSERT((is_same<std::forward_iterator_tag, typename cit_traits::iterator_category>::value));
   BOOST_STATIC_ASSERT((is_same<std::forward_iterator_tag, typename rnit_traits::iterator_category>::value));
   BOOST_STATIC_ASSERT((is_same<std::forward_iterator_tag, typename crit_traits::iterator_category>::value));
   //Test dynamic
   test_iterator_forward_and_compatible(c);
}

template<class C>
void test_iterator_bidirectional(C &c)
{
   typedef typename C::iterator        iterator;
   typedef typename C::const_iterator  const_iterator;
   typedef typename C::reverse_iterator        reverse_iterator;
   typedef typename C::const_reverse_iterator  const_reverse_iterator;
   typedef iterator_traits<iterator>         nit_traits;
   typedef iterator_traits<const_iterator>   cit_traits;
   typedef iterator_traits<reverse_iterator>         rnit_traits;
   typedef iterator_traits<const_reverse_iterator>   crit_traits;

   using boost::intrusive::detail::is_same;
   //iterator_category
   BOOST_STATIC_ASSERT((is_same<std::bidirectional_iterator_tag, typename nit_traits::iterator_category>::value));
   BOOST_STATIC_ASSERT((is_same<std::bidirectional_iterator_tag, typename cit_traits::iterator_category>::value));
   BOOST_STATIC_ASSERT((is_same<std::bidirectional_iterator_tag, typename rnit_traits::iterator_category>::value));
   BOOST_STATIC_ASSERT((is_same<std::bidirectional_iterator_tag, typename crit_traits::iterator_category>::value));
   //Test dynamic
   test_iterator_bidirectional_and_compatible(c);
}

template<class C>
void test_iterator_random(C &c)
{
   typedef typename C::iterator        iterator;
   typedef typename C::const_iterator  const_iterator;
   typedef typename C::reverse_iterator        reverse_iterator;
   typedef typename C::const_reverse_iterator  const_reverse_iterator;
   typedef iterator_traits<iterator>         nit_traits;
   typedef iterator_traits<const_iterator>   cit_traits;
   typedef iterator_traits<reverse_iterator>         rnit_traits;
   typedef iterator_traits<const_reverse_iterator>   crit_traits;

   using boost::intrusive::detail::is_same;
   //iterator_category
   BOOST_STATIC_ASSERT((is_same<std::random_access_iterator_tag, typename nit_traits::iterator_category>::value));
   BOOST_STATIC_ASSERT((is_same<std::random_access_iterator_tag, typename cit_traits::iterator_category>::value));
   BOOST_STATIC_ASSERT((is_same<std::random_access_iterator_tag, typename rnit_traits::iterator_category>::value));
   BOOST_STATIC_ASSERT((is_same<std::random_access_iterator_tag, typename crit_traits::iterator_category>::value));
   //Test dynamic
   test_iterator_random_and_compatible(c);
}

}}}   //boost::container::test
