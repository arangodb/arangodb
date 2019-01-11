//
// impl/handler_alloc_hook.ipp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_IMPL_HANDLER_ALLOC_HOOK_IPP
#define BOOST_ASIO_IMPL_HANDLER_ALLOC_HOOK_IPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>
#include <boost/asio/detail/thread_context.hpp>
#include <boost/asio/detail/thread_info_base.hpp>
#include <boost/asio/handler_alloc_hook.hpp>

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {

void* asio_handler_allocate(std::size_t size, ...)
{
#if !defined(BOOST_ASIO_DISABLE_SMALL_BLOCK_RECYCLING)
  return detail::thread_info_base::allocate(
      detail::thread_context::thread_call_stack::top(), size);
#else // !defined(BOOST_ASIO_DISABLE_SMALL_BLOCK_RECYCLING)
  return ::operator new(size);
#endif // !defined(BOOST_ASIO_DISABLE_SMALL_BLOCK_RECYCLING)
}

void asio_handler_deallocate(void* pointer, std::size_t size, ...)
{
#if !defined(BOOST_ASIO_DISABLE_SMALL_BLOCK_RECYCLING)
  detail::thread_info_base::deallocate(
      detail::thread_context::thread_call_stack::top(), pointer, size);
#else // !defined(BOOST_ASIO_DISABLE_SMALL_BLOCK_RECYCLING)
  (void)size;
  ::operator delete(pointer);
#endif // !defined(BOOST_ASIO_DISABLE_SMALL_BLOCK_RECYCLING)
}

} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_IMPL_HANDLER_ALLOC_HOOK_IPP
