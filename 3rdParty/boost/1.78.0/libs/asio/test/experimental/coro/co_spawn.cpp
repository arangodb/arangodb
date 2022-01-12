//
// experimental/coro/co_spawn.cpp
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
#include <boost/asio/experimental/co_spawn.hpp>

#include <iostream>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include "../../unit_test.hpp"

using namespace boost::asio::experimental;
namespace this_coro = boost::asio::this_coro;

namespace coro {

auto coro_simple_co_spawn_impl(boost::asio::io_context& , bool &done) noexcept
  -> boost::asio::experimental::coro<void() noexcept, int>
{
  boost::asio::steady_timer timer(
      co_await this_coro::executor,
      std::chrono::milliseconds(10));

  done = true;

  co_return 42;
}

void coro_co_spawn()
{
  boost::asio::io_context ctx;

  bool done1 = false;
  bool done2 = false;
  int res = 0;

  co_spawn(coro_simple_co_spawn_impl(ctx, done1),
      [&](int r){done2= true;  res = r;});

  ctx.run();

  BOOST_ASIO_CHECK(done1);
  BOOST_ASIO_CHECK(done2);
  BOOST_ASIO_CHECK(res == 42);
}

} // namespace coro

BOOST_ASIO_TEST_SUITE
(
  "coro/co_spawn",
  BOOST_ASIO_TEST_CASE(::coro::coro_co_spawn)
)
