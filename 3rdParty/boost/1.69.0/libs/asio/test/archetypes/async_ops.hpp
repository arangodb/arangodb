//
// async_ops.hpp
// ~~~~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ARCHETYPES_ASYNC_OPS_HPP
#define ARCHETYPES_ASYNC_OPS_HPP

#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/error.hpp>

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
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void())
async_op_0(BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::async_completion<CompletionToken,
    void()>::completion_handler_type handler_type;

  boost::asio::async_completion<CompletionToken,
    void()> completion(token);

  typename boost::asio::associated_allocator<handler_type>::type a
    = boost::asio::get_associated_allocator(completion.completion_handler);

  typename boost::asio::associated_executor<handler_type>::type ex
    = boost::asio::get_associated_executor(completion.completion_handler);

  ex.post(BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler), a);

  return completion.result.get();
}

template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(boost::system::error_code))
async_op_ec_0(bool ok, BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::async_completion<CompletionToken,
    void(boost::system::error_code)>::completion_handler_type handler_type;

  boost::asio::async_completion<CompletionToken,
    void(boost::system::error_code)> completion(token);

  typename boost::asio::associated_allocator<handler_type>::type a
    = boost::asio::get_associated_allocator(completion.completion_handler);

  typename boost::asio::associated_executor<handler_type>::type ex
    = boost::asio::get_associated_executor(completion.completion_handler);

  if (ok)
  {
    ex.post(
        bindns::bind(
          BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
          boost::system::error_code()), a);
  }
  else
  {
    ex.post(
        bindns::bind(
          BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
          boost::system::error_code(boost::asio::error::operation_aborted)), a);
  }

  return completion.result.get();
}

template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(std::exception_ptr))
async_op_ex_0(bool ok, BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::async_completion<CompletionToken,
    void(std::exception_ptr)>::completion_handler_type handler_type;

  boost::asio::async_completion<CompletionToken,
    void(std::exception_ptr)> completion(token);

  typename boost::asio::associated_allocator<handler_type>::type a
    = boost::asio::get_associated_allocator(completion.completion_handler);

  typename boost::asio::associated_executor<handler_type>::type ex
    = boost::asio::get_associated_executor(completion.completion_handler);

  if (ok)
  {
    ex.post(
        bindns::bind(
          BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
          std::exception_ptr()), a);
  }
  else
  {
    ex.post(
        bindns::bind(
          BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
          std::make_exception_ptr(std::runtime_error("blah"))), a);
  }

  return completion.result.get();
}

template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(int))
async_op_1(BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::async_completion<CompletionToken,
    void(int)>::completion_handler_type handler_type;

  boost::asio::async_completion<CompletionToken,
    void(int)> completion(token);

  typename boost::asio::associated_allocator<handler_type>::type a
    = boost::asio::get_associated_allocator(completion.completion_handler);

  typename boost::asio::associated_executor<handler_type>::type ex
    = boost::asio::get_associated_executor(completion.completion_handler);

  ex.post(
      bindns::bind(
        BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
        42), a);

  return completion.result.get();
}

template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken,
    void(boost::system::error_code, int))
async_op_ec_1(bool ok, BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::async_completion<CompletionToken,
    void(boost::system::error_code, int)>::completion_handler_type
      handler_type;

  boost::asio::async_completion<CompletionToken,
    void(boost::system::error_code, int)> completion(token);

  typename boost::asio::associated_allocator<handler_type>::type a
    = boost::asio::get_associated_allocator(completion.completion_handler);

  typename boost::asio::associated_executor<handler_type>::type ex
    = boost::asio::get_associated_executor(completion.completion_handler);

  if (ok)
  {
    ex.post(
        bindns::bind(
          BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
          boost::system::error_code(), 42), a);
  }
  else
  {
    ex.post(
        bindns::bind(
          BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
          boost::system::error_code(boost::asio::error::operation_aborted),
          0), a);
  }

  return completion.result.get();
}

template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(std::exception_ptr, int))
async_op_ex_1(bool ok, BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::async_completion<CompletionToken,
    void(std::exception_ptr, int)>::completion_handler_type
      handler_type;

  boost::asio::async_completion<CompletionToken,
    void(std::exception_ptr, int)> completion(token);

  typename boost::asio::associated_allocator<handler_type>::type a
    = boost::asio::get_associated_allocator(completion.completion_handler);

  typename boost::asio::associated_executor<handler_type>::type ex
    = boost::asio::get_associated_executor(completion.completion_handler);

  if (ok)
  {
    ex.post(
        bindns::bind(
          BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
          std::exception_ptr(), 42), a);
  }
  else
  {
    ex.post(
        bindns::bind(
          BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
          std::make_exception_ptr(std::runtime_error("blah")), 0), a);
  }

  return completion.result.get();
}

template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(int, double))
async_op_2(BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::async_completion<CompletionToken,
    void(int, double)>::completion_handler_type handler_type;

  boost::asio::async_completion<CompletionToken,
    void(int, double)> completion(token);

  typename boost::asio::associated_allocator<handler_type>::type a
    = boost::asio::get_associated_allocator(completion.completion_handler);

  typename boost::asio::associated_executor<handler_type>::type ex
    = boost::asio::get_associated_executor(completion.completion_handler);

  ex.post(
      bindns::bind(
        BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
        42, 2.0), a);

  return completion.result.get();
}

template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken,
    void(boost::system::error_code, int, double))
async_op_ec_2(bool ok, BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::async_completion<CompletionToken,
    void(boost::system::error_code, int, double)>::completion_handler_type
      handler_type;

  boost::asio::async_completion<CompletionToken,
    void(boost::system::error_code, int, double)> completion(token);

  typename boost::asio::associated_allocator<handler_type>::type a
    = boost::asio::get_associated_allocator(completion.completion_handler);

  typename boost::asio::associated_executor<handler_type>::type ex
    = boost::asio::get_associated_executor(completion.completion_handler);

  if (ok)
  {
    ex.post(
        bindns::bind(
          BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
          boost::system::error_code(), 42, 2.0), a);
  }
  else
  {
    ex.post(
        bindns::bind(
          BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
          boost::system::error_code(boost::asio::error::operation_aborted),
          0, 0.0), a);
  }

  return completion.result.get();
}

template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken,
    void(std::exception_ptr, int, double))
async_op_ex_2(bool ok, BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::async_completion<CompletionToken,
    void(std::exception_ptr, int, double)>::completion_handler_type
      handler_type;

  boost::asio::async_completion<CompletionToken,
    void(std::exception_ptr, int, double)> completion(token);

  typename boost::asio::associated_allocator<handler_type>::type a
    = boost::asio::get_associated_allocator(completion.completion_handler);

  typename boost::asio::associated_executor<handler_type>::type ex
    = boost::asio::get_associated_executor(completion.completion_handler);

  if (ok)
  {
    ex.post(
        bindns::bind(
          BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
          std::exception_ptr(), 42, 2.0), a);
  }
  else
  {
    ex.post(
        bindns::bind(
          BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
          std::make_exception_ptr(std::runtime_error("blah")), 0, 0.0), a);
  }

  return completion.result.get();
}

template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(int, double, char))
async_op_3(BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::async_completion<CompletionToken,
    void(int, double, char)>::completion_handler_type handler_type;

  boost::asio::async_completion<CompletionToken,
    void(int, double, char)> completion(token);

  typename boost::asio::associated_allocator<handler_type>::type a
    = boost::asio::get_associated_allocator(completion.completion_handler);

  typename boost::asio::associated_executor<handler_type>::type ex
    = boost::asio::get_associated_executor(completion.completion_handler);

  ex.post(
      bindns::bind(
        BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
        42, 2.0, 'a'), a);

  return completion.result.get();
}

template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken,
    void(boost::system::error_code, int, double, char))
async_op_ec_3(bool ok, BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::async_completion<CompletionToken,
    void(boost::system::error_code, int, double, char)>::completion_handler_type
      handler_type;

  boost::asio::async_completion<CompletionToken,
    void(boost::system::error_code, int, double, char)> completion(token);

  typename boost::asio::associated_allocator<handler_type>::type a
    = boost::asio::get_associated_allocator(completion.completion_handler);

  typename boost::asio::associated_executor<handler_type>::type ex
    = boost::asio::get_associated_executor(completion.completion_handler);

  if (ok)
  {
    ex.post(
        bindns::bind(
          BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
          boost::system::error_code(), 42, 2.0, 'a'), a);
  }
  else
  {
    ex.post(
        bindns::bind(
          BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
          boost::system::error_code(boost::asio::error::operation_aborted),
          0, 0.0, 'z'), a);
  }

  return completion.result.get();
}

template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken,
    void(std::exception_ptr, int, double, char))
async_op_ex_3(bool ok, BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  typedef typename boost::asio::async_completion<CompletionToken,
    void(std::exception_ptr, int, double, char)>::completion_handler_type
      handler_type;

  boost::asio::async_completion<CompletionToken,
    void(std::exception_ptr, int, double, char)> completion(token);

  typename boost::asio::associated_allocator<handler_type>::type a
    = boost::asio::get_associated_allocator(completion.completion_handler);

  typename boost::asio::associated_executor<handler_type>::type ex
    = boost::asio::get_associated_executor(completion.completion_handler);

  if (ok)
  {
    ex.post(
        bindns::bind(
          BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
          std::exception_ptr(), 42, 2.0, 'a'), a);
  }
  else
  {
    ex.post(
        bindns::bind(
          BOOST_ASIO_MOVE_CAST(handler_type)(completion.completion_handler),
          std::make_exception_ptr(std::runtime_error("blah")),
          0, 0.0, 'z'), a);
  }

  return completion.result.get();
}

} // namespace archetypes

#endif // ARCHETYPES_ASYNC_OPS_HPP
