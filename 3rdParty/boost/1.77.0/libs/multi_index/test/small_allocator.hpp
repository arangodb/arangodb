/* Used in Boost.MultiIndex tests.
 *
 * Copyright 2003-2018 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_TEST_SMALL_ALLOCATOR_HPP
#define BOOST_MULTI_INDEX_TEST_SMALL_ALLOCATOR_HPP

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */

template<typename T>
class small_allocator
{
public:
  typedef unsigned char size_type;
  typedef signed char   difference_type;
  typedef T*            pointer;
  typedef const T*      const_pointer;
  typedef void*         void_pointer;
  typedef const void*   const_void_pointer;
  typedef T&            reference;
  typedef const T&      const_reference;
  typedef T             value_type;
  template<class U>struct rebind{typedef small_allocator<U> other;};

  small_allocator(){}
  small_allocator(const small_allocator<T>&){}
  template<class U>small_allocator(const small_allocator<U>&,int=0){}

  pointer allocate(size_type n)
  {
    return pointer((T*)(new char[n*sizeof(T)]));
  }

  void deallocate(pointer p,size_type)
  {
    delete[](char *)&*p;
  }

  size_type max_size()const{return (size_type)(-1);}

  friend bool operator==(const small_allocator&,const small_allocator&)
  {
    return true;
  }

  friend bool operator!=(const small_allocator&,const small_allocator&)
  {
    return false;
  }
};

#endif
