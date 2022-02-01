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

#include <boost/core/ignore_unused.hpp>

//[move_return_example
#include "movable.hpp"
#include "copymovable.hpp"
#include <boost/move/core.hpp>

template<class Type>
struct factory_functor
{
   typedef Type return_type;

   Type operator()() const
   {  Type t;  return BOOST_MOVE_RET(Type, t);  }
};

struct return_reference
{
   typedef non_copy_movable &return_type;

   non_copy_movable &operator()() const
   {  return ncm; }

   static non_copy_movable ncm;
};

non_copy_movable return_reference::ncm;

//A wrapper that locks a mutex while the
//factory creates a new value.
//It must generically move the return value
//if possible both in C++03 and C++11
template <class Factory>
typename Factory::return_type lock_wrapper(Factory f)
{
   typedef typename Factory::return_type return_type;
   //LOCK();
   return_type r = f();
   //UNLOCK();

   //In C++03: boost::move() if R is not a reference and
   //has move emulation enabled. In C++11: just return r.
   return BOOST_MOVE_RET(return_type, r);
}

int main()
{
   movable m            = lock_wrapper(factory_functor<movable>     ());
   copy_movable cm      = lock_wrapper(factory_functor<copy_movable>());
   copyable c           = lock_wrapper(factory_functor<copyable>    ());
   non_copy_movable &mr = lock_wrapper(return_reference             ());
   //<-
   boost::ignore_unused(m); boost::ignore_unused(cm); boost::ignore_unused(c); boost::ignore_unused(mr);
   //->
   return 0;
}
//]
