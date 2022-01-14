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
namespace bmupmu = ::boost::move_upmu;

////////////////////////////////
//   unique_ptr_ctor_move_defdel
////////////////////////////////

namespace unique_ptr_ctor_move_defdel{

// test converting move ctor.  Should only require a MoveConstructible deleter, or if
//    deleter is a reference, not even that.

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   bml::unique_ptr<A> s(new A);
   A* p = s.get();
   bml::unique_ptr<A> s2(boost::move(s));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 1);
   }
   BOOST_TEST(A::count == 0);
   //Unbounded array unique_ptr
   reset_counters();
   {
   bml::unique_ptr<A[]> s(new A[2]);
   A* p = s.get();
   bml::unique_ptr<A[]> s2(boost::move(s));
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
   bml::unique_ptr<A[2]> s2(boost::move(s));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_ptr_ctor_move_defdel{

////////////////////////////////
//   unique_ptr_ctor_move_movedel
////////////////////////////////

namespace unique_ptr_ctor_move_movedel{

// test converting move ctor.  Should only require a MoveConstructible deleter, or if
//    deleter is a reference, not even that.

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   bml::unique_ptr<A, move_constr_deleter<A> > s(new A);
   A* p = s.get();
   bml::unique_ptr<A, move_constr_deleter<A> > s2 = boost::move(s);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 1);
   BOOST_TEST(s2.get_deleter().state() == 5);
   BOOST_TEST(s.get_deleter().state() == 0);
   }
   BOOST_TEST(A::count == 0);
   //Unbounded array unique_ptr
   reset_counters();
   {
   bml::unique_ptr<A[], move_constr_deleter<A[]> > s(new A[2]);
   A* p = s.get();
   bml::unique_ptr<A[], move_constr_deleter<A[]> > s2 = boost::move(s);
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
   bml::unique_ptr<A[2]> s(new A[2]);
   A* p = s.get();
   bml::unique_ptr<A[2]> s2 = boost::move(s);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_ptr_ctor_move_movedel{

////////////////////////////////
//   unique_ptr_ctor_move_dfctrdelref
////////////////////////////////

namespace unique_ptr_ctor_move_dfctrdelref{

// test converting move ctor.  Should only require a MoveConstructible deleter, or if
//    deleter is a reference, not even that.

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   def_constr_deleter<A> d;
   bml::unique_ptr<A, def_constr_deleter<A>&> s(new A, d);
   A* p = s.get();
   bml::unique_ptr<A, def_constr_deleter<A>&> s2 = boost::move(s);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 1);
   d.set_state(6);
   BOOST_TEST(s2.get_deleter().state() == d.state());
   BOOST_TEST(s.get_deleter().state() ==  d.state());
   }
   BOOST_TEST(A::count == 0);
   //Unbounded array unique_ptr
   reset_counters();
   {
   def_constr_deleter<A[]> d;
   bml::unique_ptr<A[], def_constr_deleter<A[]>&> s(new A[2], d);
   A* p = s.get();
   bml::unique_ptr<A[], def_constr_deleter<A[]>&> s2 = boost::move(s);
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   d.set_state(6);
   BOOST_TEST(s2.get_deleter().state() == d.state());
   BOOST_TEST(s.get_deleter().state() ==  d.state());
   }
   BOOST_TEST(A::count == 0);
   //Bounded array unique_ptr
   reset_counters();
   {
   def_constr_deleter<A[2]> d;
   bml::unique_ptr<A[2], def_constr_deleter<A[2]>&> s(new A[2], d);
   A* p = s.get();
   bml::unique_ptr<A[2], def_constr_deleter<A[2]>&> s2(boost::move(s));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   d.set_state(6);
   BOOST_TEST(s2.get_deleter().state() == d.state());
   BOOST_TEST(s.get_deleter().state() ==  d.state());
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_ptr_ctor_move_dfctrdelref{

////////////////////////////////
//   unique_ptr_ctor_move_convert_defdel
////////////////////////////////

namespace unique_ptr_ctor_move_convert_defdel{

// test converting move ctor.  Should only require a MoveConstructible deleter, or if
//    deleter is a reference, not even that.

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   bml::unique_ptr<B> s(new B);
   A* p = s.get();
   bml::unique_ptr<A> s2(boost::move(s));
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
   bml::unique_ptr<const volatile A[]> s2(boost::move(s));
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
   bml::unique_ptr<const volatile A[2]> s2(boost::move(s));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   }
   BOOST_TEST(A::count == 0);
   {
   bml::unique_ptr<A[2]> s(new A[2]);
   A* p = s.get();
   bml::unique_ptr<const volatile A[]> s2(boost::move(s));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_ptr_ctor_move_convert_defdel{

////////////////////////////////
//   unique_ptr_ctor_move_convert_movedel
////////////////////////////////

namespace unique_ptr_ctor_move_convert_movedel{

// test converting move ctor.  Should only require a MoveConstructible deleter, or if
//    deleter is a reference, not even that.

void test()
{
   //Single unique_ptr
   reset_counters();
   BOOST_STATIC_ASSERT((bmupmu::is_convertible<B, A>::value));
   {
   bml::unique_ptr<B, move_constr_deleter<B> > s(new B);
   A* p = s.get();
   bml::unique_ptr<A, move_constr_deleter<A> > s2(boost::move(s));
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
   bml::unique_ptr<const A[], move_constr_deleter<const A[]> > s(new const A[2]);
   const A* p = s.get();
   bml::unique_ptr<const volatile A[], move_constr_deleter<const volatile A[]> > s2(boost::move(s));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   BOOST_TEST(s2.get_deleter().state() == 5);
   BOOST_TEST(s.get_deleter().state() == 0);
   }
   BOOST_TEST(A::count == 0);
   BOOST_TEST(B::count == 0);
   //Bounded array unique_ptr
   reset_counters();
   {
   bml::unique_ptr<const A[2], move_constr_deleter<const A[2]> > s(new const A[2]);
   const A* p = s.get();
   bml::unique_ptr<const volatile A[2], move_constr_deleter<const volatile A[2]> > s2(boost::move(s));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   BOOST_TEST(s2.get_deleter().state() == 5);
   BOOST_TEST(s.get_deleter().state() == 0);
   }
   BOOST_TEST(A::count == 0);
   BOOST_TEST(B::count == 0);
   {
   bml::unique_ptr<const A[2], move_constr_deleter<const A[2]> > s(new const A[2]);
   const A* p = s.get();
   bml::unique_ptr<const volatile A[], move_constr_deleter<const volatile A[]> > s2(boost::move(s));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   BOOST_TEST(s2.get_deleter().state() == 5);
   BOOST_TEST(s.get_deleter().state() == 0);
   }
   BOOST_TEST(A::count == 0);
   BOOST_TEST(B::count == 0);
}

}  //namespace unique_ptr_ctor_move_convert_movedel{

////////////////////////////////
//   unique_ptr_ctor_move_convert_dfctrdelref
////////////////////////////////

namespace unique_ptr_ctor_move_convert_dfctrdelref{

// test converting move ctor.  Should only require a MoveConstructible deleter, or if
//    deleter is a reference, not even that.

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   def_constr_deleter<A> d;
   bml::unique_ptr<B, def_constr_deleter<A>&> s(new B, d);
   A* p = s.get();
   bml::unique_ptr<A, def_constr_deleter<A>&> s2(boost::move(s));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 1);
   BOOST_TEST(B::count == 1);
   d.set_state(6);
   BOOST_TEST(s2.get_deleter().state() == d.state());
   BOOST_TEST(s.get_deleter().state() ==  d.state());
   }
   BOOST_TEST(A::count == 0);
   BOOST_TEST(B::count == 0);
   //Unbounded array unique_ptr
   reset_counters();
   {
   def_constr_deleter<volatile A[]> d;
   bml::unique_ptr<A[], def_constr_deleter<volatile A[]>&> s(new A[2], d);
   A* p = s.get();
   bml::unique_ptr<volatile A[], def_constr_deleter<volatile A[]>&> s2(boost::move(s));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   d.set_state(6);
   BOOST_TEST(s2.get_deleter().state() == d.state());
   BOOST_TEST(s.get_deleter().state() ==  d.state());
   }
   BOOST_TEST(A::count == 0);
   //Bounded array unique_ptr
   reset_counters();
   {
   def_constr_deleter<volatile A[2]> d;
   bml::unique_ptr<A[2], def_constr_deleter<volatile A[2]>&> s(new A[2], d);
   A* p = s.get();
   bml::unique_ptr<volatile A[2], def_constr_deleter<volatile A[2]>&> s2(boost::move(s));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   d.set_state(6);
   BOOST_TEST(s2.get_deleter().state() == d.state());
   BOOST_TEST(s.get_deleter().state() ==  d.state());
   }
   BOOST_TEST(A::count == 0);
   {
   def_constr_deleter<volatile A[]> d;
   bml::unique_ptr<A[2], def_constr_deleter<volatile A[]>&> s(new A[2], d);
   A* p = s.get();
   bml::unique_ptr<volatile A[], def_constr_deleter<volatile A[]>&> s2(boost::move(s));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(A::count == 2);
   d.set_state(6);
   BOOST_TEST(s2.get_deleter().state() == d.state());
   BOOST_TEST(s.get_deleter().state() ==  d.state());
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_ptr_ctor_move_convert_dfctrdelref{

////////////////////////////////
//   unique_ptr_ctor_move_sourcesink
////////////////////////////////

namespace unique_ptr_ctor_move_sourcesink{

// test move ctor.  Should only require a MoveConstructible deleter, or if
//    deleter is a reference, not even that.

bml::unique_ptr<A> source1()
{  return bml::unique_ptr<A>(new A);  }

bml::unique_ptr<A[]> source1_unbounded_array()
{  return bml::unique_ptr<A[]> (new A[2]);  }

bml::unique_ptr<A[2]> source1_bounded_array()
{  return bml::unique_ptr<A[2]> (new A[2]);  }

void sink1(bml::unique_ptr<A>)
{}

void sink1_unbounded_array(bml::unique_ptr<A[]>)
{}

void sink1_bounded_array(bml::unique_ptr<A[2]>)
{}

bml::unique_ptr<A, move_constr_deleter<A> > source2()
{  return bml::unique_ptr<A, move_constr_deleter<A> > (new A);  }

bml::unique_ptr<A[], move_constr_deleter<A[]> > source2_unbounded_array()
{  return bml::unique_ptr<A[], move_constr_deleter<A[]> >(new A[2]);  }

bml::unique_ptr<A[2], move_constr_deleter<A[2]> > source2_bounded_array()
{  return bml::unique_ptr<A[2], move_constr_deleter<A[2]> >(new A[2]);  }

void sink2(bml::unique_ptr<A, move_constr_deleter<A> >)
{}

void sink2_unbounded_array(bml::unique_ptr<A[], move_constr_deleter<A[]> >)
{}

void sink2_bounded_array(bml::unique_ptr<A[2], move_constr_deleter<A[2]> >)
{}

bml::unique_ptr<A, def_constr_deleter<A>&> source3()
{
   static def_constr_deleter<A> d;
   return bml::unique_ptr<A, def_constr_deleter<A>&>(new A, d);
}

bml::unique_ptr<A[], def_constr_deleter<A[]>&> source3_unbounded_array()
{
   static def_constr_deleter<A[]> d;
   return bml::unique_ptr<A[], def_constr_deleter<A[]>&>(new A[2], d);
}

bml::unique_ptr<A[2], def_constr_deleter<A[2]>&> source3_bounded_array()
{
   static def_constr_deleter<A[2]> d;
   return bml::unique_ptr<A[2], def_constr_deleter<A[2]>&>(new A[2], d);
}

void sink3(bml::unique_ptr<A, def_constr_deleter<A>&> )
{}

void sink3_unbounded_array(bml::unique_ptr<A[], def_constr_deleter<A[]>&> )
{}

void sink3_bounded_array(bml::unique_ptr<A[2], def_constr_deleter<A[2]>&> )
{}

void test()
{
   //Single unique_ptr
   reset_counters(); 
   sink1(source1());
   sink2(source2());
   sink3(source3());
   BOOST_TEST(A::count == 0);
   //Unbounded array unique_ptr
   reset_counters();
   sink1_unbounded_array(source1_unbounded_array());
   sink2_unbounded_array(source2_unbounded_array());
   sink3_unbounded_array(source3_unbounded_array());
   BOOST_TEST(A::count == 0);
   //Bbounded array unique_ptr
   reset_counters();
   sink1_bounded_array(source1_bounded_array());
   sink2_bounded_array(source2_bounded_array());
   sink3_bounded_array(source3_bounded_array());
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_ptr_ctor_move_sourcesink{

////////////////////////////////
//             main
////////////////////////////////
int main()
{
   //Move Constructor
   unique_ptr_ctor_move_defdel::test();
   unique_ptr_ctor_move_movedel::test();
   unique_ptr_ctor_move_dfctrdelref::test();
   unique_ptr_ctor_move_convert_defdel::test();
   unique_ptr_ctor_move_convert_movedel::test();
   unique_ptr_ctor_move_convert_dfctrdelref::test();
   unique_ptr_ctor_move_sourcesink::test();

   //Test results
   return boost::report_errors();
}

#include "unique_ptr_test_utils_end.hpp"
