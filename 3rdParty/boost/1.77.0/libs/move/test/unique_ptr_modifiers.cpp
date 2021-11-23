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
#include <boost/move/adl_move_swap.hpp>
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
//   unique_ptr_modifiers_release
////////////////////////////////

namespace unique_ptr_modifiers_release{

void test()
{
   //Single unique_ptr
   {
   bml::unique_ptr<int> p(new int(3));
   int* i = p.get();
   int* j = p.release();
   BOOST_TEST(p.get() == 0);
   BOOST_TEST(i == j);
   p.reset(j);
   }
   //Unbounded array unique_ptr
   {
   bml::unique_ptr<int[]> p(new int[2]);
   int* i = p.get();
   int* j = p.release();
   BOOST_TEST(p.get() == 0);
   BOOST_TEST(i == j);
   p.reset(j);
   }
   //Bounded array unique_ptr
   {
   bml::unique_ptr<int[2]> p(new int[2]);
   int* i = p.get();
   int* j = p.release();
   BOOST_TEST(p.get() == 0);
   BOOST_TEST(i == j);
   p.reset(j);
   }
}

}  //namespace unique_ptr_modifiers_release{

////////////////////////////////
//   unique_ptr_modifiers_reset
////////////////////////////////

namespace unique_ptr_modifiers_reset{

void test()
{
   //Single unique_ptr
   {
      reset_counters();
      {  //reset()
      bml::unique_ptr<A> p(new A);
      BOOST_TEST(A::count == 1);
      A* i = p.get();
      ::boost::ignore_unused(i);
      p.reset();
      BOOST_TEST(A::count == 0);
      BOOST_TEST(p.get() == 0);
      }
      BOOST_TEST(A::count == 0);
      {  //reset(p)
      bml::unique_ptr<A> p(new A);
      BOOST_TEST(A::count == 1);
      A* i = p.get();
      ::boost::ignore_unused(i);
      p.reset(new A);
      BOOST_TEST(A::count == 1);
      }
      BOOST_TEST(A::count == 0);
      {  //reset(0)
      bml::unique_ptr<A> p(new A);
      BOOST_TEST(A::count == 1);
      A* i = p.get();
      ::boost::ignore_unused(i);
      p.reset(0);
      BOOST_TEST(A::count == 0);
      BOOST_TEST(p.get() == 0);
      }
      BOOST_TEST(A::count == 0);
   }
   //Unbounded array unique_ptr
   {
      reset_counters();
      {  //reset()
      bml::unique_ptr<A[]> p(new A[2]);
      BOOST_TEST(A::count == 2);
      A* i = p.get();
      ::boost::ignore_unused(i);
      p.reset();
      BOOST_TEST(A::count == 0);
      BOOST_TEST(p.get() == 0);
      }
      BOOST_TEST(A::count == 0);
      {  //reset(p)
      bml::unique_ptr<A[]> p(new A[2]);
      BOOST_TEST(A::count == 2);
      A* i = p.get();
      ::boost::ignore_unused(i);
      p.reset(new A[3]);
      BOOST_TEST(A::count == 3);
      }
      BOOST_TEST(A::count == 0);
      {  //reset(0)
      bml::unique_ptr<A[]> p(new A[2]);
      BOOST_TEST(A::count == 2);
      A* i = p.get();
      ::boost::ignore_unused(i);
      p.reset(0);
      BOOST_TEST(A::count == 0);
      BOOST_TEST(p.get() == 0);
      }
      BOOST_TEST(A::count == 0);
   }
   {
      //Bounded array unique_ptr
      reset_counters();
      {  //reset()
      bml::unique_ptr<A[2]> p(new A[2]);
      BOOST_TEST(A::count == 2);
      A* i = p.get();
      ::boost::ignore_unused(i);
      p.reset();
      BOOST_TEST(A::count == 0);
      BOOST_TEST(p.get() == 0);
      }
      BOOST_TEST(A::count == 0);
      {  //reset(p)
      bml::unique_ptr<A[2]> p(new A[2]);
      BOOST_TEST(A::count == 2);
      A* i = p.get();
      ::boost::ignore_unused(i);
      p.reset(new A[3]);
      BOOST_TEST(A::count == 3);
      }
      BOOST_TEST(A::count == 0);
      {  //reset(0)
      bml::unique_ptr<A[2]> p(new A[2]);
      BOOST_TEST(A::count == 2);
      A* i = p.get();
      ::boost::ignore_unused(i);
      p.reset(0);
      BOOST_TEST(A::count == 0);
      BOOST_TEST(p.get() == 0);
      }
      BOOST_TEST(A::count == 0);
   }
}

}  //namespace unique_ptr_modifiers_reset{

////////////////////////////////
//   unique_ptr_modifiers_reset_convert
////////////////////////////////

namespace unique_ptr_modifiers_reset_convert{

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   bml::unique_ptr<A> p(new A);
   BOOST_TEST(A::count == 1);
   BOOST_TEST(B::count == 0);
   A* i = p.get();
   ::boost::ignore_unused(i);
   p.reset(new B);
   BOOST_TEST(A::count == 1);
   BOOST_TEST(B::count == 1);
   }
   BOOST_TEST(A::count == 0);
   BOOST_TEST(B::count == 0);
   {
   bml::unique_ptr<A> p(new B);
   BOOST_TEST(A::count == 1);
   BOOST_TEST(B::count == 1);
   A* i = p.get();
   ::boost::ignore_unused(i);
   p.reset(new B);
   BOOST_TEST(A::count == 1);
   BOOST_TEST(B::count == 1);
   }
   BOOST_TEST(A::count == 0);
   BOOST_TEST(B::count == 0);
   //Unbounded array unique_ptr
   reset_counters();
   {
   bml::unique_ptr<const volatile A[2]> p(new const A[2]);
   BOOST_TEST(A::count == 2);
   const volatile A* i = p.get();
   ::boost::ignore_unused(i);
   p.reset(new volatile A[3]);
   BOOST_TEST(A::count == 3);
   }
   BOOST_TEST(A::count == 0);
   {
   bml::unique_ptr<const A[2]> p(new A[2]);
   BOOST_TEST(A::count == 2);
   const A* i = p.get();
   ::boost::ignore_unused(i);
   p.reset(new const A[3]);
   BOOST_TEST(A::count == 3);
   }
   BOOST_TEST(A::count == 0);
   //Bounded array unique_ptr
   reset_counters();
   {
   bml::unique_ptr<const volatile A[2]> p(new const A[2]);
   BOOST_TEST(A::count == 2);
   const volatile A* i = p.get();
   ::boost::ignore_unused(i);
   p.reset(new volatile A[3]);
   BOOST_TEST(A::count == 3);
   }
   BOOST_TEST(A::count == 0);
   {
   bml::unique_ptr<const A[2]> p(new A[2]);
   BOOST_TEST(A::count == 2);
   const A* i = p.get();
   ::boost::ignore_unused(i);
   p.reset(new const A[3]);
   BOOST_TEST(A::count == 3);
   }
   BOOST_TEST(A::count == 0);
}

}  //unique_ptr_modifiers_reset_convert


////////////////////////////////
//   unique_ptr_modifiers
////////////////////////////////

namespace unique_ptr_modifiers_swap{

// test swap

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   A* p1 = new A(1);
   move_constr_deleter<A> d1(1);
   bml::unique_ptr<A, move_constr_deleter<A> > s1(p1, ::boost::move(d1));
   A* p2 = new A(2);
   move_constr_deleter<A> d2(2);
   bml::unique_ptr<A, move_constr_deleter<A> > s2(p2, ::boost::move(d2));
   BOOST_TEST(s1.get() == p1);
   BOOST_TEST(*s1 == A(1));
   BOOST_TEST(s1.get_deleter().state() == 1);
   BOOST_TEST(s2.get() == p2);
   BOOST_TEST(*s2 == A(2));
   BOOST_TEST(s2.get_deleter().state() == 2);
   boost::adl_move_swap(s1, s2);
   BOOST_TEST(s1.get() == p2);
   BOOST_TEST(*s1 == A(2));
   BOOST_TEST(s1.get_deleter().state() == 2);
   BOOST_TEST(s2.get() == p1);
   BOOST_TEST(*s2 == A(1));
   BOOST_TEST(s2.get_deleter().state() == 1);
   }
   //Unbounded array unique_ptr
   reset_counters();
   {
   A* p1 = new A[2];
   p1[0].set(1);
   p1[1].set(2);
   move_constr_deleter<A[]> d1(1);
   bml::unique_ptr<A[], move_constr_deleter<A[]> > s1(p1, ::boost::move(d1));
   A* p2 = new A[2];
   p2[0].set(3);
   p2[1].set(4);
   move_constr_deleter<A[]> d2(2);
   bml::unique_ptr<A[], move_constr_deleter<A[]> > s2(p2, ::boost::move(d2));
   BOOST_TEST(s1.get() == p1);
   BOOST_TEST(s1[0] == A(1));
   BOOST_TEST(s1[1] == A(2));
   BOOST_TEST(s1.get_deleter().state() == 1);
   BOOST_TEST(s2.get() == p2);
   BOOST_TEST(s2[0] == A(3));
   BOOST_TEST(s2[1] == A(4));
   BOOST_TEST(s2.get_deleter().state() == 2);
   swap(s1, s2);
   BOOST_TEST(s1.get() == p2);
   BOOST_TEST(s1[0] == A(3));
   BOOST_TEST(s1[1] == A(4));
   BOOST_TEST(s1.get_deleter().state() == 2);
   BOOST_TEST(s2.get() == p1);
   BOOST_TEST(s2[0] == A(1));
   BOOST_TEST(s2[1] == A(2));
   BOOST_TEST(s2.get_deleter().state() == 1);
   }
   //Bounded array unique_ptr
   reset_counters();
   {
   A* p1 = new A[2];
   p1[0].set(1);
   p1[1].set(2);
   move_constr_deleter<A[2]> d1(1);
   bml::unique_ptr<A[2], move_constr_deleter<A[2]> > s1(p1, ::boost::move(d1));
   A* p2 = new A[2];
   p2[0].set(3);
   p2[1].set(4);
   move_constr_deleter<A[2]> d2(2);
   bml::unique_ptr<A[2], move_constr_deleter<A[2]> > s2(p2, ::boost::move(d2));
   BOOST_TEST(s1.get() == p1);
   BOOST_TEST(s1[0] == A(1));
   BOOST_TEST(s1[1] == A(2));
   BOOST_TEST(s1.get_deleter().state() == 1);
   BOOST_TEST(s2.get() == p2);
   BOOST_TEST(s2[0] == A(3));
   BOOST_TEST(s2[1] == A(4));
   BOOST_TEST(s2.get_deleter().state() == 2);
   swap(s1, s2);
   BOOST_TEST(s1.get() == p2);
   BOOST_TEST(s1[0] == A(3));
   BOOST_TEST(s1[1] == A(4));
   BOOST_TEST(s1.get_deleter().state() == 2);
   BOOST_TEST(s2.get() == p1);
   BOOST_TEST(s2[0] == A(1));
   BOOST_TEST(s2[1] == A(2));
   BOOST_TEST(s2.get_deleter().state() == 1);
   }
}

}  //namespace unique_ptr_modifiers_swap{

////////////////////////////////
//             main
////////////////////////////////
int main()
{
   //Modifiers
   unique_ptr_modifiers_release::test();
   unique_ptr_modifiers_reset::test();
   unique_ptr_modifiers_reset_convert::test();
   unique_ptr_modifiers_swap::test();

   //Test results
   return boost::report_errors();

}

#include "unique_ptr_test_utils_end.hpp"
