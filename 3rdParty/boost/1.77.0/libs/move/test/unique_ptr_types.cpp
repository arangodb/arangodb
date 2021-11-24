//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Howard Hinnant 2009
// (C) Copyright Ion Gaztanaga 2014-2014.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/move/utility_core.hpp>
#include <boost/move/unique_ptr.hpp>
#include <boost/move/detail/type_traits.hpp>
#include <boost/static_assert.hpp>
#include <boost/core/lightweight_test.hpp>

//////////////////////////////////////////////
//
// The initial implementation of these tests
// was written by Howard Hinnant. 
//
// These test were later refactored grouping
// and porting them to Boost.Move.
//
// Many thanks to Howard for releasing his C++03
// unique_ptr implementation with such detailed
// test cases.
//
//////////////////////////////////////////////

#include "unique_ptr_test_utils_beg.hpp"

namespace bml = ::boost::movelib;
namespace bmupmu = ::boost::move_upmu;

////////////////////////////////
//   unique_ptr_pointer_type
////////////////////////////////
namespace unique_ptr_pointer_type {

struct Deleter
{
   struct pointer {};
};

// Test unique_ptr::pointer type
void test()
{
   //Single unique_ptr
   {
   typedef bml::unique_ptr<int> P;
   BOOST_STATIC_ASSERT((bmupmu::is_same<P::pointer, int*>::value));
   }
   {
   typedef bml::unique_ptr<int, Deleter> P;
   BOOST_STATIC_ASSERT((bmupmu::is_same<P::pointer, Deleter::pointer>::value));
   }
   //Unbounded array unique_ptr
   {
   typedef bml::unique_ptr<int[]> P;
   BOOST_STATIC_ASSERT((bmupmu::is_same<P::pointer, int*>::value));
   }
   {
   typedef bml::unique_ptr<int[], Deleter> P;
   BOOST_STATIC_ASSERT((bmupmu::is_same<P::pointer, Deleter::pointer>::value));
   }
   //Bounded array unique_ptr
   {
   typedef bml::unique_ptr<int[5]> P;
   BOOST_STATIC_ASSERT((bmupmu::is_same<P::pointer, int*>::value));
   }
   {
   typedef bml::unique_ptr<int[5], Deleter> P;
   BOOST_STATIC_ASSERT((bmupmu::is_same<P::pointer, Deleter::pointer>::value));
   }
   //Unbounded array of bounded array unique_ptr
   {
   typedef int int_5_t [5];
   typedef bml::unique_ptr<int_5_t[]> P;
   BOOST_STATIC_ASSERT((bmupmu::is_same<P::pointer, int_5_t*>::value));
   }
   {
   typedef int int_5_t [5];
   typedef bml::unique_ptr<int_5_t[], Deleter> P;
   BOOST_STATIC_ASSERT((bmupmu::is_same<P::pointer, Deleter::pointer>::value));
   }
}

}  //namespace unique_ptr_pointer_type {

////////////////////////////////
//    unique_ptr_deleter_type
////////////////////////////////
namespace unique_ptr_deleter_type {

struct Deleter
{};

// Test unique_ptr::deleter type
void test()
{
   //Single unique_ptr
   {
   typedef bml::unique_ptr<int> P;
   BOOST_STATIC_ASSERT((bmupmu::is_same<P::deleter_type, bml::default_delete<int> >::value));
   }
   {
   typedef bml::unique_ptr<int, Deleter> P;
   BOOST_STATIC_ASSERT((bmupmu::is_same<P::deleter_type, Deleter >::value));
   }
   //Unbounded array unique_ptr
   {
   typedef bml::unique_ptr<int[]> P;
   BOOST_STATIC_ASSERT((bmupmu::is_same<P::deleter_type, bml::default_delete<int[]> >::value));
   }
   {
   typedef bml::unique_ptr<int[], Deleter> P;
   BOOST_STATIC_ASSERT((bmupmu::is_same<P::deleter_type, Deleter >::value));
   }
   //Bounded array unique_ptr
   {
   typedef bml::unique_ptr<int[2]> P;
   BOOST_STATIC_ASSERT((bmupmu::is_same<P::deleter_type, bml::default_delete<int[2]> >::value));
   }
   {
   typedef bml::unique_ptr<int[2], Deleter> P;
   BOOST_STATIC_ASSERT((bmupmu::is_same<P::deleter_type, Deleter >::value));
   }
}

}  //namespace unique_ptr_deleter_type {

////////////////////////////////
//    unique_ptr_element_type
////////////////////////////////
namespace unique_ptr_element_type {

// Test unique_ptr::deleter type
void test()
{
   //Single unique_ptr
   {
   typedef bml::unique_ptr<const int> P;
   BOOST_STATIC_ASSERT((bmupmu::is_same<P::element_type, const int>::value));
   }
   //Unbounded array unique_ptr
   {
   typedef bml::unique_ptr<const int[]> P;
   BOOST_STATIC_ASSERT((bmupmu::is_same<P::element_type, const int>::value));
   }
   //Bounded array unique_ptr
   {
   typedef bml::unique_ptr<const int[2]> P;
   BOOST_STATIC_ASSERT((bmupmu::is_same<P::element_type, const int>::value));
   }
}

}  //namespace unique_ptr_element_type {

////////////////////////////////
//    unique_ptr_construct_assign_traits
////////////////////////////////

namespace unique_ptr_construct_assign_traits {

   void test()
   {
      typedef bml::unique_ptr<int> unique_ptr_t;
      //Even if BOOST_MOVE_TT_CXX11_IS_COPY_CONSTRUCTIBLE is not defined
      //boost::unique_ptr shall work with boost::movelib traits
      BOOST_STATIC_ASSERT(!(boost::move_detail::is_copy_constructible<unique_ptr_t>::value));
      //Even if BOOST_MOVE_TT_CXX11_IS_COPY_ASSIGNABLE is not defined
      //boost::unique_ptr shall work with boost::movelib traits
      BOOST_STATIC_ASSERT(!(boost::move_detail::is_copy_assignable<unique_ptr_t>::value));
      //Important traits for containers like boost::vector
      BOOST_STATIC_ASSERT(!(boost::move_detail::is_trivially_copy_constructible<unique_ptr_t>::value));
      BOOST_STATIC_ASSERT(!(boost::move_detail::is_trivially_copy_assignable<unique_ptr_t>::value));
   }

}  //namespace unique_ptr_construct_assign_traits {

////////////////////////////////
//             main
////////////////////////////////

int main()
{
   //General
   unique_ptr_pointer_type::test();
   unique_ptr_deleter_type::test();
   unique_ptr_element_type::test();
   unique_ptr_construct_assign_traits::test();

   //Test results
   return boost::report_errors();
}

#include "unique_ptr_test_utils_end.hpp"
