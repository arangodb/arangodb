//
// io_context.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include "asio/io_context.hpp"

#include <sstream>
#include "asio/bind_executor.hpp"
#include "asio/dispatch.hpp"
#include "asio/post.hpp"
#include "asio/thread.hpp"
#include "unit_test.hpp"

#if defined(ASIO_HAS_BOOST_DATE_TIME)
# include "asio/deadline_timer.hpp"
#else // defined(ASIO_HAS_BOOST_DATE_TIME)
# include "asio/steady_timer.hpp"
#endif // defined(ASIO_HAS_BOOST_DATE_TIME)

#if defined(ASIO_HAS_BOOST_BIND)
# include <boost/bind.hpp>
#else // defined(ASIO_HAS_BOOST_BIND)
# include <functional>
#endif // defined(ASIO_HAS_BOOST_BIND)

using namespace asio;

#if defined(ASIO_HAS_BOOST_BIND)
namespace bindns = boost;
#else // defined(ASIO_HAS_BOOST_BIND)
namespace bindns = std;
#endif

#if defined(ASIO_HAS_BOOST_DATE_TIME)
typedef deadline_timer timer;
namespace chronons = boost::posix_time;
#elif defined(ASIO_HAS_CHRONO)
typedef steady_timer timer;
namespace chronons = asio::chrono;
#endif // defined(ASIO_HAS_BOOST_DATE_TIME)

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
    asio::post(*ioc, bindns::bind(decrement_to_zero, ioc, count));

    // Handler execution cannot nest, so count value should remain unchanged.
    ASIO_CHECK(*count == before_value);
  }
}

void nested_decrement_to_zero(io_context* ioc, int* count)
{
  if (*count > 0)
  {
    --(*count);

    asio::dispatch(*ioc,
        bindns::bind(nested_decrement_to_zero, ioc, count));

    // Handler execution is nested, so count value should now be zero.
    ASIO_CHECK(*count == 0);
  }
}

void sleep_increment(io_context* ioc, int* count)
{
  timer t(*ioc, chronons::seconds(2));
  t.wait();

  if (++(*count) < 3)
    asio::post(*ioc, bindns::bind(sleep_increment, ioc, count));
}

void start_sleep_increments(io_context* ioc, int* count)
{
  // Give all threads a chance to start.
  timer t(*ioc, chronons::seconds(2));
  t.wait();

  // Start the first of three increments.
  asio::post(*ioc, bindns::bind(sleep_increment, ioc, count));
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

  asio::post(ioc, bindns::bind(increment, &count));

  // No handlers can be called until run() is called.
  ASIO_CHECK(!ioc.stopped());
  ASIO_CHECK(count == 0);

  ioc.run();

  // The run() call will not return until all work has finished.
  ASIO_CHECK(ioc.stopped());
  ASIO_CHECK(count == 1);

  count = 0;
  ioc.restart();
  asio::post(ioc, bindns::bind(increment, &count));
  asio::post(ioc, bindns::bind(increment, &count));
  asio::post(ioc, bindns::bind(increment, &count));
  asio::post(ioc, bindns::bind(increment, &count));
  asio::post(ioc, bindns::bind(increment, &count));

  // No handlers can be called until run() is called.
  ASIO_CHECK(!ioc.stopped());
  ASIO_CHECK(count == 0);

  ioc.run();

  // The run() call will not return until all work has finished.
  ASIO_CHECK(ioc.stopped());
  ASIO_CHECK(count == 5);

  count = 0;
  ioc.restart();
  executor_work_guard<io_context::executor_type> w = make_work_guard(ioc);
  asio::post(ioc, bindns::bind(&io_context::stop, &ioc));
  ASIO_CHECK(!ioc.stopped());
  ioc.run();

  // The only operation executed should have been to stop run().
  ASIO_CHECK(ioc.stopped());
  ASIO_CHECK(count == 0);

  ioc.restart();
  asio::post(ioc, bindns::bind(increment, &count));
  w.reset();

  // No handlers can be called until run() is called.
  ASIO_CHECK(!ioc.stopped());
  ASIO_CHECK(count == 0);

  ioc.run();

  // The run() call will not return until all work has finished.
  ASIO_CHECK(ioc.stopped());
  ASIO_CHECK(count == 1);

  count = 10;
  ioc.restart();
  asio::post(ioc, bindns::bind(decrement_to_zero, &ioc, &count));

  // No handlers can be called until run() is called.
  ASIO_CHECK(!ioc.stopped());
  ASIO_CHECK(count == 10);

  ioc.run();

  // The run() call will not return until all work has finished.
  ASIO_CHECK(ioc.stopped());
  ASIO_CHECK(count == 0);

  count = 10;
  ioc.restart();
  asio::post(ioc, bindns::bind(nested_decrement_to_zero, &ioc, &count));

  // No handlers can be called until run() is called.
  ASIO_CHECK(!ioc.stopped());
  ASIO_CHECK(count == 10);

  ioc.run();

  // The run() call will not return until all work has finished.
  ASIO_CHECK(ioc.stopped());
  ASIO_CHECK(count == 0);

  count = 10;
  ioc.restart();
  asio::dispatch(ioc,
      bindns::bind(nested_decrement_to_zero, &ioc, &count));

  // No handlers can be called until run() is called, even though nested
  // delivery was specifically allowed in the previous call.
  ASIO_CHECK(!ioc.stopped());
  ASIO_CHECK(count == 10);

  ioc.run();

  // The run() call will not return until all work has finished.
  ASIO_CHECK(ioc.stopped());
  ASIO_CHECK(count == 0);

  count = 0;
  int count2 = 0;
  ioc.restart();
  ASIO_CHECK(!ioc.stopped());
  asio::post(ioc, bindns::bind(start_sleep_increments, &ioc, &count));
  asio::post(ioc, bindns::bind(start_sleep_increments, &ioc, &count2));
  thread thread1(bindns::bind(io_context_run, &ioc));
  thread thread2(bindns::bind(io_context_run, &ioc));
  thread1.join();
  thread2.join();

  // The run() calls will not return until all work has finished.
  ASIO_CHECK(ioc.stopped());
  ASIO_CHECK(count == 3);
  ASIO_CHECK(count2 == 3);

  count = 10;
  io_context ioc2;
  asio::dispatch(ioc, asio::bind_executor(ioc2,
        bindns::bind(decrement_to_zero, &ioc2, &count)));
  ioc.restart();
  ASIO_CHECK(!ioc.stopped());
  ioc.run();

  // No decrement_to_zero handlers can be called until run() is called on the
  // second io_context object.
  ASIO_CHECK(ioc.stopped());
  ASIO_CHECK(count == 10);

  ioc2.run();

  // The run() call will not return until all work has finished.
  ASIO_CHECK(count == 0);

  count = 0;
  int exception_count = 0;
  ioc.restart();
  asio::post(ioc, &throw_exception);
  asio::post(ioc, bindns::bind(increment, &count));
  asio::post(ioc, bindns::bind(increment, &count));
  asio::post(ioc, &throw_exception);
  asio::post(ioc, bindns::bind(increment, &count));

  // No handlers can be called until run() is called.
  ASIO_CHECK(!ioc.stopped());
  ASIO_CHECK(count == 0);
  ASIO_CHECK(exception_count == 0);

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
  ASIO_CHECK(ioc.stopped());
  ASIO_CHECK(count == 3);
  ASIO_CHECK(exception_count == 2);
}

class test_service : public asio::io_context::service
{
public:
  static asio::io_context::id id;
  test_service(asio::io_context& s)
    : asio::io_context::service(s) {}
private:
  virtual void shutdown_service() {}
};

asio::io_context::id test_service::id;

void io_context_service_test()
{
  asio::io_context ioc1;
  asio::io_context ioc2;
  asio::io_context ioc3;

  // Implicit service registration.

  asio::use_service<test_service>(ioc1);

  ASIO_CHECK(asio::has_service<test_service>(ioc1));

  test_service* svc1 = new test_service(ioc1);
  try
  {
    asio::add_service(ioc1, svc1);
    ASIO_ERROR("add_service did not throw");
  }
  catch (asio::service_already_exists&)
  {
  }
  delete svc1;

  // Explicit service registration.

  test_service* svc2 = new test_service(ioc2);
  asio::add_service(ioc2, svc2);

  ASIO_CHECK(asio::has_service<test_service>(ioc2));
  ASIO_CHECK(&asio::use_service<test_service>(ioc2) == svc2);

  test_service* svc3 = new test_service(ioc2);
  try
  {
    asio::add_service(ioc2, svc3);
    ASIO_ERROR("add_service did not throw");
  }
  catch (asio::service_already_exists&)
  {
  }
  delete svc3;

  // Explicit registration with invalid owner.

  test_service* svc4 = new test_service(ioc2);
  try
  {
    asio::add_service(ioc3, svc4);
    ASIO_ERROR("add_service did not throw");
  }
  catch (asio::invalid_service_owner&)
  {
  }
  delete svc4;

  ASIO_CHECK(!asio::has_service<test_service>(ioc3));
}

ASIO_TEST_SUITE
(
  "io_context",
  ASIO_TEST_CASE(io_context_test)
  ASIO_TEST_CASE(io_context_service_test)
)
