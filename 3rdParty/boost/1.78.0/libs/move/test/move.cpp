//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright David Abrahams, Vicente Botet, Ion Gaztanaga 2009.
// (C) Copyright Ion Gaztanaga 2009-2014.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/move/utility_core.hpp>
#include <boost/core/ignore_unused.hpp>
#include "../example/movable.hpp"
#include "../example/copymovable.hpp"
#include <boost/static_assert.hpp>

movable function(movable m)
{
   return movable(boost::move(m));
}

movable functionr(BOOST_RV_REF(movable) m)
{
   return movable(boost::move(m));
}

movable function2(movable m)
{
   return boost::move(m);
}

BOOST_RV_REF(movable) function2r(BOOST_RV_REF(movable) m)
{
   return boost::move(m);
}

movable move_return_function2 ()
{
    return movable();
}

movable move_return_function ()
{
    movable m;
    return (boost::move(m));
}


//Catch by value
void function_value(movable)
{}

//Catch by reference
void function_ref(const movable &)
{}

//Catch by reference
void function_ref(BOOST_RV_REF(movable))
{}

movable create_movable()
{  return movable(); }

template<class Type>
struct factory
{
   Type operator()() const
   {
      Type t;
      return BOOST_MOVE_RET(Type, t);
   }
};

template<class Type>
struct factory<Type &>
{
   static Type t;
   Type &operator()() const
   {
      return BOOST_MOVE_RET(Type&, t);
   }
};

template<class Type>
Type factory<Type&>::t;

template <class R, class F>
R factory_wrapper(F f)
{
  // lock();
  R r = f();
  // unlock();
  return BOOST_MOVE_RET(R, r);
}

int main()
{
   #if defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
   BOOST_STATIC_ASSERT((boost::has_nothrow_move<movable>::value == true));
   BOOST_STATIC_ASSERT((boost::has_move_emulation_enabled<copyable>::value == false));
   BOOST_STATIC_ASSERT((boost::has_move_emulation_enabled<copyable*>::value == false));
   BOOST_STATIC_ASSERT((boost::has_move_emulation_enabled<int>::value == false));
   BOOST_STATIC_ASSERT((boost::has_move_emulation_enabled<int&>::value == false));
   BOOST_STATIC_ASSERT((boost::has_move_emulation_enabled<int*>::value == false));
   #endif

   {
      movable m;
      movable m2(boost::move(m));
      movable m3(function(movable(boost::move(m2))));
      movable m4(function(boost::move(m3)));
      ::boost::ignore_unused(m);::boost::ignore_unused(m2);::boost::ignore_unused(m3);::boost::ignore_unused(m4);
   }
   {
      movable m;
      movable m2(boost::move(m));
      movable m3(functionr(movable(boost::move(m2))));
      movable m4(functionr(boost::move(m3))); 
      ::boost::ignore_unused(m);::boost::ignore_unused(m2);::boost::ignore_unused(m3);::boost::ignore_unused(m4);
   }
   {
      movable m;
      movable m2(boost::move(m));
      movable m3(function2(movable(boost::move(m2))));
      movable m4(function2(boost::move(m3)));
      ::boost::ignore_unused(m);::boost::ignore_unused(m2);::boost::ignore_unused(m3);::boost::ignore_unused(m4);
   }
   {
      movable m;
      movable m2(boost::move(m));
      movable m3(function2r(movable(boost::move(m2))));
      movable m4(function2r(boost::move(m3)));
      ::boost::ignore_unused(m);::boost::ignore_unused(m2);::boost::ignore_unused(m3);::boost::ignore_unused(m4);
   }
   {
      movable m;
      movable m2(boost::move(m));
      movable m3(move_return_function());
      ::boost::ignore_unused(m);::boost::ignore_unused(m2);::boost::ignore_unused(m3);
   }
   {
      movable m;
      movable m2(boost::move(m));
      movable m3(move_return_function2());
      ::boost::ignore_unused(m);::boost::ignore_unused(m2);::boost::ignore_unused(m3);
   }
   {
      //movable
      movable m (factory_wrapper<movable>(factory<movable>()));
      m = factory_wrapper<movable>(factory<movable>());
      movable&mr(factory_wrapper<movable&>(factory<movable&>()));
      movable&mr2 = factory_wrapper<movable&>(factory<movable&>());
      ::boost::ignore_unused(mr);
      ::boost::ignore_unused(mr2);
      ::boost::ignore_unused(m);
   }
   {
      //copyable
      copyable c (factory_wrapper<copyable>(factory<copyable>()));
      c = factory_wrapper<copyable>(factory<copyable>());
      copyable&cr(factory_wrapper<copyable&>(factory<copyable&>()));
      copyable&cr2 = factory_wrapper<copyable&>(factory<copyable&>());
      ::boost::ignore_unused(cr);
      ::boost::ignore_unused(cr2);
      ::boost::ignore_unused(c);
   }

   {
      //copy_movable
      copy_movable c (factory_wrapper<copy_movable>(factory<copy_movable>()));
      c = factory_wrapper<copy_movable>(factory<copy_movable>());
      copy_movable&cr(factory_wrapper<copy_movable&>(factory<copy_movable&>()));
      copy_movable&cr2 = factory_wrapper<copy_movable&>(factory<copy_movable&>());
      ::boost::ignore_unused(cr);
      ::boost::ignore_unused(cr2);
      ::boost::ignore_unused(c);
   }

   return 0;
}
