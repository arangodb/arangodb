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
//   unique_ptr_dtor_null
////////////////////////////////

namespace unique_ptr_dtor_null{

// The deleter is not called if get() == 0

void test()
{
   //Single unique_ptr
   {
   def_constr_deleter<int> d;
   BOOST_TEST(d.state() == 5);
   {
   bml::unique_ptr<int, def_constr_deleter<int>&> p(0, d);
   BOOST_TEST(p.get() == 0);
   BOOST_TEST(&p.get_deleter() == &d);
   }
   BOOST_TEST(d.state() == 5);
   }
   {
   //Unbounded array unique_ptr
   def_constr_deleter<int[]> d;
   BOOST_TEST(d.state() == 5);
   {
   bml::unique_ptr<int[], def_constr_deleter<int[]>&> p(0, d);
   BOOST_TEST(p.get() == 0);
   BOOST_TEST(&p.get_deleter() == &d);
   }
   BOOST_TEST(d.state() == 5);
   }
   {
   //Bounded array unique_ptr
   def_constr_deleter<int[2]> d;
   BOOST_TEST(d.state() == 5);
   {
   bml::unique_ptr<int[2], def_constr_deleter<int[2]>&> p(0, d);
   BOOST_TEST(p.get() == 0);
   BOOST_TEST(&p.get_deleter() == &d);
   }
   BOOST_TEST(d.state() == 5);
   }
}

}  //namespace unique_ptr_dtor_null{

////////////////////////////////
//   unique_ptr_ctor_default_delreq
////////////////////////////////

namespace unique_ptr_ctor_default_delreq{

// default unique_ptr ctor should only require default deleter ctor

void test()
{
   //Single unique_ptr
   {
   bml::unique_ptr<int> p;
   BOOST_TEST(p.get() == 0);
   }
   {
   bml::unique_ptr<int, def_constr_deleter<int> > p;
   BOOST_TEST(p.get() == 0);
   BOOST_TEST(p.get_deleter().state() == 5);
   }
   //Unbounded array unique_ptr
   {
   bml::unique_ptr<int[]> p;
   BOOST_TEST(p.get() == 0);
   }
   {
   bml::unique_ptr<int[], def_constr_deleter<int[]> > p;
   BOOST_TEST(p.get() == 0);
   BOOST_TEST(p.get_deleter().state() == 5);
   }

   //Unbounded array unique_ptr
   {
   bml::unique_ptr<int[]> p;
   BOOST_TEST(p.get() == 0);
   }
   {
   bml::unique_ptr<int[], def_constr_deleter<int[]> > p;
   BOOST_TEST(p.get() == 0);
   BOOST_TEST(p.get_deleter().state() == 5);
   }

   //Unbounded array unique_ptr
   {
   bml::unique_ptr<int[2]> p;
   BOOST_TEST(p.get() == 0);
   }
   {
   bml::unique_ptr<int[2], def_constr_deleter<int[2]> > p;
   BOOST_TEST(p.get() == 0);
   BOOST_TEST(p.get_deleter().state() == 5);
   }
}

}  //namespace unique_ptr_ctor_default_delreq{

////////////////////////////////
//   unique_ptr_ctor_default_nocomplete
////////////////////////////////

namespace unique_ptr_ctor_default_nocomplete{

// default unique_ptr ctor shouldn't require complete type

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   J<I> s;
   BOOST_TEST(s.get() == 0);
   }
   check(0);
   {
   J<I, def_constr_deleter<I> > s;
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   check(0);
   //Unbounded array unique_ptr
   reset_counters();
   {
   J<I[]> s;
   BOOST_TEST(s.get() == 0);
   }
   check(0);
   {
   J<I[], def_constr_deleter<I[]> > s;
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   check(0);

   //Bounded array unique_ptr
   reset_counters();
   {
   J<I[2]> s;
   BOOST_TEST(s.get() == 0);
   }
   check(0);
   {
   J<I[2], def_constr_deleter<I[2]> > s;
   BOOST_TEST(s.get() == 0);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   check(0);
}

}  //namespace unique_ptr_ctor_default_nocomplete{

////////////////////////////////
//   unique_ptr_ctor_pointer_delreq
////////////////////////////////

namespace unique_ptr_ctor_pointer_delreq{

// unique_ptr(pointer) ctor should only require default deleter ctor

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   A* p = new A;
   BOOST_TEST(A::count == 1);
   bml::unique_ptr<A> s(p);
   BOOST_TEST(s.get() == p);
   }
   BOOST_TEST(A::count == 0);
   {
   A* p = new A;
   BOOST_TEST(A::count == 1);
   bml::unique_ptr<A, def_constr_deleter<A> > s(p);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
   //Unbounded array unique_ptr
   reset_counters();
   {
   A* p = new A[2];
   BOOST_TEST(A::count == 2);
   bml::unique_ptr<A[]> s(p);
   BOOST_TEST(s.get() == p);
   }
   BOOST_TEST(A::count == 0);
   {
   A* p = new A[2];
   BOOST_TEST(A::count == 2);
   bml::unique_ptr<A[], def_constr_deleter<A[]> > s(p);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
   //Bounded array unique_ptr
   reset_counters();
   {
   A* p = new A[2];
   BOOST_TEST(A::count == 2);
   bml::unique_ptr<A[2]> s(p);
   BOOST_TEST(s.get() == p);
   }
   BOOST_TEST(A::count == 0);
   {
   A* p = new A[2];
   BOOST_TEST(A::count == 2);
   bml::unique_ptr<A[2], def_constr_deleter<A[2]> > s(p);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_ptr_ctor_pointer_delreq{

////////////////////////////////
//   unique_ptr_ctor_pointer_nocomplete
////////////////////////////////

namespace unique_ptr_ctor_pointer_nocomplete{

// unique_ptr(pointer) ctor shouldn't require complete type

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   I* p = get();
   check(1);
   J<I> s(p);
   BOOST_TEST(s.get() == p);
   }
   check(0);
   {
   I* p = get();
   check(1);
   J<I, def_constr_deleter<I> > s(p);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   check(0);
   //Unbounded array unique_ptr
   reset_counters();
   {
   I* p = get_array(2);
   check(2);
   J<I[]> s(p);
   BOOST_TEST(s.get() == p);
   }
   check(0);
   {
   I* p = get_array(2);
   check(2);
   J<I[], def_constr_deleter<I[]> > s(p);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   check(0);
   //Bounded array unique_ptr
   reset_counters();
   {
   I* p = get_array(2);
   check(2);
   J<I[]> s(p);
   BOOST_TEST(s.get() == p);
   }
   check(0);
   {
   I* p = get_array(2);
   check(2);
   J<I[2], def_constr_deleter<I[2]> > s(p);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   check(0);
}

}  //namespace unique_ptr_ctor_pointer_nocomplete{

////////////////////////////////
//   unique_ptr_ctor_pointer_convert
////////////////////////////////

namespace unique_ptr_ctor_pointer_convert{

// unique_ptr(pointer) ctor should work with derived pointers
// or same types (cv aside) for unique_ptr<arrays>

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   B* p = new B;
   BOOST_TEST(A::count == 1);
   BOOST_TEST(B::count == 1);
   bml::unique_ptr<A> s(p);
   BOOST_TEST(s.get() == p);
   }
   BOOST_TEST(A::count == 0);
   BOOST_TEST(B::count == 0);
   {
   B* p = new B;
   BOOST_TEST(A::count == 1);
   BOOST_TEST(B::count == 1);
   bml::unique_ptr<A, def_constr_deleter<A> > s(p);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
   BOOST_TEST(B::count == 0);
   //Unbounded array unique_ptr
   reset_counters();
   {
   A* p = new A[2];
   BOOST_TEST(A::count == 2);
   bml::unique_ptr<const A[]> s(p);
   BOOST_TEST(s.get() == p);
   }
   BOOST_TEST(A::count == 0);
   {
   const A* p = new const A[2];
   BOOST_TEST(A::count == 2);
   bml::unique_ptr<const volatile A[], def_constr_deleter<const volatile A[]> > s(p);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
   //Bounded array unique_ptr
   reset_counters();
   {
   A* p = new A[2];
   BOOST_TEST(A::count == 2);
   bml::unique_ptr<const A[2]> s(p);
   BOOST_TEST(s.get() == p);
   }
   BOOST_TEST(A::count == 0);
   {
   const A* p = new const A[2];
   BOOST_TEST(A::count == 2);
   bml::unique_ptr<const volatile A[2], def_constr_deleter<const volatile A[2]> > s(p);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_ptr_ctor_pointer_convert{

////////////////////////////////
//   unique_ptr_ctor_pointer_deleter_movedel
////////////////////////////////

namespace unique_ptr_ctor_pointer_deleter_movedel{

// test move ctor.  Should only require a MoveConstructible deleter, or if
//    deleter is a reference, not even that.

// unique_ptr(pointer, deleter()) only requires MoveConstructible deleter

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   A* p = new A;
   BOOST_TEST(A::count == 1);
   move_constr_deleter<A> d;
   bml::unique_ptr<A, move_constr_deleter<A> > s(p, ::boost::move(d));
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   bml::unique_ptr<A, move_constr_deleter<A> > s2(s.release(), move_constr_deleter<A>(6));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s2.get_deleter().state() == 6);
   }
   BOOST_TEST(A::count == 0);
   //Unbounded array unique_ptr
   reset_counters();
   {
   A* p = new A[2];
   BOOST_TEST(A::count == 2);
   move_constr_deleter<A[]> d;
   bml::unique_ptr<A[], move_constr_deleter<A[]> > s(p, ::boost::move(d));
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   bml::unique_ptr<A[], move_constr_deleter<A[]> > s2(s.release(), move_constr_deleter<A[]>(6));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s2.get_deleter().state() == 6);
   }
   BOOST_TEST(A::count == 0);
   //Bounded array unique_ptr
   reset_counters();
   {
   A* p = new A[2];
   BOOST_TEST(A::count == 2);
   move_constr_deleter<A[2]> d;
   bml::unique_ptr<A[2], move_constr_deleter<A[2]> > s(p, ::boost::move(d));
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   bml::unique_ptr<A[2], move_constr_deleter<A[2]> > s2(s.release(), move_constr_deleter<A[2]>(6));
   BOOST_TEST(s2.get() == p);
   BOOST_TEST(s2.get_deleter().state() == 6);
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_ptr_ctor_pointer_deleter_movedel{

////////////////////////////////
//   unique_ptr_ctor_pointer_deleter_copydel
////////////////////////////////

namespace unique_ptr_ctor_pointer_deleter_copydel{

// unique_ptr(pointer, d) requires CopyConstructible deleter

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   A* p = new A;
   BOOST_TEST(A::count == 1);
   copy_constr_deleter<A> d;
   bml::unique_ptr<A, copy_constr_deleter<A> > s(p, d);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   d.set_state(6);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
   //Unbounded array unique_ptr
   reset_counters();
   {
   A* p = new A[2];
   BOOST_TEST(A::count == 2);
   copy_constr_deleter<A[]> d;
   bml::unique_ptr<A[], copy_constr_deleter<A[]> > s(p, d);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   d.set_state(6);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
   //Bounded array unique_ptr
   reset_counters();
   {
   A* p = new A[2];
   BOOST_TEST(A::count == 2);
   copy_constr_deleter<A[2]> d;
   bml::unique_ptr<A[2], copy_constr_deleter<A[2]> > s(p, d);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   d.set_state(6);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_ptr_ctor_pointer_deleter_copydel{

////////////////////////////////
//   unique_ptr_ctor_pointer_deleter_dfctrdelref
////////////////////////////////

namespace unique_ptr_ctor_pointer_deleter_dfctrdelref{

// unique_ptr<T, D&>(pointer, d) does not requires CopyConstructible deleter

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   A* p = new A;
   BOOST_TEST(A::count == 1);
   def_constr_deleter<A> d;
   bml::unique_ptr<A, def_constr_deleter<A>&> s(p, d);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   d.set_state(6);
   BOOST_TEST(s.get_deleter().state() == 6);
   }
   BOOST_TEST(A::count == 0);
   //Unbounded array unique_ptr
   reset_counters();
   {
   A* p = new A[2];
   BOOST_TEST(A::count == 2);
   def_constr_deleter<A[]> d;
   bml::unique_ptr<A[], def_constr_deleter<A[]>&> s(p, d);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   d.set_state(6);
   BOOST_TEST(s.get_deleter().state() == 6);
   }
   BOOST_TEST(A::count == 0);
   //Bounded array unique_ptr
   reset_counters();
   {
   A* p = new A[2];
   BOOST_TEST(A::count == 2);
   def_constr_deleter<A[2]> d;
   bml::unique_ptr<A[2], def_constr_deleter<A[2]>&> s(p, d);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   d.set_state(6);
   BOOST_TEST(s.get_deleter().state() == 6);
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_ptr_ctor_pointer_deleter_dfctrdelref{

////////////////////////////////
//   unique_ptr_ctor_pointer_deleter_dfctrdelconstref
////////////////////////////////

namespace unique_ptr_ctor_pointer_deleter_dfctrdelconstref{

// unique_ptr<T, const D&>(pointer, d) does not requires CopyConstructible deleter

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   A* p = new A;
   BOOST_TEST(A::count == 1);
   def_constr_deleter<A> d;
   bml::unique_ptr<A, const def_constr_deleter<A>&> s(p, d);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
   //Unbounded array unique_ptr
   reset_counters();
   {
   A* p = new A[2];
   BOOST_TEST(A::count == 2);
   def_constr_deleter<A[]> d;
   bml::unique_ptr<A[], const def_constr_deleter<A[]>&> s(p, d);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
   //Bounded array unique_ptr
   reset_counters();
   {
   A* p = new A[2];
   BOOST_TEST(A::count == 2);
   def_constr_deleter<A[2]> d;
   bml::unique_ptr<A[2], const def_constr_deleter<A[2]>&> s(p, d);
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_ptr_ctor_pointer_deleter_dfctrdelconstref{

////////////////////////////////
//   unique_ptr_ctor_pointer_deleter_convert
////////////////////////////////

namespace unique_ptr_ctor_pointer_deleter_convert{

// unique_ptr(pointer, deleter) should work with derived pointers
// or same (cv aside) types for array unique_ptrs

void test()
{
   //Single unique_ptr
   reset_counters();
   {
   B* p = new B;
   BOOST_TEST(A::count == 1);
   BOOST_TEST(B::count == 1);
   bml::unique_ptr<A, copy_constr_deleter<A> > s(p, copy_constr_deleter<A>());
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
   BOOST_TEST(B::count == 0);
   //Unbounded array unique_ptr
   reset_counters();
   {
   A* p = new A[2];
   BOOST_TEST(A::count == 2);
   bml::unique_ptr<const A[], copy_constr_deleter<const A[]> > s(p, copy_constr_deleter<const A[]>());
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
   BOOST_TEST(B::count == 0);
   //Bounded array unique_ptr
   reset_counters();
   {
   A* p = new A[2];
   BOOST_TEST(A::count == 2);
   bml::unique_ptr<const A[2], copy_constr_deleter<const A[2]> > s(p, copy_constr_deleter<const A[2]>());
   BOOST_TEST(s.get() == p);
   BOOST_TEST(s.get_deleter().state() == 5);
   }
   BOOST_TEST(A::count == 0);
   BOOST_TEST(B::count == 0);
}

}  //namespace unique_ptr_ctor_pointer_deleter_convert{

////////////////////////////////
//   unique_ptr_ctor_pointer_deleter_void
////////////////////////////////

namespace unique_ptr_ctor_pointer_deleter_void{

// unique_ptr(pointer, deleter) should work with function pointers
// unique_ptr<void> should work

bool my_free_called = false;

void my_free(void*)
{
    my_free_called = true;
}

void test()
{
   {
   int i = 0;
   bml::unique_ptr<void, void (*)(void*)> s(&i, my_free);
   BOOST_TEST(s.get() == &i);
   BOOST_TEST(s.get_deleter() == my_free);
   BOOST_TEST(!my_free_called);
   }
   BOOST_TEST(my_free_called);
}

}  //namespace unique_ptr_ctor_pointer_deleter_void{

////////////////////////////////
//   return_unique_single_conversion
////////////////////////////////

namespace return_unique_single_conversion{

template<class T>
bml::unique_ptr<T> make_unique_ptr_of_t()
{
   return bml::unique_ptr<T>(new T);
}

template<class T>
bml::unique_ptr<T const> return_const_unique_of_t()
{
   return bml::unique_ptr<T const> (make_unique_ptr_of_t<T>());
} 

void test()
{
   reset_counters();
   BOOST_TEST(A::count == 0);
   {
   bml::unique_ptr<const A> p(return_const_unique_of_t<A>());
   BOOST_TEST(A::count == 1);
   BOOST_TEST(B::count == 0);
   }
   BOOST_TEST(A::count == 0);
   {
   bml::unique_ptr<const A> p(return_const_unique_of_t<B>());
   BOOST_TEST(A::count == 1);
   BOOST_TEST(B::count == 1);
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace return_unique_single_conversion{


////////////////////////////////
//   return_unique_array_conversion
////////////////////////////////

namespace return_unique_array_conversion{

template<class T>
bml::unique_ptr<T[]> return_unique_array_of_t(std::size_t n)
{
   return bml::unique_ptr<T[]>(new T[n]);
}

template<class T>
bml::unique_ptr<const T[]> return_const_array_of_t(std::size_t n)
{
   return bml::unique_ptr<const T[]>(return_unique_array_of_t<T>(n));
} 

template<class T>
bml::unique_ptr<T[2]> return_unique_array_of_t_2()
{
   return bml::unique_ptr<T[2]>(new T[2]);
}

template<class T>
bml::unique_ptr<const T[2]> return_const_array_of_t_2()
{
   return bml::unique_ptr<const T[2]>(return_unique_array_of_t_2<T>());
} 

void test()
{
   reset_counters();
   BOOST_TEST(A::count == 0);
   {
   bml::unique_ptr<const A[]> p(return_unique_array_of_t<A>(2));
   BOOST_TEST(A::count == 2);
   BOOST_TEST(B::count == 0);
   }
   BOOST_TEST(A::count == 0);
   {
   bml::unique_ptr<const volatile A[]> p(return_unique_array_of_t<volatile A>(2));
   BOOST_TEST(A::count == 2);
   BOOST_TEST(B::count == 0);
   }
   BOOST_TEST(A::count == 0);
   {
   bml::unique_ptr<const volatile A[2]> p(return_const_array_of_t_2<A>());
   BOOST_TEST(A::count == 2);
   BOOST_TEST(B::count == 0);
   }
   BOOST_TEST(A::count == 0);
   {
   bml::unique_ptr<const volatile A[]> p(return_const_array_of_t_2<A>());
   BOOST_TEST(A::count == 2);
   BOOST_TEST(B::count == 0);
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace return_unique_array_conversion{

////////////////////////////////
//             main
////////////////////////////////
int main()
{
   //Constructors/Destructor
   unique_ptr_dtor_null::test();
   unique_ptr_ctor_default_delreq::test();
   unique_ptr_ctor_default_nocomplete::test();
   unique_ptr_ctor_pointer_delreq::test();
   unique_ptr_ctor_pointer_nocomplete::test();
   unique_ptr_ctor_pointer_convert::test();
   unique_ptr_ctor_pointer_deleter_movedel::test();
   unique_ptr_ctor_pointer_deleter_copydel::test();
   unique_ptr_ctor_pointer_deleter_dfctrdelref::test();
   unique_ptr_ctor_pointer_deleter_dfctrdelconstref::test();
   unique_ptr_ctor_pointer_deleter_convert::test();
   unique_ptr_ctor_pointer_deleter_void::test();
   return_unique_single_conversion::test();
   return_unique_array_conversion::test();

   //Test results
   return boost::report_errors();

}

#include "unique_ptr_test_utils_end.hpp"
