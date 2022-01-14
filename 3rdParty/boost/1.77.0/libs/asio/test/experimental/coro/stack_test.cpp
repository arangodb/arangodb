//
// experimental/coro/stack_test.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <iostream>
#include "../../unit_test.hpp"

using namespace boost::asio::experimental;

namespace coro {

boost::asio::experimental::coro<int()>
  stack_generator(boost::asio::any_io_executor, int i = 1)
{
  for (;;)
  {
    co_yield i;
    i *= 2;
  }
}

boost::asio::experimental::coro<int(int)>
stack_accumulate(boost::asio::any_io_executor exec)
{
  auto gen  = stack_generator(exec);
  int offset = 0;
  while (auto next = co_await gen) // 1, 2, 4, 8, ...
    offset  = co_yield *next + offset; // offset is delayed by one cycle
}

boost::asio::experimental::coro<int>
main_stack_coro(boost::asio::io_context&, bool & done)
{
  auto g = stack_accumulate(co_await boost::asio::this_coro::executor);

  BOOST_ASIO_CHECK(g.is_open());
  BOOST_ASIO_CHECK(1    == (co_await g(1000)).value_or(-1));
  BOOST_ASIO_CHECK(2002 == (co_await g(2000)).value_or(-1));
  BOOST_ASIO_CHECK(3004 == (co_await g(3000)).value_or(-1));
  BOOST_ASIO_CHECK(4008 == (co_await g(4000)).value_or(-1));
  BOOST_ASIO_CHECK(5016 == (co_await g(5000)).value_or(-1));
  BOOST_ASIO_CHECK(6032 == (co_await g(6000)).value_or(-1));
  BOOST_ASIO_CHECK(7064 == (co_await g(7000)).value_or(-1));
  BOOST_ASIO_CHECK(8128 == (co_await g(8000)).value_or(-1));
  BOOST_ASIO_CHECK(9256 == (co_await g(9000)).value_or(-1));
  BOOST_ASIO_CHECK(511 == (co_await g(-1)).value_or(-1));
  done = true;
}

void stack_test()
{
  bool done = false;
  boost::asio::io_context ctx;
  auto k = main_stack_coro(ctx, done);
  k.async_resume(boost::asio::detached);
  ctx.run();
  BOOST_ASIO_CHECK(done);
}

} // namespace coro

BOOST_ASIO_TEST_SUITE
(
  "coro/stack_test",
  BOOST_ASIO_TEST_CASE(::coro::stack_test)
)
