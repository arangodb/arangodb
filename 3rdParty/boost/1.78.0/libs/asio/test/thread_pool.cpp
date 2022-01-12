//
// thread_pool.cpp
// ~~~~~~~~~~~~~~~
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
#include <boost/asio/thread_pool.hpp>

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

void increment(int* count)
{
  ++(*count);
}

void decrement_to_zero(thread_pool* pool, int* count)
{
  if (*count > 0)
  {
    --(*count);

    int before_value = *count;
    boost::asio::post(*pool, bindns::bind(decrement_to_zero, pool, count));

    // Handler execution cannot nest, so count value should remain unchanged.
    BOOST_ASIO_CHECK(*count == before_value);
  }
}

void nested_decrement_to_zero(thread_pool* pool, int* count)
{
  if (*count > 0)
  {
    --(*count);

    boost::asio::dispatch(*pool,
        bindns::bind(nested_decrement_to_zero, pool, count));

    // Handler execution is nested, so count value should now be zero.
    BOOST_ASIO_CHECK(*count == 0);
  }
}

void thread_pool_test()
{
  thread_pool pool(1);

  int count1 = 0;
  boost::asio::post(pool, bindns::bind(increment, &count1));

  int count2 = 10;
  boost::asio::post(pool, bindns::bind(decrement_to_zero, &pool, &count2));

  int count3 = 10;
  boost::asio::post(pool, bindns::bind(nested_decrement_to_zero, &pool, &count3));

  pool.wait();

  BOOST_ASIO_CHECK(count1 == 1);
  BOOST_ASIO_CHECK(count2 == 0);
  BOOST_ASIO_CHECK(count3 == 0);
}

class test_service : public boost::asio::execution_context::service
{
public:
#if defined(BOOST_ASIO_NO_TYPEID)
  static boost::asio::execution_context::id id;
#endif // defined(BOOST_ASIO_NO_TYPEID)

  typedef test_service key_type;

  test_service(boost::asio::execution_context& ctx)
    : boost::asio::execution_context::service(ctx)
  {
  }

private:
  virtual void shutdown() {}
};

#if defined(BOOST_ASIO_NO_TYPEID)
boost::asio::execution_context::id test_service::id;
#endif // defined(BOOST_ASIO_NO_TYPEID)

void thread_pool_service_test()
{
  boost::asio::thread_pool pool1(1);
  boost::asio::thread_pool pool2(1);
  boost::asio::thread_pool pool3(1);

  // Implicit service registration.

  boost::asio::use_service<test_service>(pool1);

  BOOST_ASIO_CHECK(boost::asio::has_service<test_service>(pool1));

  test_service* svc1 = new test_service(pool1);
  try
  {
    boost::asio::add_service(pool1, svc1);
    BOOST_ASIO_ERROR("add_service did not throw");
  }
  catch (boost::asio::service_already_exists&)
  {
  }
  delete svc1;

  // Explicit service registration.

  test_service& svc2 = boost::asio::make_service<test_service>(pool2);

  BOOST_ASIO_CHECK(boost::asio::has_service<test_service>(pool2));
  BOOST_ASIO_CHECK(&boost::asio::use_service<test_service>(pool2) == &svc2);

  test_service* svc3 = new test_service(pool2);
  try
  {
    boost::asio::add_service(pool2, svc3);
    BOOST_ASIO_ERROR("add_service did not throw");
  }
  catch (boost::asio::service_already_exists&)
  {
  }
  delete svc3;

  // Explicit registration with invalid owner.

  test_service* svc4 = new test_service(pool2);
  try
  {
    boost::asio::add_service(pool3, svc4);
    BOOST_ASIO_ERROR("add_service did not throw");
  }
  catch (boost::asio::invalid_service_owner&)
  {
  }
  delete svc4;

  BOOST_ASIO_CHECK(!boost::asio::has_service<test_service>(pool3));
}

void thread_pool_executor_query_test()
{
  thread_pool pool(1);

  BOOST_ASIO_CHECK(
      &boost::asio::query(pool.executor(),
        boost::asio::execution::context)
      == &pool);

  BOOST_ASIO_CHECK(
      boost::asio::query(pool.executor(),
        boost::asio::execution::blocking)
      == boost::asio::execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(pool.executor(),
        boost::asio::execution::blocking.possibly)
      == boost::asio::execution::blocking.possibly);

  BOOST_ASIO_CHECK(
      boost::asio::query(pool.executor(),
        boost::asio::execution::outstanding_work)
      == boost::asio::execution::outstanding_work.untracked);

  BOOST_ASIO_CHECK(
      boost::asio::query(pool.executor(),
        boost::asio::execution::outstanding_work.untracked)
      == boost::asio::execution::outstanding_work.untracked);

  BOOST_ASIO_CHECK(
      boost::asio::query(pool.executor(),
        boost::asio::execution::relationship)
      == boost::asio::execution::relationship.fork);

  BOOST_ASIO_CHECK(
      boost::asio::query(pool.executor(),
        boost::asio::execution::relationship.fork)
      == boost::asio::execution::relationship.fork);

  BOOST_ASIO_CHECK(
      boost::asio::query(pool.executor(),
        boost::asio::execution::bulk_guarantee)
      == boost::asio::execution::bulk_guarantee.parallel);

  BOOST_ASIO_CHECK(
      boost::asio::query(pool.executor(),
        boost::asio::execution::mapping)
      == boost::asio::execution::mapping.thread);

  BOOST_ASIO_CHECK(
      boost::asio::query(pool.executor(),
        boost::asio::execution::allocator)
      == std::allocator<void>());

  BOOST_ASIO_CHECK(
      boost::asio::query(pool.executor(),
        boost::asio::execution::occupancy)
      == 1);
}

void thread_pool_executor_execute_test()
{
  int count = 0;
  thread_pool pool(1);

  boost::asio::execution::execute(pool.executor(),
      bindns::bind(increment, &count));

  boost::asio::execution::execute(
      boost::asio::require(pool.executor(),
        boost::asio::execution::blocking.possibly),
      bindns::bind(increment, &count));

  boost::asio::execution::execute(
      boost::asio::require(pool.executor(),
        boost::asio::execution::blocking.always),
      bindns::bind(increment, &count));

  boost::asio::execution::execute(
      boost::asio::require(pool.executor(),
        boost::asio::execution::blocking.never),
      bindns::bind(increment, &count));

  boost::asio::execution::execute(
      boost::asio::require(pool.executor(),
        boost::asio::execution::blocking.never,
        boost::asio::execution::outstanding_work.tracked),
      bindns::bind(increment, &count));

  boost::asio::execution::execute(
      boost::asio::require(pool.executor(),
        boost::asio::execution::blocking.never,
        boost::asio::execution::outstanding_work.untracked),
      bindns::bind(increment, &count));

  boost::asio::execution::execute(
      boost::asio::require(pool.executor(),
        boost::asio::execution::blocking.never,
        boost::asio::execution::outstanding_work.untracked,
        boost::asio::execution::relationship.fork),
      bindns::bind(increment, &count));

  boost::asio::execution::execute(
      boost::asio::require(pool.executor(),
        boost::asio::execution::blocking.never,
        boost::asio::execution::outstanding_work.untracked,
        boost::asio::execution::relationship.continuation),
      bindns::bind(increment, &count));

  boost::asio::execution::execute(
      boost::asio::prefer(
        boost::asio::require(pool.executor(),
          boost::asio::execution::blocking.never,
          boost::asio::execution::outstanding_work.untracked,
          boost::asio::execution::relationship.continuation),
        boost::asio::execution::allocator(std::allocator<void>())),
      bindns::bind(increment, &count));

  boost::asio::execution::execute(
      boost::asio::prefer(
        boost::asio::require(pool.executor(),
          boost::asio::execution::blocking.never,
          boost::asio::execution::outstanding_work.untracked,
          boost::asio::execution::relationship.continuation),
        boost::asio::execution::allocator),
      bindns::bind(increment, &count));

  pool.wait();

  BOOST_ASIO_CHECK(count == 10);
}

struct receiver
{
  int* count_;

  receiver(int* count)
    : count_(count)
  {
  }

  receiver(const receiver& other) BOOST_ASIO_NOEXCEPT
    : count_(other.count_)
  {
  }

#if defined(BOOST_ASIO_HAS_MOVE)
  receiver(receiver&& other) BOOST_ASIO_NOEXCEPT
    : count_(other.count_)
  {
    other.count_ = 0;
  }
#endif // defined(BOOST_ASIO_HAS_MOVE)

  void set_value() BOOST_ASIO_NOEXCEPT
  {
    ++(*count_);
  }

  template <typename E>
  void set_error(BOOST_ASIO_MOVE_ARG(E) e) BOOST_ASIO_NOEXCEPT
  {
    (void)e;
  }

  void set_done() BOOST_ASIO_NOEXCEPT
  {
  }
};

namespace boost {
namespace asio {
namespace traits {

#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_VALUE_MEMBER_TRAIT)

template <>
struct set_value_member<receiver, void()>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SET_VALUE_MEMBER_TRAIT)

#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_ERROR_MEMBER_TRAIT)

template <typename E>
struct set_error_member<receiver, E>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SET_ERROR_MEMBER_TRAIT)

#if !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)

template <>
struct set_done_member<receiver>
{
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_valid = true);
  BOOST_ASIO_STATIC_CONSTEXPR(bool, is_noexcept = true);
  typedef void result_type;
};

#endif // !defined(BOOST_ASIO_HAS_DEDUCED_SET_DONE_MEMBER_TRAIT)

} // namespace traits
} // namespace asio
} // namespace boost

void thread_pool_scheduler_test()
{
  int count = 0;
  receiver r(&count);
  thread_pool pool(1);

  boost::asio::execution::submit(
    boost::asio::execution::schedule(pool.scheduler()), r);

  boost::asio::execution::submit(
      boost::asio::require(
        boost::asio::execution::schedule(pool.executor()),
        boost::asio::execution::blocking.possibly), r);

  boost::asio::execution::submit(
      boost::asio::require(
        boost::asio::execution::schedule(pool.executor()),
        boost::asio::execution::blocking.always), r);

  boost::asio::execution::submit(
      boost::asio::require(
        boost::asio::execution::schedule(pool.executor()),
        boost::asio::execution::blocking.never), r);

  boost::asio::execution::submit(
      boost::asio::require(
        boost::asio::execution::schedule(pool.executor()),
        boost::asio::execution::blocking.never,
        boost::asio::execution::outstanding_work.tracked), r);

  boost::asio::execution::submit(
      boost::asio::require(
        boost::asio::execution::schedule(pool.executor()),
        boost::asio::execution::blocking.never,
        boost::asio::execution::outstanding_work.untracked), r);

  boost::asio::execution::submit(
      boost::asio::require(
        boost::asio::execution::schedule(pool.executor()),
        boost::asio::execution::blocking.never,
        boost::asio::execution::outstanding_work.untracked,
        boost::asio::execution::relationship.fork), r);

  boost::asio::execution::submit(
      boost::asio::require(
        boost::asio::execution::schedule(pool.executor()),
        boost::asio::execution::blocking.never,
        boost::asio::execution::outstanding_work.untracked,
        boost::asio::execution::relationship.continuation), r);

  boost::asio::execution::submit(
      boost::asio::prefer(
        boost::asio::require(
          boost::asio::execution::schedule(pool.executor()),
          boost::asio::execution::blocking.never,
          boost::asio::execution::outstanding_work.untracked,
          boost::asio::execution::relationship.continuation),
        boost::asio::execution::allocator(std::allocator<void>())), r);

  boost::asio::execution::submit(
      boost::asio::prefer(
        boost::asio::require(
          boost::asio::execution::schedule(pool.executor()),
          boost::asio::execution::blocking.never,
          boost::asio::execution::outstanding_work.untracked,
          boost::asio::execution::relationship.continuation),
        boost::asio::execution::allocator), r);

  pool.wait();

  BOOST_ASIO_CHECK(count == 10);
}

void thread_pool_executor_bulk_execute_test()
{
  int count = 0;
  thread_pool pool(1);

  pool.executor().bulk_execute(
      bindns::bind(increment, &count), 2);

  boost::asio::require(pool.executor(),
    boost::asio::execution::blocking.possibly).bulk_execute(
      bindns::bind(increment, &count), 2);

  boost::asio::require(pool.executor(),
    boost::asio::execution::blocking.always).bulk_execute(
      bindns::bind(increment, &count), 2);

  boost::asio::require(pool.executor(),
    boost::asio::execution::blocking.never).bulk_execute(
      bindns::bind(increment, &count), 2);

  boost::asio::require(pool.executor(),
    boost::asio::execution::blocking.never,
    boost::asio::execution::outstanding_work.tracked).bulk_execute(
      bindns::bind(increment, &count), 2);

  boost::asio::require(pool.executor(),
    boost::asio::execution::blocking.never,
    boost::asio::execution::outstanding_work.untracked).bulk_execute(
      bindns::bind(increment, &count), 2);

  boost::asio::require(pool.executor(),
    boost::asio::execution::blocking.never,
    boost::asio::execution::outstanding_work.untracked,
    boost::asio::execution::relationship.fork).bulk_execute(
      bindns::bind(increment, &count), 2);

  boost::asio::require(pool.executor(),
    boost::asio::execution::blocking.never,
    boost::asio::execution::outstanding_work.untracked,
    boost::asio::execution::relationship.continuation).bulk_execute(
      bindns::bind(increment, &count), 2);

  boost::asio::prefer(
    boost::asio::require(pool.executor(),
      boost::asio::execution::blocking.never,
      boost::asio::execution::outstanding_work.untracked,
      boost::asio::execution::relationship.continuation),
    boost::asio::execution::allocator(std::allocator<void>())).bulk_execute(
      bindns::bind(increment, &count), 2);

  boost::asio::prefer(
    boost::asio::require(pool.executor(),
      boost::asio::execution::blocking.never,
      boost::asio::execution::outstanding_work.untracked,
      boost::asio::execution::relationship.continuation),
    boost::asio::execution::allocator).bulk_execute(
      bindns::bind(increment, &count), 2);

  pool.wait();

  BOOST_ASIO_CHECK(count == 20);
}

BOOST_ASIO_TEST_SUITE
(
  "thread_pool",
  BOOST_ASIO_TEST_CASE(thread_pool_test)
  BOOST_ASIO_TEST_CASE(thread_pool_service_test)
  BOOST_ASIO_TEST_CASE(thread_pool_executor_query_test)
  BOOST_ASIO_TEST_CASE(thread_pool_executor_execute_test)
  BOOST_ASIO_TEST_CASE(thread_pool_executor_bulk_execute_test)
  BOOST_ASIO_TEST_CASE(thread_pool_scheduler_test)
)
