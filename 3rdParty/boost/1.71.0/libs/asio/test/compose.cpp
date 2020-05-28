//
// compose.cpp
// ~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include <boost/asio/compose.hpp>

#include "unit_test.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#if defined(BOOST_ASIO_HAS_BOOST_BIND)
# include <boost/bind.hpp>
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
# include <functional>
#endif // defined(BOOST_ASIO_HAS_BOOST_BIND)

//------------------------------------------------------------------------------

class impl_0_completion_args
{
public:
  explicit impl_0_completion_args(boost::asio::io_context& ioc)
    : ioc_(ioc),
      state_(starting)
  {
  }

  template <typename Self>
  void operator()(Self& self)
  {
    switch (state_)
    {
    case starting:
      state_ = posting;
      boost::asio::post(ioc_, BOOST_ASIO_MOVE_CAST(Self)(self));
      break;
    case posting:
      self.complete();
      break;
    default:
      break;
    }
  }

private:
  boost::asio::io_context& ioc_;
  enum { starting, posting } state_;
};

template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void())
async_0_completion_args(boost::asio::io_context& ioc,
    BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  return boost::asio::async_compose<CompletionToken, void()>(
      impl_0_completion_args(ioc), token);
}

void compose_0_args_handler(int* count)
{
  ++(*count);
}

void compose_0_completion_args_test()
{
#if defined(BOOST_ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
#endif // defined(BOOST_ASIO_HAS_BOOST_BIND)

  boost::asio::io_context ioc;
  int count = 0;

  async_0_completion_args(ioc, bindns::bind(&compose_0_args_handler, &count));

  // No handlers can be called until run() is called.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);

  ioc.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 1);
}

//------------------------------------------------------------------------------

class impl_1_completion_arg
{
public:
  explicit impl_1_completion_arg(boost::asio::io_context& ioc)
    : ioc_(ioc),
      state_(starting)
  {
  }

  template <typename Self>
  void operator()(Self& self)
  {
    switch (state_)
    {
    case starting:
      state_ = posting;
      boost::asio::post(ioc_, BOOST_ASIO_MOVE_CAST(Self)(self));
      break;
    case posting:
      self.complete(42);
      break;
    default:
      break;
    }
  }

private:
  boost::asio::io_context& ioc_;
  enum { starting, posting } state_;
};

template <typename CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(int))
async_1_completion_arg(boost::asio::io_context& ioc,
    BOOST_ASIO_MOVE_ARG(CompletionToken) token)
{
  return boost::asio::async_compose<CompletionToken, void(int)>(
      impl_1_completion_arg(ioc), token);
}

void compose_1_args_handler(int* count, int* result_out, int result)
{
  ++(*count);
  *result_out = result;
}

void compose_1_completion_arg_test()
{
#if defined(BOOST_ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
  using std::placeholders::_1;
#endif // defined(BOOST_ASIO_HAS_BOOST_BIND)

  boost::asio::io_context ioc;
  int count = 0;
  int result = 0;

  async_1_completion_arg(ioc,
      bindns::bind(&compose_1_args_handler, &count, &result, _1));

  // No handlers can be called until run() is called.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);
  BOOST_ASIO_CHECK(result == 0);

  ioc.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 1);
  BOOST_ASIO_CHECK(result == 42);
}

//------------------------------------------------------------------------------

BOOST_ASIO_TEST_SUITE
(
  "compose",
  BOOST_ASIO_TEST_CASE(compose_0_completion_args_test)
  BOOST_ASIO_TEST_CASE(compose_1_completion_arg_test)
)
