//////////////////////////////////////////////////////////////////////////////
// (C) Copyright John Maddock 2000.
// (C) Copyright Ion Gaztanaga 2005-2009.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTERPROCESS_DETAIL_TYPE_TRAITS_HPP
#define BOOST_INTERPROCESS_DETAIL_TYPE_TRAITS_HPP

#if (defined _MSC_VER) && (_MSC_VER >= 1200)
#  pragma once
#endif

#include <boost/interprocess/detail/config_begin.hpp>

namespace boost {
namespace interprocess { 
namespace ipcdetail {

struct nat{};

template<class T>
struct remove_reference
{
   typedef T type;
};

template<class T>
struct remove_reference<T&>
{
   typedef T type;
};

template<class T>
struct is_reference
{
   enum {  value = false   };
};

template<class T>
struct is_reference<T&>
{
   enum {  value = true   };
};

template<class T>
struct is_pointer
{
   enum {  value = false   };
};

template<class T>
struct is_pointer<T*>
{
   enum {  value = true   };
};

template <typename T>
struct add_reference
{
    typedef T& type;
};

template<class T>
struct add_reference<T&>
{
    typedef T& type;
};

template<>
struct add_reference<void>
{
    typedef nat &type;
};

template<>
struct add_reference<const void>
{
    typedef const nat &type;
};

template <class T>
struct add_const_reference
{  typedef const T &type;   };

template <class T>
struct add_const_reference<T&>
{  typedef T& type;   };

template <typename T, typename U>
struct is_same
{
   typedef char yes_type;
   struct no_type
   {
      char padding[8];
   };

   template <typename V>
   static yes_type is_same_tester(V*, V*);
   static no_type is_same_tester(...);

   static T *t;
   static U *u;

   static const bool value = sizeof(yes_type) == sizeof(is_same_tester(t,u));
};

} // namespace ipcdetail
}  //namespace interprocess { 
}  //namespace boost {

#endif   //#ifndef BOOST_INTERPROCESS_DETAIL_TYPE_TRAITS_HPP

#include <boost/interprocess/detail/config_end.hpp>

