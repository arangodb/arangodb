//
// experimental/coro/partial.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

#include <boost/asio/io_context.hpp>
#include "../../unit_test.hpp"

using namespace boost::asio::experimental;

namespace coro {

void partial()
{
  boost::asio::io_context ctx;
  bool ran = false;
  auto p = detail::post_coroutine(ctx, [&]{ran = true;});
  BOOST_ASIO_CHECK(!ran);
  p.resume();
  BOOST_ASIO_CHECK(!ran);
  ctx.run();
  BOOST_ASIO_CHECK(ran);
}

} // namespace coro

BOOST_ASIO_TEST_SUITE
(
  "coro/partial",
  BOOST_ASIO_TEST_CASE(::coro::partial)
)
