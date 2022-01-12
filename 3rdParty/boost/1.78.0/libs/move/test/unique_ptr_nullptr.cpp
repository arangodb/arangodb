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
#include <boost/core/ignore_unused.hpp>
#include <boost/move/unique_ptr.hpp>
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

////////////////////////////////
//   unique_ptr_zero
////////////////////////////////
namespace unique_ptr_zero {

// test initialization/assignment from zero

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   bml::unique_ptr<A> s2(0);
   BOOST_TEST(A::count == 0);
   }
   BOOST_TEST(A::count == 0);
   {
   bml::unique_ptr<A> s2(new A);
   BOOST_TEST(A::count == 1);
   s2 = 0;
   BOOST_TEST(A::count == 0);
   BOOST_TEST(s2.get() == 0);
   }
   BOOST_TEST(A::count == 0);

   //Unbounded array unique_ptr
   {
   bml::unique_ptr<A[]> s2(0);
   BOOST_TEST(A::count == 0);
   }
   BOOST_TEST(A::count == 0);
   {
   bml::unique_ptr<A[]> s2(new A[2]);
   BOOST_TEST(A::count == 2);
   s2 = 0;
   BOOST_TEST(A::count == 0);
   BOOST_TEST(s2.get() == 0);
   }
   BOOST_TEST(A::count == 0);

   //Bounded array unique_ptr
   {
   bml::unique_ptr<A[2]> s2(0);
   BOOST_TEST(A::count == 0);
   }
   BOOST_TEST(A::count == 0);
   {
   bml::unique_ptr<A[2]> s2(new A[2]);
   BOOST_TEST(A::count == 2);
   s2 = 0;
   BOOST_TEST(A::count == 0);
   BOOST_TEST(s2.get() == 0);
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_ptr_zero {


////////////////////////////////
//       unique_ptr_nullptr
////////////////////////////////

namespace unique_ptr_nullptr{

void test()
{
   #if !defined(BOOST_NO_CXX11_NULLPTR)
   //Single unique_ptr
   reset_counters();
   {
      bml::unique_ptr<A> p(new A);
      BOOST_TEST(A::count == 1);
      A* i = p.get();
      ::boost::ignore_unused(i);
      p.reset(nullptr);
      BOOST_TEST(A::count == 0);
      BOOST_TEST(p.get() == 0);
   }
   BOOST_TEST(A::count == 0);
   {
      bml::unique_ptr<A> p(new A);
      BOOST_TEST(A::count == 1);
      A* i = p.get();
      ::boost::ignore_unused(i);
      p = nullptr;
      BOOST_TEST(A::count == 0);
      BOOST_TEST(p.get() == 0);
   }
   BOOST_TEST(A::count == 0);

   {
      bml::unique_ptr<A> pi(nullptr);
      BOOST_TEST(pi.get() == nullptr);
      BOOST_TEST(pi.get() == 0);
   }
   BOOST_TEST(A::count == 0);
   {
      bml::unique_ptr<A> pi(nullptr, bml::unique_ptr<A>::deleter_type());
      BOOST_TEST(pi.get() == nullptr);
      BOOST_TEST(pi.get() == 0);
   }
   BOOST_TEST(A::count == 0);

   //Unbounded array unique_ptr
   reset_counters();
   {
      bml::unique_ptr<A[]> p(new A[2]);
      BOOST_TEST(A::count == 2);
      A* i = p.get();
      ::boost::ignore_unused(i);
      p.reset(nullptr);
      BOOST_TEST(A::count == 0);
      BOOST_TEST(p.get() == 0);
   }
   BOOST_TEST(A::count == 0);
   {
      bml::unique_ptr<A[]> p(new A[2]);
      BOOST_TEST(A::count == 2);
      A* i = p.get();
      ::boost::ignore_unused(i);
      p = nullptr;
      BOOST_TEST(A::count == 0);
      BOOST_TEST(p.get() == 0);
   }
   BOOST_TEST(A::count == 0);
   {
      bml::unique_ptr<A[]> pi(nullptr);
      BOOST_TEST(pi.get() == nullptr);
      BOOST_TEST(pi.get() == 0);
   }
   BOOST_TEST(A::count == 0);
   {
      bml::unique_ptr<A[]> pi(nullptr, bml::unique_ptr<A[]>::deleter_type());
      BOOST_TEST(pi.get() == nullptr);
      BOOST_TEST(pi.get() == 0);
   }
   BOOST_TEST(A::count == 0);

   //Bounded array unique_ptr
   reset_counters();
   {
      bml::unique_ptr<A[2]> p(new A[2]);
      BOOST_TEST(A::count == 2);
      A* i = p.get();
      ::boost::ignore_unused(i);
      p.reset(nullptr);
      BOOST_TEST(A::count == 0);
      BOOST_TEST(p.get() == 0);
   }
   BOOST_TEST(A::count == 0);
   {
      bml::unique_ptr<A[2]> p(new A[2]);
      BOOST_TEST(A::count == 2);
      A* i = p.get();
      ::boost::ignore_unused(i);
      p = nullptr;
      BOOST_TEST(A::count == 0);
      BOOST_TEST(p.get() == 0);
   }
   BOOST_TEST(A::count == 0);
   {
      bml::unique_ptr<A[2]> pi(nullptr);
      BOOST_TEST(pi.get() == nullptr);
      BOOST_TEST(pi.get() == 0);
   }
   BOOST_TEST(A::count == 0);
   {
      bml::unique_ptr<A[2]> pi(nullptr, bml::unique_ptr<A[2]>::deleter_type());
      BOOST_TEST(pi.get() == nullptr);
      BOOST_TEST(pi.get() == 0);
   }
   BOOST_TEST(A::count == 0);
   #endif
}

}  //namespace unique_ptr_nullptr{

////////////////////////////////
//             main
////////////////////////////////
int main()
{
   //nullptr
   unique_ptr_zero::test();
   unique_ptr_nullptr::test();

   //Test results
   return boost::report_errors();

}

#include "unique_ptr_test_utils_end.hpp"
