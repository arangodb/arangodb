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
#include <boost/move/default_delete.hpp>
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

////////////////////////////////
//   unique_ptr_dltr_dflt_convert_ctor
////////////////////////////////

namespace bml = ::boost::movelib;

struct A
{
   static int count;
   A() {++count;}
   A(const A&) {++count;}
   virtual ~A() {--count;}
};

int A::count = 0;

struct B
   : public A
{
   static int count;
   B() : A() {++count;}
   B(const B&) : A() {++count;}
   virtual ~B() {--count;}
};

int B::count = 0;

void reset_counters()
{  A::count = B::count = 0;  }

namespace unique_ptr_dltr_dflt_convert_ctor{

void test()
{
   //Single element deleter
   {
   reset_counters();
   bml::default_delete<B> d2;
   bml::default_delete<A> d1 = d2;
   A* p = new B;
   BOOST_TEST(A::count == 1);
   BOOST_TEST(B::count == 1);
   d1(p);
   BOOST_TEST(A::count == 0);
   BOOST_TEST(B::count == 0);
   }
   //Array element deleter
   {
   reset_counters();
   bml::default_delete<A[]> d2;
   bml::default_delete<const A[]> d1 = d2;
   const A* p = new const A[2];
   BOOST_TEST(A::count == 2);
   d1(p);
   BOOST_TEST(A::count == 0);
   }
   //Bounded array element deleter
   {
   reset_counters();
   bml::default_delete<A[2]> d2;
   bml::default_delete<const A[2]> d1 = d2;
   const A* p = new const A[2];
   BOOST_TEST(A::count == 2);
   d1(p);
   bml::default_delete<const A[]> d0 = d1;
   d0(0);
   BOOST_TEST(A::count == 0);
   }
}

}  //namespace unique_ptr_dltr_dflt_convert_ctor{

////////////////////////////////
//   unique_ptr_dltr_dflt_convert_assign
////////////////////////////////

namespace unique_ptr_dltr_dflt_convert_assign{

void test()
{
   //Single element deleter
   {
   reset_counters();
   bml::default_delete<B> d2;
   bml::default_delete<A> d1;
   d1 = d2;
   A* p = new B;
   BOOST_TEST(A::count == 1);
   BOOST_TEST(B::count == 1);
   d1(p);
   BOOST_TEST(A::count == 0);
   BOOST_TEST(B::count == 0);
   }
   //Array element deleter
   {
   reset_counters();
   bml::default_delete<A[]> d2;
   bml::default_delete<const A[]> d1;
   d1 = d2;
   const A* p = new const A[2];
   BOOST_TEST(A::count == 2);
   d1(p);
   BOOST_TEST(A::count == 0);
   }
   //Bounded array element deleter
   {
   reset_counters();
   bml::default_delete<A[2]> d2;
   bml::default_delete<const A[2]> d1;
   d1 = d2;
   const A* p = new const A[2];
   BOOST_TEST(A::count == 2);
   d1(p);
   bml::default_delete<const A[]> d0;
   d0 = d1;
   d0(0);
   BOOST_TEST(A::count == 0);
   }
}

}  //namespace unique_ptr_dltr_dflt_convert_assign{


////////////////////////////////
//   unique_ptr_dltr_dflt_default
////////////////////////////////

namespace unique_ptr_dltr_dflt_default{

void test()
{
   {
   //Single element deleter
   reset_counters();
   bml::default_delete<A> d;
   A* p = new A;
   BOOST_TEST(A::count == 1);
   d(p);
   BOOST_TEST(A::count == 0);
   }
   {
   //Array element deleter
   reset_counters();
   bml::default_delete<A[]> d;
   A* p = new A[2];
   BOOST_TEST(A::count == 2);
   d(p);
   BOOST_TEST(A::count == 0);
   }

   {
   //Bounded Array element deleter
   reset_counters();
   bml::default_delete<A[10]> d;
   A* p = new A[10];
   BOOST_TEST(A::count == 10);
   d(p);
   BOOST_TEST(A::count == 0);
   }
}

}  //namespace unique_ptr_dltr_dflt_default{

////////////////////////////////
//             main
////////////////////////////////
int main()
{
   unique_ptr_dltr_dflt_convert_ctor::test();
   unique_ptr_dltr_dflt_convert_assign::test();
   unique_ptr_dltr_dflt_default::test();

   //Test results
   return boost::report_errors();
}
