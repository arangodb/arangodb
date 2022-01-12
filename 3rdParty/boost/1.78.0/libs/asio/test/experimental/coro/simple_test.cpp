//
// experimental/coro/simple_test.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
#include <iostream>
#include <vector>
#include "../../unit_test.hpp"

using namespace boost::asio::experimental;

namespace boost {
namespace asio {
namespace experimental {

template struct coro<void(), void, any_io_executor>;
template struct coro<int(), void, any_io_executor>;
template struct coro<void(), int, any_io_executor>;
template struct coro<int(int), void, any_io_executor>;
template struct coro<int(), int, any_io_executor>;
template struct coro<int(int), int, any_io_executor>;

template struct coro<void() noexcept, void, any_io_executor>;
template struct coro<int() noexcept, void, any_io_executor>;
template struct coro<void() noexcept, int, any_io_executor>;
template struct coro<int(int) noexcept, void, any_io_executor>;
template struct coro<int() noexcept, int, any_io_executor>;
template struct coro<int(int) noexcept, int, any_io_executor>;

} // namespace experimental
} // namespace asio
} // namespace boost

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

boost::asio::experimental::coro<int> generator_impl(
    boost::asio::any_io_executor, int& last, bool& destroyed)
{
  on_scope_exit x = [&]() noexcept { destroyed = true; };
  (void)x;

  int i = 0;
  while (true)
    co_yield last = ++i;
}

boost::asio::awaitable<void> generator_test()
{
  int val = 0;
  bool destr = false;
  {
    auto gi = generator_impl(
        co_await boost::asio::this_coro::executor, val, destr);

    for (int i = 0; i < 10; i++)
    {
      BOOST_ASIO_CHECK(val == i);
      const auto next = co_await gi.async_resume(boost::asio::use_awaitable);
      BOOST_ASIO_CHECK(next);
      BOOST_ASIO_CHECK(val == *next);
      BOOST_ASIO_CHECK(val == i + 1);
    }

    BOOST_ASIO_CHECK(!destr);
  }
  BOOST_ASIO_CHECK(destr);
};

void run_generator_test()
{
  boost::asio::io_context ctx;
  boost::asio::co_spawn(ctx, generator_test, boost::asio::detached);
  ctx.run();
}

boost::asio::experimental::coro<void, int> task_test(boost::asio::io_context&)
{
  co_return 42;
}

boost::asio::experimental::coro<void, int> task_throw(boost::asio::io_context&)
{
  throw std::logic_error(__func__);
  co_return 42;
}

void run_task_test()
{
  boost::asio::io_context ctx;

  bool tt1 = false;
  bool tt2 = false;
  bool tt3 = false;
  bool tt4 = false;
  auto tt = task_test(ctx);
  tt.async_resume(
      [&](std::exception_ptr pt, int i)
      {
        tt1 = true;
        BOOST_ASIO_CHECK(!pt);
        BOOST_ASIO_CHECK(i == 42);
        tt.async_resume(
            [&](std::exception_ptr pt, int)
            {
              tt2 = true;
              BOOST_ASIO_CHECK(pt);
            });
      });

  auto tt_2 = task_throw(ctx);

  tt_2.async_resume(
      [&](std::exception_ptr pt, int)
      {
        tt3 = true;
        BOOST_ASIO_CHECK(pt);
        tt.async_resume(
            [&](std::exception_ptr pt, int)
            {
              tt4 = true;
              BOOST_ASIO_CHECK(pt);
            });
      });

  ctx.run();

  BOOST_ASIO_CHECK(tt1);
  BOOST_ASIO_CHECK(tt2);
  BOOST_ASIO_CHECK(tt3);
  BOOST_ASIO_CHECK(tt4);
}

boost::asio::experimental::coro<char> completion_generator_test_impl(
    boost::asio::any_io_executor, int limit)
{
  for (int i = 0; i < limit; i++)
    co_yield i;
}

boost::asio::awaitable<void> completion_generator_test()
{
  std::vector<int> res;
  auto g = completion_generator_test_impl(
      co_await boost::asio::this_coro::executor, 10);

  BOOST_ASIO_CHECK(g.is_open());
  while (auto val = co_await g.async_resume(boost::asio::use_awaitable))
    res.push_back(*val);

  BOOST_ASIO_CHECK(!g.is_open());
  BOOST_ASIO_CHECK((res == std::vector{0,1,2,3,4,5,6,7,8,9}));
};


void run_completion_generator_test()
{
  boost::asio::io_context ctx;
  boost::asio::co_spawn(ctx, completion_generator_test, boost::asio::detached);
  ctx.run();
}

boost::asio::experimental::coro<int(int)>
symmetrical_test_impl(boost::asio::any_io_executor)
{
  int i = 0;
  while (true)
    i = (co_yield i) + i;
}

boost::asio::awaitable<void> symmetrical_test()
{
  auto g = symmetrical_test_impl(co_await boost::asio::this_coro::executor);

  BOOST_ASIO_CHECK(g.is_open());

  BOOST_ASIO_CHECK(0  == (co_await g.async_resume(0,
          boost::asio::use_awaitable)).value_or(-1));

  BOOST_ASIO_CHECK(1  == (co_await g.async_resume(1,
          boost::asio::use_awaitable)).value_or(-1));

  BOOST_ASIO_CHECK(3  == (co_await g.async_resume(2,
          boost::asio::use_awaitable)).value_or(-1));

  BOOST_ASIO_CHECK(6  == (co_await g.async_resume(3,
          boost::asio::use_awaitable)).value_or(-1));

  BOOST_ASIO_CHECK(10  == (co_await g.async_resume(4,
          boost::asio::use_awaitable)).value_or(-1));

  BOOST_ASIO_CHECK(15 == (co_await g.async_resume(5,
          boost::asio::use_awaitable)).value_or(-1));

  BOOST_ASIO_CHECK(21 == (co_await g.async_resume(6,
          boost::asio::use_awaitable)).value_or(-1));

  BOOST_ASIO_CHECK(28 == (co_await g.async_resume(7,
          boost::asio::use_awaitable)).value_or(-1));

  BOOST_ASIO_CHECK(36 == (co_await g.async_resume(8,
          boost::asio::use_awaitable)).value_or(-1));

  BOOST_ASIO_CHECK(45 == (co_await g.async_resume(9,
          boost::asio::use_awaitable)).value_or(-1));
};

void run_symmetrical_test()
{
  boost::asio::io_context ctx;
  boost::asio::co_spawn(ctx, symmetrical_test, boost::asio::detached);
  ctx.run();
}

} // namespace coro

BOOST_ASIO_TEST_SUITE
(
  "coro/simple_test",
  BOOST_ASIO_TEST_CASE(::coro::run_generator_test)
  BOOST_ASIO_TEST_CASE(::coro::run_task_test)
  BOOST_ASIO_TEST_CASE(::coro::run_symmetrical_test)
  BOOST_ASIO_TEST_CASE(::coro::run_completion_generator_test)
)
