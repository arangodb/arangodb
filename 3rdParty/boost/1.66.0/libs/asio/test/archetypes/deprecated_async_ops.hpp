//
// deprecated_async_ops.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ARCHETYPES_DEPRECATED_ASYNC_OPS_HPP
#define ARCHETYPES_DEPRECATED_ASYNC_OPS_HPP

#include <boost/asio/async_result.hpp>

#if !defined(BOOST_ASIO_NO_DEPRECATED)

#include <boost/asio/handler_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>

#if defined(BOOST_ASIO_HAS_BOOST_BIND)
# include <boost/bind.hpp>
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
# include <functional>
#endif // defined(BOOST_ASIO_HAS_BOOST_BIND)

namespace archetypes {

#if defined(BOOST_ASIO_HAS_BOOST_BIND)
namespace bindns = boost;
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
namespace bindns = std;
#endif // defined(BOOST_ASIO_HAS_BOOST_BIND)

template <typename CompletionToken>
typename boost::asio::async_result<
  typename boost::asio::handler_type<CompletionToken,
    void()>::type>::type
deprecated_async_op_0(boost::asio::io_context& ctx,
    BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::handler_type<CompletionToken,
    void()>::type handler_type;

  handler_type handler(BOOST_ASIO_MOVE_CAST(CompletionToken)(token));

  boost::asio::async_result<handler_type> result(handler);

  ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler)));

  return result.get();
}

template <typename CompletionToken>
typename boost::asio::async_result<
  typename boost::asio::handler_type<CompletionToken,
    void(boost::system::error_code)>::type>::type
deprecated_async_op_ec_0(boost::asio::io_context& ctx,
    bool ok, BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::handler_type<CompletionToken,
    void(boost::system::error_code)>::type handler_type;

  handler_type handler(BOOST_ASIO_MOVE_CAST(CompletionToken)(token));

  boost::asio::async_result<handler_type> result(handler);

  if (ok)
  {
    ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
          boost::system::error_code()));
  }
  else
  {
    ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
          boost::system::error_code(boost::asio::error::operation_aborted)));
  }

  return result.get();
}

template <typename CompletionToken>
typename boost::asio::async_result<
  typename boost::asio::handler_type<CompletionToken,
    void(std::exception_ptr)>::type>::type
deprecated_async_op_ex_0(boost::asio::io_context& ctx,
    bool ok, BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::handler_type<CompletionToken,
    void(std::exception_ptr)>::type handler_type;

  handler_type handler(BOOST_ASIO_MOVE_CAST(CompletionToken)(token));

  boost::asio::async_result<handler_type> result(handler);

  if (ok)
  {
    ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
          std::exception_ptr()));
  }
  else
  {
    ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
          std::make_exception_ptr(std::runtime_error("blah"))));
  }

  return result.get();
}

template <typename CompletionToken>
typename boost::asio::async_result<
  typename boost::asio::handler_type<CompletionToken,
    void(int)>::type>::type
deprecated_async_op_1(boost::asio::io_context& ctx,
    BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::handler_type<CompletionToken,
    void(int)>::type handler_type;

  handler_type handler(BOOST_ASIO_MOVE_CAST(CompletionToken)(token));

  boost::asio::async_result<handler_type> result(handler);

  ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler), 42));

  return result.get();
}

template <typename CompletionToken>
typename boost::asio::async_result<
  typename boost::asio::handler_type<CompletionToken,
    void(boost::system::error_code, int)>::type>::type
deprecated_async_op_ec_1(boost::asio::io_context& ctx,
    bool ok, BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::handler_type<CompletionToken,
    void(boost::system::error_code, int)>::type handler_type;

  handler_type handler(BOOST_ASIO_MOVE_CAST(CompletionToken)(token));

  boost::asio::async_result<handler_type> result(handler);

  if (ok)
  {
    ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
          boost::system::error_code(), 42));
  }
  else
  {
    ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
          boost::system::error_code(boost::asio::error::operation_aborted), 0));
  }

  return result.get();
}

template <typename CompletionToken>
typename boost::asio::async_result<
  typename boost::asio::handler_type<CompletionToken,
    void(std::exception_ptr, int)>::type>::type
deprecated_async_op_ex_1(boost::asio::io_context& ctx,
    bool ok, BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::handler_type<CompletionToken,
    void(std::exception_ptr, int)>::type handler_type;

  handler_type handler(BOOST_ASIO_MOVE_CAST(CompletionToken)(token));

  boost::asio::async_result<handler_type> result(handler);

  if (ok)
  {
    ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
          std::exception_ptr(), 42));
  }
  else
  {
    ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
          std::make_exception_ptr(std::runtime_error("blah")), 0));
  }

  return result.get();
}

template <typename CompletionToken>
typename boost::asio::async_result<
  typename boost::asio::handler_type<CompletionToken,
    void(int, double)>::type>::type
deprecated_async_op_2(boost::asio::io_context& ctx,
    BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::handler_type<CompletionToken,
    void(int, double)>::type handler_type;

  handler_type handler(BOOST_ASIO_MOVE_CAST(CompletionToken)(token));

  boost::asio::async_result<handler_type> result(handler);

  ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
        42, 2.0));

  return result.get();
}

template <typename CompletionToken>
typename boost::asio::async_result<
  typename boost::asio::handler_type<CompletionToken,
    void(boost::system::error_code, int, double)>::type>::type
deprecated_async_op_ec_2(boost::asio::io_context& ctx,
    bool ok, BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::handler_type<CompletionToken,
    void(boost::system::error_code, int, double)>::type handler_type;

  handler_type handler(BOOST_ASIO_MOVE_CAST(CompletionToken)(token));

  boost::asio::async_result<handler_type> result(handler);

  if (ok)
  {
    ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
          boost::system::error_code(), 42, 2.0));
  }
  else
  {
    ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
          boost::system::error_code(boost::asio::error::operation_aborted),
          0, 0.0));
  }

  return result.get();
}

template <typename CompletionToken>
typename boost::asio::async_result<
  typename boost::asio::handler_type<CompletionToken,
    void(std::exception_ptr, int, double)>::type>::type
deprecated_async_op_ex_2(boost::asio::io_context& ctx,
    bool ok, BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::handler_type<CompletionToken,
    void(std::exception_ptr, int, double)>::type handler_type;

  handler_type handler(BOOST_ASIO_MOVE_CAST(CompletionToken)(token));

  boost::asio::async_result<handler_type> result(handler);

  if (ok)
  {
    ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
          std::exception_ptr(), 42, 2.0));
  }
  else
  {
    ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
          std::make_exception_ptr(std::runtime_error("blah")), 0, 0.0));
  }

  return result.get();
}

template <typename CompletionToken>
typename boost::asio::async_result<
  typename boost::asio::handler_type<CompletionToken,
    void(int, double, char)>::type>::type
deprecated_async_op_3(boost::asio::io_context& ctx,
    BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::handler_type<CompletionToken,
    void(int, double, char)>::type handler_type;

  handler_type handler(BOOST_ASIO_MOVE_CAST(CompletionToken)(token));

  boost::asio::async_result<handler_type> result(handler);

  ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
        42, 2.0, 'a'));

  return result.get();
}

template <typename CompletionToken>
typename boost::asio::async_result<
  typename boost::asio::handler_type<CompletionToken,
    void(boost::system::error_code, int, double, char)>::type>::type
deprecated_async_op_ec_3(boost::asio::io_context& ctx,
    bool ok, BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::handler_type<CompletionToken,
    void(boost::system::error_code, int, double, char)>::type handler_type;

  handler_type handler(BOOST_ASIO_MOVE_CAST(CompletionToken)(token));

  boost::asio::async_result<handler_type> result(handler);

  if (ok)
  {
    ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
          boost::system::error_code(), 42, 2.0, 'a'));
  }
  else
  {
    ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
          boost::system::error_code(boost::asio::error::operation_aborted),
          0, 0.0, 'z'));
  }

  return result.get();
}

template <typename CompletionToken>
typename boost::asio::async_result<
  typename boost::asio::handler_type<CompletionToken,
    void(std::exception_ptr, int, double, char)>::type>::type
deprecated_async_op_ex_3(boost::asio::io_context& ctx,
    bool ok, BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::handler_type<CompletionToken,
    void(std::exception_ptr, int, double, char)>::type handler_type;

  handler_type handler(BOOST_ASIO_MOVE_CAST(CompletionToken)(token));

  boost::asio::async_result<handler_type> result(handler);

  if (ok)
  {
    ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
          std::exception_ptr(), 42, 2.0, 'a'));
  }
  else
  {
    ctx.post(bindns::bind(BOOST_ASIO_MOVE_CAST(handler_type)(handler),
          std::make_exception_ptr(std::runtime_error("blah")),
          0, 0.0, 'z'));
  }

  return result.get();
}

} // namespace archetypes

#endif // !defined(BOOST_ASIO_NO_DEPRECATED)

#endif // ARCHETYPES_DEPRECATED_ASYNC_OPS_HPP
