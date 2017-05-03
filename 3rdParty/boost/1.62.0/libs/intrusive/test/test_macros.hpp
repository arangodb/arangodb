/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2006-2013
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_TEST_TEST_MACROS_HPP
#define BOOST_INTRUSIVE_TEST_TEST_MACROS_HPP

#include <boost/intrusive/intrusive_fwd.hpp>
#include <algorithm> //std::unique

namespace boost{
namespace intrusive{
namespace test{

template <class T>
struct is_multikey_true
{
   typedef char yes_type;
   template<bool IsMultikey>
   struct two { yes_type _[1+IsMultikey]; };
   template <class U> static yes_type           test(...);
   template <class U> static two<U::is_multikey>test(int);
   static const bool value = sizeof(test<T>(0)) != sizeof(yes_type);
};

}  //namespace test{

template<class It1, class It2>
bool test_equal(It1 f1, It1 l1, It2 f2)
{
   while(f1 != l1){
      if(*f1 != *f2){
         return false;
      }
      ++f1;
      ++f2;
   }
   return true;
}

#define TEST_INTRUSIVE_SEQUENCE( INTVALUES, ITERATOR )\
{  \
   BOOST_TEST (boost::intrusive::test_equal(&INTVALUES[0], &INTVALUES[0] + sizeof(INTVALUES)/sizeof(INTVALUES[0]), ITERATOR) ); \
}

#define TEST_INTRUSIVE_SEQUENCE_EXPECTED( EXPECTEDVECTOR, ITERATOR )\
{  \
   BOOST_TEST (boost::intrusive::test_equal(EXPECTEDVECTOR.begin(), EXPECTEDVECTOR.end(), ITERATOR) ); \
}

namespace test{

template<class Container, class Vector>
void test_intrusive_maybe_unique(const Container &c, Vector &v)
{
   if(!is_multikey_true<Container>::value)
      v.erase(std::unique(v.begin(), v.end()), v.end());
   BOOST_TEST (boost::intrusive::test_equal(v.begin(), v.end(), c.begin()) );
}

}  //namespace test{

#define TEST_INTRUSIVE_SEQUENCE_MAYBEUNIQUE(INTVALUES, CONTAINER)\
{\
   boost::container::vector<int>v(&INTVALUES[0], &INTVALUES[0] + sizeof(INTVALUES)/sizeof(INTVALUES[0]));\
   boost::intrusive::test::test_intrusive_maybe_unique(CONTAINER, v);\
}\
//

}  //namespace boost{
}  //namespace intrusive{

#endif
