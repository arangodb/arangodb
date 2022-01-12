//
// experimental/coro/cancel.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
#include <iostream>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include "../../unit_test.hpp"

using namespace boost::asio::experimental;
namespace this_coro = boost::asio::this_coro;

namespace coro {


auto coro_simple_cancel_impl(boost::asio::io_context& ) noexcept
  -> boost::asio::experimental::coro<void() noexcept, boost::system::error_code>
{
    BOOST_ASIO_CHECK(
        !(co_await this_coro::cancellation_state).cancelled());

    boost::asio::steady_timer timer{
        co_await this_coro::executor,
        std::chrono::seconds(1)};

    BOOST_ASIO_CHECK(
        !(co_await this_coro::cancellation_state).cancelled());

    auto ec = co_await timer;

    BOOST_ASIO_CHECK(
        (co_await this_coro::cancellation_state).cancelled());

    co_return ec;
}

void coro_simple_cancel()
{
  boost::asio::io_context ctx;
  boost::asio::cancellation_signal sig;

  auto k = coro_simple_cancel_impl(ctx);

  boost::system::error_code res_ec;
  k.async_resume(
      boost::asio::bind_cancellation_slot(sig.slot(),
        [&](boost::system::error_code ec) {res_ec = ec;}));
  boost::asio::post(ctx, [&]{sig.emit(boost::asio::cancellation_type::all);});

  BOOST_ASIO_CHECK(!res_ec);

  ctx.run();

  BOOST_ASIO_CHECK(res_ec == boost::asio::error::operation_aborted);
}

auto coro_throw_cancel_impl(boost::asio::io_context& )
  -> boost::asio::experimental::coro<void() , void>
{
    boost::asio::steady_timer timer{
        co_await this_coro::executor,
        std::chrono::seconds(1)};
    co_await timer;
}

void coro_throw_cancel()
{
  boost::asio::io_context ctx;
  boost::asio::cancellation_signal sig;

  auto k = coro_throw_cancel_impl(ctx);

  std::exception_ptr res_ex;
  k.async_resume(
      boost::asio::bind_cancellation_slot(sig.slot(),
        [&](std::exception_ptr ex) {res_ex = ex;}));
  boost::asio::post(ctx, [&]{sig.emit(boost::asio::cancellation_type::all);});

  BOOST_ASIO_CHECK(!res_ex);

  ctx.run();

  BOOST_ASIO_CHECK(res_ex);
  try
  {
    if (res_ex)
      std::rethrow_exception(res_ex);
  }
  catch (boost::system::system_error& se)
  {
    BOOST_ASIO_CHECK(se.code() == boost::asio::error::operation_aborted);
  }
}

auto coro_simple_cancel_nested_k(boost::asio::io_context&, int& cnt) noexcept
  -> boost::asio::experimental::coro<
      void() noexcept,
      boost::system::error_code>
{
  boost::asio::steady_timer timer{
          co_await this_coro::executor,
          std::chrono::milliseconds(100)};

  BOOST_ASIO_CHECK(!(co_await this_coro::cancellation_state).cancelled());
  auto ec = co_await timer;
  cnt++;
  BOOST_ASIO_CHECK((co_await this_coro::cancellation_state).cancelled());

  co_return ec;
}

auto coro_simple_cancel_nested_kouter(
    boost::asio::io_context& ctx, int& cnt) noexcept
  -> boost::asio::experimental::coro<
      boost::system::error_code() noexcept,
      boost::system::error_code>
{
    BOOST_ASIO_CHECK(cnt == 0);
    co_yield co_await coro_simple_cancel_nested_k(ctx, cnt);
    BOOST_ASIO_CHECK(cnt == 1);
    auto ec = co_await coro_simple_cancel_nested_k(ctx, cnt);
    BOOST_ASIO_CHECK(cnt == 2);
    co_return ec;
}

void coro_simple_cancel_nested()
{
  boost::asio::io_context ctx;
  boost::asio::cancellation_signal sig;

  int cnt = 0;
  auto kouter = coro_simple_cancel_nested_kouter(ctx, cnt);

  boost::system::error_code res_ec;
  kouter.async_resume(
      boost::asio::bind_cancellation_slot(sig.slot(),
        [&](boost::system::error_code ec) {res_ec = ec;}));
  boost::asio::post(ctx, [&]{sig.emit(boost::asio::cancellation_type::all);});
  BOOST_ASIO_CHECK(!res_ec);
  ctx.run();
  BOOST_ASIO_CHECK(res_ec == boost::asio::error::operation_aborted);

  ctx.restart();
  res_ec = {};
  kouter.async_resume(
      boost::asio::bind_cancellation_slot(sig.slot(),
        [&](boost::system::error_code ec) {res_ec = ec;}));
  boost::asio::post(ctx, [&]{sig.emit(boost::asio::cancellation_type::all);});
  BOOST_ASIO_CHECK(!res_ec);
  ctx.run();
  BOOST_ASIO_CHECK(res_ec == boost::asio::error::operation_aborted);
  BOOST_ASIO_CHECK(cnt == 2);
}

} // namespace coro

BOOST_ASIO_TEST_SUITE
(
  "coro/cancel",
  BOOST_ASIO_TEST_CASE(::coro::coro_simple_cancel)
  BOOST_ASIO_TEST_CASE(::coro::coro_throw_cancel)
  BOOST_ASIO_TEST_CASE(::coro::coro_simple_cancel_nested)
)
