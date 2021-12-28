//
// system_executor.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Prevent link dependency on the Boost.System library.
#if !defined(BOOST_SYSTEM_NO_DEPRECATED)
#define BOOST_SYSTEM_NO_DEPRECATED
#endif // !defined(BOOST_SYSTEM_NO_DEPRECATED)

// Test that header file is self-contained.
#include <boost/asio/system_executor.hpp>

#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include "unit_test.hpp"

#if defined(BOOST_ASIO_HAS_BOOST_BIND)
# include <boost/bind/bind.hpp>
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
# include <functional>
#endif // defined(BOOST_ASIO_HAS_BOOST_BIND)

using namespace boost::asio;

#if defined(BOOST_ASIO_HAS_BOOST_BIND)
namespace bindns = boost;
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
namespace bindns = std;
#endif

void increment(boost::asio::detail::atomic_count* count)
{
  ++(*count);
}

void system_executor_query_test()
{
  BOOST_ASIO_CHECK(
      &boost::asio::query(system_executor(),
        boost::asio::execution::context)
      != static_cast<const system_context*>(0));

  BOOST_ASIO_CHECK(
      boost::asio::query(system_executor(),
        boost::asio::execution::blocking)
      == boost::asio::execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(system_executor(),
        boost::asio::execution::blocking.possibly)
      == boost::asio::execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(system_executor(),
        boost::asio::execution::outstanding_work)
      == boost::asio::execution::outstanding_work.untracked);

  BOOST_ASIO_CHECK(
      boost::asio::query(system_executor(),
        boost::asio::execution::outstanding_work.untracked)
      == boost::asio::execution::outstanding_work.untracked);

  BOOST_ASIO_CHECK(
      boost::asio::query(system_executor(),
        boost::asio::execution::relationship)
      == boost::asio::execution::relationship.fork);

  BOOST_ASIO_CHECK(
      boost::asio::query(system_executor(),
        boost::asio::execution::relationship.fork)
      == boost::asio::execution::relationship.fork);

  BOOST_ASIO_CHECK(
      boost::asio::query(system_executor(),
        boost::asio::execution::bulk_guarantee)
      == boost::asio::execution::bulk_guarantee.unsequenced);

  BOOST_ASIO_CHECK(
      boost::asio::query(system_executor(),
        boost::asio::execution::mapping)
      == boost::asio::execution::mapping.thread);

  BOOST_ASIO_CHECK(
      boost::asio::query(system_executor(),
        boost::asio::execution::allocator)
      == std::allocator<void>());
}

void system_executor_execute_test()
{
  boost::asio::detail::atomic_count count(0);

  boost::asio::execution::execute(system_executor(),
      bindns::bind(increment, &count));

  boost::asio::execution::execute(
      boost::asio::require(system_executor(),
        boost::asio::execution::blocking.possibly),
      bindns::bind(increment, &count));

  boost::asio::execution::execute(
      boost::asio::require(system_executor(),
        boost::asio::execution::blocking.always),
      bindns::bind(increment, &count));

  boost::asio::execution::execute(
      boost::asio::require(system_executor(),
        boost::asio::execution::blocking.never),
      bindns::bind(increment, &count));

  boost::asio::execution::execute(
      boost::asio::require(system_executor(),
        boost::asio::execution::blocking.never,
        boost::asio::execution::outstanding_work.untracked),
      bindns::bind(increment, &count));

  boost::asio::execution::execute(
      boost::asio::require(system_executor(),
        boost::asio::execution::blocking.never,
        boost::asio::execution::outstanding_work.untracked,
        boost::asio::execution::relationship.fork),
      bindns::bind(increment, &count));

  boost::asio::execution::execute(
      boost::asio::require(system_executor(),
        boost::asio::execution::blocking.never,
        boost::asio::execution::outstanding_work.untracked,
        boost::asio::execution::relationship.continuation),
      bindns::bind(increment, &count));

  boost::asio::execution::execute(
      boost::asio::prefer(
        boost::asio::require(system_executor(),
          boost::asio::execution::blocking.never,
          boost::asio::execution::outstanding_work.untracked,
          boost::asio::execution::relationship.continuation),
        boost::asio::execution::allocator(std::allocator<void>())),
      bindns::bind(increment, &count));

  boost::asio::execution::execute(
      boost::asio::prefer(
        boost::asio::require(system_executor(),
          boost::asio::execution::blocking.never,
          boost::asio::execution::outstanding_work.untracked,
          boost::asio::execution::relationship.continuation),
        boost::asio::execution::allocator),
      bindns::bind(increment, &count));

  boost::asio::query(system_executor(), execution::context).join();

  BOOST_ASIO_CHECK(count == 9);
}

BOOST_ASIO_TEST_SUITE
(
  "system_executor",
  BOOST_ASIO_TEST_CASE(system_executor_query_test)
  BOOST_ASIO_TEST_CASE(system_executor_execute_test)
)
