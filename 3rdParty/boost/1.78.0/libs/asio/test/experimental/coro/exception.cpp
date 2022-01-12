//
// experimental/coro/exception.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/awaitable.hpp>
#include "../../unit_test.hpp"

using namespace boost::asio::experimental;

namespace coro {

template <typename Func>
struct on_scope_exit
{
	Func func;

  static_assert(noexcept(func()));

  on_scope_exit(const Func &f)
    : func(static_cast< Func && >(f))
	{
	}

	on_scope_exit(Func &&f)
    : func(f)
	{
	}

  on_scope_exit(const on_scope_exit &) = delete;

	~on_scope_exit()
  {
    func();
  }
};

boost::asio::experimental::coro<int> throwing_generator(
  boost::asio::any_io_executor, int &last, bool &destroyed)
{
  on_scope_exit x = [&]() noexcept { destroyed = true; };
  (void)x;

  int i = 0;
  while (i < 3)
    co_yield last = ++i;

  throw std::runtime_error("throwing-generator");
}

boost::asio::awaitable<void> throwing_generator_test()
{
  int val = 0;
  bool destr = false;
  {
    auto gi = throwing_generator(
        co_await boost::asio::this_coro::executor,
        val, destr);
    bool caught = false;
    try
    {
      for (int i = 0; i < 10; i++)
      {
        BOOST_ASIO_CHECK(val == i);
        const auto next = co_await gi.async_resume(boost::asio::use_awaitable);
        BOOST_ASIO_CHECK(next);
        BOOST_ASIO_CHECK(val == *next);
        BOOST_ASIO_CHECK(val == i + 1);
      }
    }
    catch (std::runtime_error &err)
    {
      caught = true;
      using std::operator ""sv;
      BOOST_ASIO_CHECK(err.what() == "throwing-generator"sv);
    }
    BOOST_ASIO_CHECK(val == 3);
    BOOST_ASIO_CHECK(caught);
  }
  BOOST_ASIO_CHECK(destr);
};

void run_throwing_generator_test()
{
  boost::asio::io_context ctx;
  boost::asio::co_spawn(ctx, throwing_generator_test(), boost::asio::detached);
  ctx.run();
}

boost::asio::experimental::coro<int(int)> throwing_stacked(
    boost::asio::any_io_executor exec, int &val,
    bool &destroyed_inner, bool &destroyed)
{
  BOOST_ASIO_CHECK((co_await boost::asio::this_coro::throw_if_cancelled()));

  on_scope_exit x = [&]() noexcept { destroyed = true; };
  (void)x;

  auto gen = throwing_generator(exec, val, destroyed_inner);
  while (auto next = co_await gen) // 1, 2, 4, 8, ...
    BOOST_ASIO_CHECK(42 ==(co_yield *next)); // offset is delayed by one cycle
}

boost::asio::awaitable<void> throwing_generator_stacked_test()
{
  int val = 0;
  bool destr = false, destr_inner = false;
  {
    auto gi = throwing_stacked(
        co_await boost::asio::this_coro::executor,
        val, destr, destr_inner);
    bool caught = false;
    try
    {
      for (int i = 0; i < 10; i++)
      {
        BOOST_ASIO_CHECK(val == i);
        const auto next =
          co_await gi.async_resume(42, boost::asio::use_awaitable);
        BOOST_ASIO_CHECK(next);
        BOOST_ASIO_CHECK(val == *next);
        BOOST_ASIO_CHECK(val == i + 1);
      }
    }
    catch (std::runtime_error &err)
    {
      caught = true;
      using std::operator ""sv;
      BOOST_ASIO_CHECK(err.what() == "throwing-generator"sv);
    }
    BOOST_ASIO_CHECK(val == 3);
    BOOST_ASIO_CHECK(caught);
  }
  BOOST_ASIO_CHECK(destr);
  BOOST_ASIO_CHECK(destr_inner);
};

void run_throwing_generator_stacked_test()
{
  boost::asio::io_context ctx;
  boost::asio::co_spawn(ctx,
      throwing_generator_stacked_test,
      boost::asio::detached);
  ctx.run();
}

} // namespace coro

BOOST_ASIO_TEST_SUITE
(
  "coro/exception",
  BOOST_ASIO_TEST_CASE(::coro::run_throwing_generator_stacked_test)
  BOOST_ASIO_TEST_CASE(::coro::run_throwing_generator_test)
)
