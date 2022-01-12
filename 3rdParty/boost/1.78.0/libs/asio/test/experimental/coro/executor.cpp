//
// experimental/coro/executor.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2021 Klemens D. Morgenstern
//                    (klemens dot morgenstern at gmx dot net)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include <boost/asio/experimental/coro.hpp>

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/io_context.hpp>
#include "../../unit_test.hpp"

using namespace boost::asio::experimental;

namespace coro {

#define BOOST_ASIO_CHECKPOINT() \
  BOOST_ASIO_TEST_IOSTREAM << __FILE__ << "(" << __LINE__ << "): " \
  << boost::asio::detail::test_name() << ": " \
  << "Checkpoint" << std::endl;

template <typename T>
void different_execs()
{
  boost::asio::io_context ctx;
  boost::asio::thread_pool th_ctx{1u};

  auto o = std::make_optional(
      boost::asio::prefer(th_ctx.get_executor(),
        boost::asio::execution::outstanding_work.tracked));

  static bool ran_inner = false, ran_outer = false;

  struct c_inner_t
  {
    auto operator()(boost::asio::any_io_executor e) -> boost::asio::experimental::coro<T>
    {
      auto p = e.target<boost::asio::thread_pool::executor_type>();
      BOOST_ASIO_CHECKPOINT();
      BOOST_ASIO_CHECK(p);
      BOOST_ASIO_CHECK(p->running_in_this_thread());
      ran_inner = true;
      co_return;
    };

  };

  c_inner_t c_inner;

  struct c_outer_t
  {

    auto operator()(boost::asio::any_io_executor e, int,
        boost::asio::experimental::coro<T> tp)
      -> boost::asio::experimental::coro<void>
    {
      auto p = e.target<boost::asio::io_context::executor_type>();

      BOOST_ASIO_CHECK(p);
      BOOST_ASIO_CHECK(p->running_in_this_thread());
      BOOST_ASIO_CHECKPOINT();

      co_await tp;

      BOOST_ASIO_CHECKPOINT();
      BOOST_ASIO_CHECK(p->running_in_this_thread());

      ran_outer = true;
    };
  };

  c_outer_t c_outer;

  bool ran = false;
  std::exception_ptr ex;

  auto c = c_outer(ctx.get_executor(), 10, c_inner(th_ctx.get_executor()));
  c.async_resume(
      [&](std::exception_ptr e)
      {
        BOOST_ASIO_CHECK(!e);
        BOOST_ASIO_CHECKPOINT();
        ran = true;
      });

  BOOST_ASIO_CHECK(!ran);
  ctx.run();
  o.reset();
  BOOST_ASIO_CHECK(ran);
  BOOST_ASIO_CHECK(ran_inner);
  BOOST_ASIO_CHECK(ran_outer);
  BOOST_ASIO_CHECK(!ex);

  th_ctx.stop();
  th_ctx.join();
}

} // namespace coro

BOOST_ASIO_TEST_SUITE
(
  "coro/partial",
  BOOST_ASIO_TEST_CASE(::coro::different_execs<void>)
  BOOST_ASIO_TEST_CASE(::coro::different_execs<int()>)
)
