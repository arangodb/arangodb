/* Used in Boost.MultiIndex tests.
 *
 * Copyright 2003-2020 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#ifndef BOOST_MULTI_INDEX_TEST_ROOTED_ALLOCATOR_HPP
#define BOOST_MULTI_INDEX_TEST_ROOTED_ALLOCATOR_HPP

#include <boost/config.hpp> /* keep it first to prevent nasty warns in MSVC */
#include <boost/type_traits/integral_constant.hpp>
#include <memory>

#if defined(BOOST_MSVC)
#pragma warning(push)
#pragma warning(disable:4355) /* this used in base member initializer list */
#endif

template<typename T,bool Propagate,bool AlwaysEqual>
class rooted_allocator:public std::allocator<T>
{
  typedef boost::integral_constant<bool,Propagate>   propagate_type;
  typedef boost::integral_constant<bool,AlwaysEqual> always_equal_type;

public:
  typedef propagate_type    propagate_on_container_copy_assignment;
  typedef propagate_type    propagate_on_container_move_assignment;
  typedef propagate_type    propagate_on_container_swap;
  typedef always_equal_type is_always_equal;
  template<typename U>
  struct rebind{typedef rooted_allocator<U,Propagate,AlwaysEqual> other;};

  rooted_allocator():root(0){}
  explicit rooted_allocator(int):root(this){}
  template<typename U>
  rooted_allocator(const rooted_allocator<U,Propagate,AlwaysEqual>& x):
    root(x.root){}

  template<typename U>
  bool operator==(const rooted_allocator<U,Propagate,AlwaysEqual>& x)const
    {return AlwaysEqual?true:root==x.root;}
  template<typename U>
  bool operator!=(const rooted_allocator<U,Propagate,AlwaysEqual>& x)const
    {return !(*this==x);}

  template<typename U>
  bool comes_from(const rooted_allocator<U,Propagate,AlwaysEqual>& x)const
    {return root==&x;}

private:
  template<typename,bool,bool> friend class rooted_allocator;

  const void* root;
};

#if defined(BOOST_MSVC)
#pragma warning(pop) /* C4355 */
#endif

#endif
