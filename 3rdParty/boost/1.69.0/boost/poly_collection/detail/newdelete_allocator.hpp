/* Copyright 2016-2017 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_DETAIL_NEWDELETE_ALLOCATOR_HPP
#define BOOST_POLY_COLLECTION_DETAIL_NEWDELETE_ALLOCATOR_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/poly_collection/detail/is_constructible.hpp>
#include <memory>
#include <new>
#include <utility>

namespace boost{

namespace poly_collection{

namespace detail{

/* In order to comply with [container.requirements.general]/3,
 * newdelete_allocator_adaptor<Allocator> overrides
 * Allocator::construct/destroy with vanilla new/delete implementations.
 * Used therefore in all auxiliary internal structures.
 */

template<typename Allocator>
struct newdelete_allocator_adaptor:Allocator
{
  using traits=std::allocator_traits<Allocator>;

  using value_type=typename traits::value_type;
  using size_type=typename traits::size_type;
  using difference_type=typename traits::difference_type;
  using pointer=typename traits::pointer;
  using const_pointer=typename traits::const_pointer;
  using void_pointer=typename traits::void_pointer;
  using const_void_pointer=typename traits::const_void_pointer;
  using propagate_on_container_copy_assignment=
    typename traits::propagate_on_container_copy_assignment;
  using propagate_on_container_move_assignment=
    typename traits::propagate_on_container_move_assignment;
  using propagate_on_container_swap=
    typename traits::propagate_on_container_swap;

  template<typename U>
  struct rebind
  {
    using other=newdelete_allocator_adaptor<
      typename traits::template rebind_alloc<U>>;
  };

  newdelete_allocator_adaptor()=default;
  newdelete_allocator_adaptor(const newdelete_allocator_adaptor&)=default;

  template<
    typename Allocator2,
    typename std::enable_if<
      is_constructible<Allocator,Allocator2>::value
    >::type* =nullptr
  >
  newdelete_allocator_adaptor(const Allocator2& x)noexcept:Allocator{x}{}

  template<
    typename Allocator2,
    typename std::enable_if<
      is_constructible<Allocator,Allocator2>::value
    >::type* =nullptr
  >
  newdelete_allocator_adaptor(
    const newdelete_allocator_adaptor<Allocator2>& x)noexcept:
    Allocator{static_cast<const Allocator2&>(x)}{}

  newdelete_allocator_adaptor& operator=(
    const newdelete_allocator_adaptor&)=default;

  template<typename T,typename... Args>
  void construct(T* p,Args&&... args)
  {
    ::new ((void*)p) T(std::forward<Args>(args)...);
  }

  template<typename T>
  void destroy(T* p)
  {
    p->~T();
  }
};

} /* namespace poly_collection::detail */

} /* namespace poly_collection */

} /* namespace boost */

#endif
