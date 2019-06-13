//
// async_result.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ARCHETYPES_DEPRECATED_ASYNC_RESULT_HPP
#define ARCHETYPES_DEPRECATED_ASYNC_RESULT_HPP

#include <boost/asio/async_result.hpp>

#if !defined(BOOST_ASIO_NO_DEPRECATED)

#include <boost/asio/handler_type.hpp>

namespace archetypes {

struct deprecated_lazy_handler
{
};

struct deprecated_concrete_handler
{
  deprecated_concrete_handler(deprecated_lazy_handler)
  {
  }

  template <typename Arg1>
  void operator()(Arg1)
  {
  }

  template <typename Arg1, typename Arg2>
  void operator()(Arg1, Arg2)
  {
  }

#if defined(BOOST_ASIO_HAS_MOVE)
  deprecated_concrete_handler(deprecated_concrete_handler&&) {}
private:
  deprecated_concrete_handler(const deprecated_concrete_handler&);
#endif // defined(BOOST_ASIO_HAS_MOVE)
};

} // namespace archetypes

namespace boost {
namespace asio {

template <typename Signature>
struct handler_type<archetypes::deprecated_lazy_handler, Signature>
{
  typedef archetypes::deprecated_concrete_handler type;
};

template <>
class async_result<archetypes::deprecated_concrete_handler>
{
public:
  // The return type of the initiating function.
  typedef double type;

  // Construct an async_result from a given handler.
  explicit async_result(archetypes::deprecated_concrete_handler&)
  {
  }

  // Obtain the value to be returned from the initiating function.
  type get()
  {
    return 42;
  }
};

} // namespace asio
} // namespace boost

#endif // !defined(BOOST_ASIO_NO_DEPRECATED)

#endif // ARCHETYPES_DEPRECATED_ASYNC_RESULT_HPP
