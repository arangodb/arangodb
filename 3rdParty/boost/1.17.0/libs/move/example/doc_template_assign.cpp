//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2014-2014.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/move/detail/config_begin.hpp>
#include <boost/move/detail/meta_utils_core.hpp>

#include <boost/move/move.hpp>

//[template_assign_example_foo_bar

class Foo
{
   BOOST_COPYABLE_AND_MOVABLE(Foo)

   public:
   int i;
   explicit Foo(int val)      : i(val)   {}

   Foo(BOOST_RV_REF(Foo) obj) : i(obj.i) {}

   Foo& operator=(BOOST_RV_REF(Foo) rhs)
   {  i = rhs.i; rhs.i = 0; return *this; }

   Foo& operator=(BOOST_COPY_ASSIGN_REF(Foo) rhs)
   {  i = rhs.i; return *this;   } //(1)

   template<class U> //(*) TEMPLATED ASSIGNMENT, potential problem
   //<-
   #if 1
   typename ::boost::move_detail::disable_if_same<U, Foo, Foo&>::type
   operator=(const U& rhs)
   #else
   //->
   Foo& operator=(const U& rhs)
   //<-
   #endif
   //->
   {  i = -rhs.i; return *this;  } //(2)
};
//]

struct Bar
{
   int i;
   explicit Bar(int val) : i(val) {}
};


//<-
#ifdef NDEBUG
#undef NDEBUG
#endif
//->
#include <cassert>

int main()
{
//[template_assign_example_main
   Foo foo1(1);
   //<-
   assert(foo1.i == 1);
   //->
   Foo foo2(2);
   //<-
   assert(foo2.i == 2);
   Bar bar(3);
   assert(bar.i == 3);
   //->
   foo2 = foo1; // Calls (1) in C++11 but (2) in C++98
   //<-
   assert(foo2.i == 1);
   assert(foo1.i == 1); //Fails in C++98 unless workaround is applied
   foo1 = bar;
   assert(foo1.i == -3);
   foo2 = boost::move(foo1);
   assert(foo1.i == 0);
   assert(foo2.i == -3);
   //->
   const Foo foo5(5);
   foo2 = foo5; // Calls (1) in C++11 but (2) in C++98
   //<-
   assert(foo2.i == 5); //Fails in C++98 unless workaround is applied
   assert(foo5.i == 5);
   //->
//]
   return 0;
}


#include <boost/move/detail/config_end.hpp>
