/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2006-2013
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOOST_INTRUSIVE_TEST_COMMON_FUNCTORS_HPP
#define BOOST_INTRUSIVE_TEST_COMMON_FUNCTORS_HPP

#include<boost/intrusive/detail/iterator.hpp>
#include<boost/intrusive/detail/mpl.hpp>
#include<boost/static_assert.hpp>
#include<boost/move/detail/to_raw_pointer.hpp>

namespace boost      {
namespace intrusive  {
namespace test       {

template<class T>
class delete_disposer
{
   public:
   template <class Pointer>
      void operator()(Pointer p)
   {
      typedef typename boost::intrusive::iterator_traits<Pointer>::value_type value_type;
      BOOST_STATIC_ASSERT(( detail::is_same<T, value_type>::value ));
      delete boost::movelib::to_raw_pointer(p);
   }
};

template<class T>
class new_cloner
{
   public:
      T *operator()(const T &t)
   {  return new T(t);  }
};

template<class T>
class new_nonconst_cloner
{
   public:
      T *operator()(T &t)
   {  return new T(t);  }
};

template<class T>
class new_default_factory
{
   public:
      T *operator()()
   {  return new T();  }
};

class empty_disposer
{
   public:
   template<class T>
   void operator()(const T &)
   {}
};

struct any_less
{
   template<class T, class U>
   bool operator()(const T &t, const U &u) const
   {  return t < u;  }
};

struct any_greater
{
   template<class T, class U>
   bool operator()(const T &t, const U &u) const
   {  return t > u;  }
};

}  //namespace test       {
}  //namespace intrusive  {
}  //namespace boost      {

#endif
