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
//   unique_ptr_asgn_move_convert_defdel
////////////////////////////////
namespace unique_ptr_asgn_move_convert_defdel {

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   bml::unique_ptr<B> s(new B);
   A* p = s.get();
   bml::unique_ptr<A> s2(new A);
   BOOST_TEST(A::count == 2);
   s2 = boost::move(s);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 1);
   BOOST_TEST(B::count == 1);
   }
   BOOST_TEST(A::count == 0);
   BOOST_TEST(B::count == 0);

   //Unbounded array unique_ptr
   reset_counters();
   {
   bml::unique_ptr<A[]> s(new A[2]);
   A* p = s.get();
   bml::unique_ptr<const A[]> s2(new const A[2]);
   BOOST_TEST(A::count == 4);
   s2 = boost::move(s);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   }
   BOOST_TEST(A::count == 0);
   //Bounded array unique_ptr
   reset_counters();
   {
   bml::unique_ptr<A[2]> s(new A[2]);
   A* p = s.get();
   bml::unique_ptr<const A[2]> s2(new const A[2]);
   BOOST_TEST(A::count == 4);
   s2 = boost::move(s);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   }
   {
   BOOST_TEST(A::count == 0);
   bml::unique_ptr<A[2]> s(new A[2]);
   A* p = s.get();
   bml::unique_ptr<const A[]> s2(new const A[2]);
   BOOST_TEST(A::count == 4);
   s2 = boost::move(s);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_ptr_asgn_move_convert_defdel{

////////////////////////////////
//   unique_ptr_asgn_move_convert_movdel
////////////////////////////////

namespace unique_ptr_asgn_move_convert_movedel{

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   bml::unique_ptr<B, move_constr_deleter<B> > s(new B);
   A* p = s.get();
   bml::unique_ptr<A, move_constr_deleter<A> > s2(new A);
   BOOST_TEST(A::count == 2);
   s2 = (boost::move(s));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 1);
   BOOST_TEST(B::count == 1);
   BOOST_TEST(s2.get_deleter().state() == 5);
   BOOST_TEST(s.get_deleter().state() == 0);
   }
   BOOST_TEST(A::count == 0);
   BOOST_TEST(B::count == 0);

   //Unbounded array unique_ptr
   reset_counters();
   {
   bml::unique_ptr<A[], move_constr_deleter<A[]> > s(new A[2]);
   A* p = s.get();
   bml::unique_ptr<const A[], move_constr_deleter<const A[]> > s2(new const A[2]);
   BOOST_TEST(A::count == 4);
   s2 = (boost::move(s));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   BOOST_TEST(s2.get_deleter().state() == 5);
   BOOST_TEST(s.get_deleter().state() == 0);
   }
   BOOST_TEST(A::count == 0);

   //Bounded array unique_ptr
   reset_counters();
   {
   bml::unique_ptr<A[2], move_constr_deleter<A[2]> > s(new A[3]);
   A* p = s.get();
   bml::unique_ptr<const A[2], move_constr_deleter<const A[2]> > s2(new const A[2]);
   BOOST_TEST(A::count == 5);
   s2 = (boost::move(s));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 3);
   BOOST_TEST(s2.get_deleter().state() == 5);
   BOOST_TEST(s.get_deleter().state() == 0);
   }
   BOOST_TEST(A::count == 0);
   reset_counters();
   {
   bml::unique_ptr<A[2], move_constr_deleter<A[3]> > s(new A[2]);
   A* p = s.get();
   bml::unique_ptr<const A[], move_constr_deleter<const A[]> > s2(new const A[2]);
   BOOST_TEST(A::count == 4);
   s2 = (boost::move(s));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   BOOST_TEST(s2.get_deleter().state() == 5);
   BOOST_TEST(s.get_deleter().state() == 0);
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_ptr_asgn_move_convert_movedel{

////////////////////////////////
//   unique_ptr_asgn_move_convert_copydelref
////////////////////////////////

namespace unique_ptr_asgn_move_convert_copydelref{

// test converting move assignment with reference deleters

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   copy_constr_deleter<B> db(5);
   bml::unique_ptr<B, copy_constr_deleter<B>&> s(new B, db);
   A* p = s.get();
   copy_constr_deleter<A> da(6);
   bml::unique_ptr<A, copy_constr_deleter<A>&> s2(new A, da);
   s2 = boost::move(s);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 1);
   BOOST_TEST(B::count == 1);
   BOOST_TEST(s2.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
   BOOST_TEST(B::count == 0);

   //Unbounded array unique_ptr
   reset_counters();
   {
   copy_constr_deleter<A[]> db(5);
   bml::unique_ptr<A[], copy_constr_deleter<A[]>&> s(new A[2], db);
   A* p = s.get();
   copy_constr_deleter<const A[]> da(6);
   bml::unique_ptr<const A[], copy_constr_deleter<const A[]>&> s2(new const A[2], da);
   BOOST_TEST(A::count == 4);
   s2 = boost::move(s);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   BOOST_TEST(s2.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);

   //Bounded array unique_ptr
   reset_counters();
   {
   copy_constr_deleter<A[2]> db(5);
   bml::unique_ptr<A[2], copy_constr_deleter<A[2]>&> s(new A[2], db);
   A* p = s.get();
   copy_constr_deleter<const A[2]> da(6);
   bml::unique_ptr<const A[2], copy_constr_deleter<const A[2]>&> s2(new const A[2], da);
   BOOST_TEST(A::count == 4);
   s2 = boost::move(s);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   BOOST_TEST(s2.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
   reset_counters();
   {
   copy_constr_deleter<A[2]> db(5);
   bml::unique_ptr<A[2], copy_constr_deleter<A[2]>&> s(new A[2], db);
   A* p = s.get();
   copy_constr_deleter<const A[]> da(6);
   bml::unique_ptr<const A[], copy_constr_deleter<const A[]>&> s2(new const A[2], da);
   BOOST_TEST(A::count == 4);
   s2 = boost::move(s);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   BOOST_TEST(s2.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_ptr_asgn_move_convert_copydelref{

////////////////////////////////
//   unique_ptr_asgn_move_defdel
////////////////////////////////
namespace unique_ptr_asgn_move_defdel {

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   bml::unique_ptr<A> s1(new A);
   A* p = s1.get();
   bml::unique_ptr<A> s2(new A);
   BOOST_TEST(A::count == 2);
   s2 = boost::move(s1);
   BOOST_TEST(A::count == 1);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s1.get() == 0);
   }
   BOOST_TEST(A::count == 0);

   //Unbounded array unique_ptr
   reset_counters();
   {
   bml::unique_ptr<A[]> s1(new A[2]);
   A* p = s1.get();
   bml::unique_ptr<A[]> s2(new A[2]);
   BOOST_TEST(A::count == 4);
   s2 = boost::move(s1);
   BOOST_TEST(A::count == 2);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s1.get() == 0);
   }
   BOOST_TEST(A::count == 0);
   //Bounded array unique_ptr
   reset_counters();
   {
   bml::unique_ptr<A[2]> s1(new A[2]);
   A* p = s1.get();
   bml::unique_ptr<A[2]> s2(new A[2]);
   BOOST_TEST(A::count == 4);
   s2 = boost::move(s1);
   BOOST_TEST(A::count == 2);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s1.get() == 0);
   }
   BOOST_TEST(A::count == 0);
}

}  //unique_ptr_asgn_move_defdel

////////////////////////////////
//   unique_ptr_asgn_move_movedel
////////////////////////////////
namespace unique_ptr_asgn_move_movedel {

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   bml::unique_ptr<A, move_constr_deleter<A> > s1(new A);
   A* p = s1.get();
   bml::unique_ptr<A, move_constr_deleter<A> > s2(new A);
   BOOST_TEST(A::count == 2);
   s2 = boost::move(s1);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s1.get() == 0);
   BOOST_TEST(A::count == 1);
   BOOST_TEST(s2.get_deleter().state() == 5);
   BOOST_TEST(s1.get_deleter().state() == 0);
   }
   BOOST_TEST(A::count == 0);

   //Unbounded array unique_ptr
   reset_counters();
   {
   bml::unique_ptr<A[], move_constr_deleter<A[]> > s1(new A[2]);
   A* p = s1.get();
   bml::unique_ptr<A[], move_constr_deleter<A[]> > s2(new A[2]);
   BOOST_TEST(A::count == 4);
   s2 = boost::move(s1);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s1.get() == 0);
   BOOST_TEST(A::count == 2);
   BOOST_TEST(s2.get_deleter().state() == 5);
   BOOST_TEST(s1.get_deleter().state() == 0);
   }
   BOOST_TEST(A::count == 0);

   BOOST_TEST(A::count == 0);
   //Bounded array unique_ptr
   reset_counters();
   {
   bml::unique_ptr<A[2], move_constr_deleter<A[2]> > s1(new A[2]);
   A* p = s1.get();
   bml::unique_ptr<A[2], move_constr_deleter<A[2]> > s2(new A[2]);
   BOOST_TEST(A::count == 4);
   s2 = boost::move(s1);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s1.get() == 0);
   BOOST_TEST(A::count == 2);
   BOOST_TEST(s2.get_deleter().state() == 5);
   BOOST_TEST(s1.get_deleter().state() == 0);
   }
   BOOST_TEST(A::count == 0);
}

}  //unique_ptr_asgn_move_movedel

////////////////////////////////
//   unique_ptr_asgn_move_copydelref
////////////////////////////////
namespace unique_ptr_asgn_move_copydelref {

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   copy_constr_deleter<A> d1(5);
   bml::unique_ptr<A, copy_constr_deleter<A>&> s1(new A, d1);
   A* p = s1.get();
   copy_constr_deleter<A> d2(6);
   bml::unique_ptr<A, copy_constr_deleter<A>&> s2(new A, d2);
   s2 = boost::move(s1);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s1.get() == 0);
   BOOST_TEST(A::count == 1);
   BOOST_TEST(d1.state() == 5);
   BOOST_TEST(d2.state() == 5);
   }
   BOOST_TEST(A::count == 0);

   //Unbounded array unique_ptr
   reset_counters();
   {
   copy_constr_deleter<A[]> d1(5);
   bml::unique_ptr<A[], copy_constr_deleter<A[]>&> s1(new A[2], d1);
   A* p = s1.get();
   copy_constr_deleter<A[]> d2(6);
   bml::unique_ptr<A[], copy_constr_deleter<A[]>&> s2(new A[2], d2);
   BOOST_TEST(A::count == 4);
   s2 = boost::move(s1);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s1.get() == 0);
   BOOST_TEST(A::count == 2);
   BOOST_TEST(d1.state() == 5);
   BOOST_TEST(d2.state() == 5);
   }
   BOOST_TEST(A::count == 0);
   //Bounded array unique_ptr
   reset_counters();
   {
   copy_constr_deleter<A[2]> d1(5);
   bml::unique_ptr<A[2], copy_constr_deleter<A[2]>&> s1(new A[2], d1);
   A* p = s1.get();
   copy_constr_deleter<A[2]> d2(6);
   bml::unique_ptr<A[2], copy_constr_deleter<A[2]>&> s2(new A[2], d2);
   BOOST_TEST(A::count == 4);
   s2 = boost::move(s1);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s1.get() == 0);
   BOOST_TEST(A::count == 2);
   BOOST_TEST(d1.state() == 5);
   BOOST_TEST(d2.state() == 5);
   }
   BOOST_TEST(A::count == 0);
}

}  //unique_ptr_asgn_move_copydelref

////////////////////////////////
//             main
////////////////////////////////
int main()
{
   //Assignment
   unique_ptr_asgn_move_convert_defdel::test();
   unique_ptr_asgn_move_convert_movedel::test();
   unique_ptr_asgn_move_convert_copydelref::test();
   unique_ptr_asgn_move_defdel::test();
   unique_ptr_asgn_move_movedel::test();
   unique_ptr_asgn_move_copydelref::test();

   //Test results
   return boost::report_errors();
}

#include "unique_ptr_test_utils_end.hpp"
