//
// experimental/coro/use_coro.cpp
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
#include <boost/asio/experimental/use_coro.hpp>

#include <boost/asio/steady_timer.hpp>
#include <iostream>
#include "../../unit_test.hpp"

using namespace boost::asio::experimental;

namespace coro {

boost::asio::experimental::coro<void(), int>
awaiter(boost::asio::any_io_executor exec)
{
  boost::asio::steady_timer timer{exec};
  co_await timer.async_wait(use_coro);
  co_return 42;
}

boost::asio::experimental::coro<void() noexcept, int>
awaiter_noexcept(boost::asio::any_io_executor exec)
{
  boost::asio::steady_timer timer{exec};
  auto ec = co_await timer.async_wait(use_coro);
  BOOST_ASIO_CHECK(ec == boost::system::error_code{});
  co_return 42;
}

void stack_test2()
{
  bool done = false;
  boost::asio::io_context ctx;

  auto k = awaiter(ctx.get_executor());
  auto k2 = awaiter_noexcept(ctx.get_executor());

  k.async_resume(
      [&](std::exception_ptr ex, int res)
      {
        BOOST_ASIO_CHECK(!ex);
        BOOST_ASIO_CHECK(res == 42);
        done = true;
      });

  k2.async_resume([&](int res)
       {
         BOOST_ASIO_CHECK(res == 42);
         done = true;
       });

  ctx.run();
  BOOST_ASIO_CHECK(done);
}

} // namespace coro

BOOST_ASIO_TEST_SUITE
(
  "coro/use_coro",
  BOOST_ASIO_TEST_CASE(::coro::stack_test2)
)
