/* Used in Boost.MultiIndex tests.
 *
 * Copyright 2003-2020 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_TEST_COUNT_ALLOCATOR_HPP
#define BOOST_MULTI_INDEX_TEST_COUNT_ALLOCATOR_HPP

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <memory>

template<typename T>
struct count_allocator:std::allocator<T>
{
  typedef std::allocator<T> super;
  template<class U>
  struct rebind{typedef count_allocator<U> other;};

  count_allocator(std::size_t& element_count,std::size_t& allocator_count):
    pelement_count(&element_count),pallocator_count(&allocator_count)
    {++(*pallocator_count);}
  count_allocator(const count_allocator<T>& x):
    super(x),
    pelement_count(x.pelement_count),pallocator_count(x.pallocator_count)
    {++(*pallocator_count);}
  template<class U>count_allocator(const count_allocator<U>& x):
    super(x),
    pelement_count(x.pelement_count),pallocator_count(x.pallocator_count)
    {++(*pallocator_count);}
  ~count_allocator()
    {--(*pallocator_count);}

  count_allocator& operator=(const count_allocator<T>& x)
  {
    pelement_count=x.pelement_count;
    pallocator_count=x.pallocator_count;
    return *this;
  }

  T* allocate(std::size_t n)
  {
    *pelement_count+=n;
    return super::allocate(n);
  }

  void deallocate(T* p,std::size_t n)
  {
    super::deallocate(p,n);
    *pelement_count-=n;
  }

  std::size_t* pelement_count;
  std::size_t* pallocator_count;
};

#endif
