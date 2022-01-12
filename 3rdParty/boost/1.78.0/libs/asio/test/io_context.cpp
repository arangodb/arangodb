//
// io_context.cpp
// ~~~~~~~~~~~~~~
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

// Test that header file is self-contained.
#include <boost/asio/io_context.hpp>

#include <sstream>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/detail/thread.hpp>
#include "unit_test.hpp"

#if defined(BOOST_ASIO_HAS_BOOST_DATE_TIME)
# include <boost/asio/deadline_timer.hpp>
#else // defined(BOOST_ASIO_HAS_BOOST_DATE_TIME)
# include <boost/asio/steady_timer.hpp>
#endif // defined(BOOST_ASIO_HAS_BOOST_DATE_TIME)

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

#if defined(BOOST_ASIO_HAS_BOOST_DATE_TIME)
typedef deadline_timer timer;
namespace chronons = boost::posix_time;
#elif defined(BOOST_ASIO_HAS_CHRONO)
typedef steady_timer timer;
namespace chronons = boost::asio::chrono;
#endif // defined(BOOST_ASIO_HAS_BOOST_DATE_TIME)

void increment(int* count)
{
  ++(*count);
}

void decrement_to_zero(io_context* ioc, int* count)
{
  if (*count > 0)
  {
    --(*count);

    int before_value = *count;
    boost::asio::post(*ioc, bindns::bind(decrement_to_zero, ioc, count));

    // Handler execution cannot nest, so count value should remain unchanged.
    BOOST_ASIO_CHECK(*count == before_value);
  }
}

void nested_decrement_to_zero(io_context* ioc, int* count)
{
  if (*count > 0)
  {
    --(*count);

    boost::asio::dispatch(*ioc,
        bindns::bind(nested_decrement_to_zero, ioc, count));

    // Handler execution is nested, so count value should now be zero.
    BOOST_ASIO_CHECK(*count == 0);
  }
}

void sleep_increment(io_context* ioc, int* count)
{
  timer t(*ioc, chronons::seconds(2));
  t.wait();

  if (++(*count) < 3)
    boost::asio::post(*ioc, bindns::bind(sleep_increment, ioc, count));
}

void start_sleep_increments(io_context* ioc, int* count)
{
  // Give all threads a chance to start.
  timer t(*ioc, chronons::seconds(2));
  t.wait();

  // Start the first of three increments.
  boost::asio::post(*ioc, bindns::bind(sleep_increment, ioc, count));
}

void throw_exception()
{
  throw 1;
}

void io_context_run(io_context* ioc)
{
  ioc->run();
}

void io_context_test()
{
  io_context ioc;
  int count = 0;

  boost::asio::post(ioc, bindns::bind(increment, &count));

  // No handlers can be called until run() is called.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);

  ioc.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  ioc.restart();
  boost::asio::post(ioc, bindns::bind(increment, &count));
  boost::asio::post(ioc, bindns::bind(increment, &count));
  boost::asio::post(ioc, bindns::bind(increment, &count));
  boost::asio::post(ioc, bindns::bind(increment, &count));
  boost::asio::post(ioc, bindns::bind(increment, &count));

  // No handlers can be called until run() is called.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);

  ioc.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 5);

  count = 0;
  ioc.restart();
  executor_work_guard<io_context::executor_type> w = make_work_guard(ioc);
  boost::asio::post(ioc, bindns::bind(&io_context::stop, &ioc));
  BOOST_ASIO_CHECK(!ioc.stopped());
  ioc.run();

  // The only operation executed should have been to stop run().
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);

  ioc.restart();
  boost::asio::post(ioc, bindns::bind(increment, &count));
  w.reset();

  // No handlers can be called until run() is called.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);

  ioc.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 1);

  count = 10;
  ioc.restart();
  boost::asio::post(ioc, bindns::bind(decrement_to_zero, &ioc, &count));

  // No handlers can be called until run() is called.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 10);

  ioc.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);

  count = 10;
  ioc.restart();
  boost::asio::post(ioc, bindns::bind(nested_decrement_to_zero, &ioc, &count));

  // No handlers can be called until run() is called.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 10);

  ioc.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);

  count = 10;
  ioc.restart();
  boost::asio::dispatch(ioc,
      bindns::bind(nested_decrement_to_zero, &ioc, &count));

  // No handlers can be called until run() is called, even though nested
  // delivery was specifically allowed in the previous call.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 10);

  ioc.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);

  count = 0;
  int count2 = 0;
  ioc.restart();
  BOOST_ASIO_CHECK(!ioc.stopped());
  boost::asio::post(ioc, bindns::bind(start_sleep_increments, &ioc, &count));
  boost::asio::post(ioc, bindns::bind(start_sleep_increments, &ioc, &count2));
  boost::asio::detail::thread thread1(bindns::bind(io_context_run, &ioc));
  boost::asio::detail::thread thread2(bindns::bind(io_context_run, &ioc));
  thread1.join();
  thread2.join();

  // The run() calls will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 3);
  BOOST_ASIO_CHECK(count2 == 3);

  count = 10;
  io_context ioc2;
  boost::asio::dispatch(ioc, boost::asio::bind_executor(ioc2,
        bindns::bind(decrement_to_zero, &ioc2, &count)));
  ioc.restart();
  BOOST_ASIO_CHECK(!ioc.stopped());
  ioc.run();

  // No decrement_to_zero handlers can be called until run() is called on the
  // second io_context object.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 10);

  ioc2.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(count == 0);

  count = 0;
  int exception_count = 0;
  ioc.restart();
  boost::asio::post(ioc, &throw_exception);
  boost::asio::post(ioc, bindns::bind(increment, &count));
  boost::asio::post(ioc, bindns::bind(increment, &count));
  boost::asio::post(ioc, &throw_exception);
  boost::asio::post(ioc, bindns::bind(increment, &count));

  // No handlers can be called until run() is called.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);
  BOOST_ASIO_CHECK(exception_count == 0);

  for (;;)
  {
    try
    {
      ioc.run();
      break;
    }
    catch (int)
    {
      ++exception_count;
    }
  }

  // The run() calls will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 3);
  BOOST_ASIO_CHECK(exception_count == 2);
}

class test_service : public boost::asio::io_context::service
{
public:
  static boost::asio::io_context::id id;
  test_service(boost::asio::io_context& s)
    : boost::asio::io_context::service(s) {}
private:
  virtual void shutdown_service() {}
};

boost::asio::io_context::id test_service::id;

void io_context_service_test()
{
  boost::asio::io_context ioc1;
  boost::asio::io_context ioc2;
  boost::asio::io_context ioc3;

  // Implicit service registration.

  boost::asio::use_service<test_service>(ioc1);

  BOOST_ASIO_CHECK(boost::asio::has_service<test_service>(ioc1));

  test_service* svc1 = new test_service(ioc1);
  try
  {
    boost::asio::add_service(ioc1, svc1);
    BOOST_ASIO_ERROR("add_service did not throw");
  }
  catch (boost::asio::service_already_exists&)
  {
  }
  delete svc1;

  // Explicit service registration.

  test_service* svc2 = new test_service(ioc2);
  boost::asio::add_service(ioc2, svc2);

  BOOST_ASIO_CHECK(boost::asio::has_service<test_service>(ioc2));
  BOOST_ASIO_CHECK(&boost::asio::use_service<test_service>(ioc2) == svc2);

  test_service* svc3 = new test_service(ioc2);
  try
  {
    boost::asio::add_service(ioc2, svc3);
    BOOST_ASIO_ERROR("add_service did not throw");
  }
  catch (boost::asio::service_already_exists&)
  {
  }
  delete svc3;

  // Explicit registration with invalid owner.

  test_service* svc4 = new test_service(ioc2);
  try
  {
    boost::asio::add_service(ioc3, svc4);
    BOOST_ASIO_ERROR("add_service did not throw");
  }
  catch (boost::asio::invalid_service_owner&)
  {
  }
  delete svc4;

  BOOST_ASIO_CHECK(!boost::asio::has_service<test_service>(ioc3));
}

void io_context_executor_query_test()
{
  io_context ioc;

  BOOST_ASIO_CHECK(
      &boost::asio::query(ioc.get_executor(),
        boost::asio::execution::context)
      == &ioc);

  BOOST_ASIO_CHECK(
      boost::asio::query(ioc.get_executor(),
        boost::asio::execution::blocking)
      == boost::asio::execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ioc.get_executor(),
        boost::asio::execution::blocking.possibly)
      == boost::asio::execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(ioc.get_executor(),
        boost::asio::execution::outstanding_work)
      == boost::asio::execution::outstanding_work.untracked);

  BOOST_ASIO_CHECK(
      boost::asio::query(ioc.get_executor(),
        boost::asio::execution::outstanding_work.untracked)
      == boost::asio::execution::outstanding_work.untracked);

  BOOST_ASIO_CHECK(
      boost::asio::query(ioc.get_executor(),
        boost::asio::execution::relationship)
      == boost::asio::execution::relationship.fork);

  BOOST_ASIO_CHECK(
      boost::asio::query(ioc.get_executor(),
        boost::asio::execution::relationship.fork)
      == boost::asio::execution::relationship.fork);

  BOOST_ASIO_CHECK(
      boost::asio::query(ioc.get_executor(),
        boost::asio::execution::mapping)
      == boost::asio::execution::mapping.thread);

  BOOST_ASIO_CHECK(
      boost::asio::query(ioc.get_executor(),
        boost::asio::execution::allocator)
      == std::allocator<void>());
}

void io_context_executor_execute_test()
{
  io_context ioc;
  int count = 0;

  boost::asio::execution::execute(ioc.get_executor(),
      bindns::bind(increment, &count));

  // No handlers can be called until run() is called.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);

  ioc.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  ioc.restart();
  boost::asio::execution::execute(
      boost::asio::require(ioc.get_executor(),
        boost::asio::execution::blocking.possibly),
      bindns::bind(increment, &count));

  // No handlers can be called until run() is called.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);

  ioc.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  ioc.restart();
  boost::asio::execution::execute(
      boost::asio::require(ioc.get_executor(),
        boost::asio::execution::blocking.never),
      bindns::bind(increment, &count));

  // No handlers can be called until run() is called.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);

  ioc.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  ioc.restart();
  BOOST_ASIO_CHECK(!ioc.stopped());

  boost::asio::execution::execute(
      boost::asio::require(ioc.get_executor(),
        boost::asio::execution::blocking.never,
        boost::asio::execution::outstanding_work.tracked),
      bindns::bind(increment, &count));

  // No handlers can be called until run() is called.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);

  ioc.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  ioc.restart();
  boost::asio::execution::execute(
      boost::asio::require(ioc.get_executor(),
        boost::asio::execution::blocking.never,
        boost::asio::execution::outstanding_work.untracked),
      bindns::bind(increment, &count));

  // No handlers can be called until run() is called.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);

  ioc.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  ioc.restart();
  boost::asio::execution::execute(
      boost::asio::require(ioc.get_executor(),
        boost::asio::execution::blocking.never,
        boost::asio::execution::outstanding_work.untracked,
        boost::asio::execution::relationship.fork),
      bindns::bind(increment, &count));

  // No handlers can be called until run() is called.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);

  ioc.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  ioc.restart();
  boost::asio::execution::execute(
      boost::asio::require(ioc.get_executor(),
        boost::asio::execution::blocking.never,
        boost::asio::execution::outstanding_work.untracked,
        boost::asio::execution::relationship.continuation),
      bindns::bind(increment, &count));

  // No handlers can be called until run() is called.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);

  ioc.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  ioc.restart();
  boost::asio::execution::execute(
      boost::asio::prefer(
        boost::asio::require(ioc.get_executor(),
          boost::asio::execution::blocking.never,
          boost::asio::execution::outstanding_work.untracked,
          boost::asio::execution::relationship.continuation),
        boost::asio::execution::allocator(std::allocator<void>())),
      bindns::bind(increment, &count));

  // No handlers can be called until run() is called.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);

  ioc.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 1);

  count = 0;
  ioc.restart();
  boost::asio::execution::execute(
      boost::asio::prefer(
        boost::asio::require(ioc.get_executor(),
          boost::asio::execution::blocking.never,
          boost::asio::execution::outstanding_work.untracked,
          boost::asio::execution::relationship.continuation),
        boost::asio::execution::allocator),
      bindns::bind(increment, &count));

  // No handlers can be called until run() is called.
  BOOST_ASIO_CHECK(!ioc.stopped());
  BOOST_ASIO_CHECK(count == 0);

  ioc.run();

  // The run() call will not return until all work has finished.
  BOOST_ASIO_CHECK(ioc.stopped());
  BOOST_ASIO_CHECK(count == 1);
}

BOOST_ASIO_TEST_SUITE
(
  "io_context",
  BOOST_ASIO_TEST_CASE(io_context_test)
  BOOST_ASIO_TEST_CASE(io_context_service_test)
  BOOST_ASIO_TEST_CASE(io_context_executor_query_test)
  BOOST_ASIO_TEST_CASE(io_context_executor_execute_test)
)
