//
// experimental/awaitable_operators.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

// Prevent link dependency on the Boost.System library.
#if !defined(BOOST_SYSTEM_NO_DEPRECATED)
#define BOOST_SYSTEM_NO_DEPRECATED
#endif // !defined(BOOST_SYSTEM_NO_DEPRECATED)

// Test that header file is self-contained.
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include "../unit_test.hpp"

using boost::asio::awaitable;
using namespace boost::asio::experimental::awaitable_operators;

struct move_only
{
  move_only(int i) : value(i) {}
  move_only(const move_only&) = delete;
  move_only(move_only&&) {}
  move_only& operator=(const move_only&) = delete;
  move_only& operator=(move_only&&) { return *this; }
  int value;
};

awaitable<void> void_ok()
{
  co_return;
}

awaitable<void> void_ex()
{
  throw std::runtime_error("exception");
  co_return;
}

awaitable<void> void_post_loop()
{
  for (;;)
    co_await boost::asio::post(boost::asio::use_awaitable);
}

awaitable<int> int_0()
{
  co_return 0;
}

awaitable<int> int_1()
{
  co_return 1;
}

awaitable<int> int_2()
{
  co_return 2;
}

awaitable<int> int_ex()
{
  throw std::runtime_error("exception");
  co_return -1;
}

awaitable<move_only> move_only_0()
{
  co_return move_only(0);
}

awaitable<move_only> move_only_1()
{
  co_return move_only(1);
}

awaitable<move_only> move_only_2()
{
  co_return move_only(2);
}

awaitable<move_only> move_only_ex()
{
  throw std::runtime_error("exception");
  co_return move_only(-1);
}

awaitable<void> do_test_and_awaitable_operator()
{
  int i;
  std::tuple<int, int> ii;
  std::tuple<int, int, int> iii;

  co_await (void_ok() && void_ok());
  co_await (void_ok() && void_ok() && void_ok());
  try { co_await (void_ex() && void_ok()); } catch (...) {}
  try { co_await (void_ex() && void_ok() && void_ok()); } catch (...) {}
  try { co_await (void_ex() && void_post_loop()); } catch (...) {}
  try { co_await (void_ex() && void_ok() && void_post_loop()); } catch (...) {}

  i = co_await (void_ok() && int_0());
  BOOST_ASIO_CHECK(i == 0);

  i = co_await (void_ok() && int_1());
  BOOST_ASIO_CHECK(i == 1);

  i = co_await (int_0() && void_ok());
  BOOST_ASIO_CHECK(i == 0);

  ii = co_await (int_0() && int_1());
  BOOST_ASIO_CHECK(std::get<0>(ii) == 0);
  BOOST_ASIO_CHECK(std::get<1>(ii) == 1);

  i = co_await (void_ok() && void_ok() && int_0());
  BOOST_ASIO_CHECK(i == 0);

  i = co_await (void_ok() && int_0() && void_ok());
  BOOST_ASIO_CHECK(i == 0);

  ii = co_await (void_ok() && int_0() && int_1());
  BOOST_ASIO_CHECK(std::get<0>(ii) == 0);
  BOOST_ASIO_CHECK(std::get<1>(ii) == 1);

  ii = co_await (int_0() && void_ok() && int_1());
  BOOST_ASIO_CHECK(std::get<0>(ii) == 0);
  BOOST_ASIO_CHECK(std::get<1>(ii) == 1);

  iii = co_await (int_0() && int_1() && int_2());
  BOOST_ASIO_CHECK(std::get<0>(iii) == 0);
  BOOST_ASIO_CHECK(std::get<1>(iii) == 1);
  BOOST_ASIO_CHECK(std::get<2>(iii) == 2);
}

void test_and_awaitable_operator()
{
  boost::asio::io_context ctx;
  co_spawn(ctx, do_test_and_awaitable_operator(), boost::asio::detached);
  ctx.run();
}

awaitable<void> do_test_or_awaitable_operator()
{
  std::variant<int, int> ii;
  std::variant<int, std::monostate> iv;
  std::variant<std::monostate, int> vi;
  std::variant<std::monostate, std::monostate> vv;
  std::variant<int, int, int> iii;
  std::variant<int, int, std::monostate> iiv;
  std::variant<int, std::monostate, int> ivi;
  std::variant<int, std::monostate, std::monostate> ivv;
  std::variant<std::monostate, int, int> vii;
  std::variant<std::monostate, int, std::monostate> viv;
  std::variant<std::monostate, std::monostate, int> vvi;
  std::variant<std::monostate, std::monostate, std::monostate> vvv;

  vv = co_await (void_ok() || void_ok());
  BOOST_ASIO_CHECK(vv.index() == 0);

  vv = co_await (void_ex() || void_ok());
  BOOST_ASIO_CHECK(vv.index() == 1);

  vi = co_await (void_ok() || int_0());
  BOOST_ASIO_CHECK(vi.index() == 0);

  vi = co_await (void_ex() || int_0());
  BOOST_ASIO_CHECK(vi.index() == 1);

  iv = co_await (int_0() || void_ex());
  BOOST_ASIO_CHECK(iv.index() == 0);
  BOOST_ASIO_CHECK(std::get<0>(iv) == 0);

  ii = co_await (int_0() || int_1());
  BOOST_ASIO_CHECK(ii.index() == 0);
  BOOST_ASIO_CHECK(std::get<0>(ii) == 0);

  ii = co_await (int_ex() || int_1());
  BOOST_ASIO_CHECK(ii.index() == 1);
  BOOST_ASIO_CHECK(std::get<1>(ii) == 1);

  vvv = co_await (void_ok() || void_ok() || void_ok());
  BOOST_ASIO_CHECK(vvv.index() == 0);

  vvv = co_await (void_ex() || void_ok() || void_ok());
  BOOST_ASIO_CHECK(vvv.index() == 1);

  vvv = co_await (void_ex() || void_ex() || void_ok());
  BOOST_ASIO_CHECK(vvv.index() == 2);

  vvi = co_await (void_ok() || void_ok() || int_0());
  BOOST_ASIO_CHECK(vvi.index() == 0);

  viv = co_await (void_ok() || int_0() || void_ok());
  BOOST_ASIO_CHECK(viv.index() == 0);

  viv = co_await (void_ex() || int_0() || void_ok());
  BOOST_ASIO_CHECK(viv.index() == 1);
  BOOST_ASIO_CHECK(std::get<1>(viv) == 0);

  vii = co_await (void_ex() || int_0() || int_1());
  BOOST_ASIO_CHECK(vii.index() == 1);
  BOOST_ASIO_CHECK(std::get<1>(vii) == 0);

  ivv = co_await (int_0() || void_ok() || void_ok());
  BOOST_ASIO_CHECK(ivv.index() == 0);
  BOOST_ASIO_CHECK(std::get<0>(ivv) == 0);

  ivv = co_await (int_ex() || void_ok() || void_ok());
  BOOST_ASIO_CHECK(ivv.index() == 1);

  ivi = co_await (int_0() || void_ok() || int_1());
  BOOST_ASIO_CHECK(ivi.index() == 0);
  BOOST_ASIO_CHECK(std::get<0>(ivi) == 0);

  ivi = co_await (int_ex() || void_ok() || int_1());
  BOOST_ASIO_CHECK(ivi.index() == 1);

  iiv = co_await (int_0() || int_1() || void_ok());
  BOOST_ASIO_CHECK(iiv.index() == 0);
  BOOST_ASIO_CHECK(std::get<0>(iiv) == 0);

  iiv = co_await (int_ex() || int_1() || void_ok());
  BOOST_ASIO_CHECK(iiv.index() == 1);
  BOOST_ASIO_CHECK(std::get<0>(iiv) == 1);

  iiv = co_await (int_ex() || int_ex() || void_ok());
  BOOST_ASIO_CHECK(iiv.index() == 2);

  iii = co_await (int_0() || int_1() || int_2());
  BOOST_ASIO_CHECK(iii.index() == 0);
  BOOST_ASIO_CHECK(std::get<0>(iii) == 0);

  iii = co_await (int_ex() || int_1() || int_2());
  BOOST_ASIO_CHECK(iii.index() == 1);
  BOOST_ASIO_CHECK(std::get<1>(iii) == 1);

  iii = co_await (int_ex() || int_ex() || int_2());
  BOOST_ASIO_CHECK(iii.index() == 2);
  BOOST_ASIO_CHECK(std::get<2>(iii) == 2);

  std::variant<move_only, int> mi = co_await (move_only_0() || int_1());
  BOOST_ASIO_CHECK(mi.index() == 0);
  BOOST_ASIO_CHECK(std::get<0>(mi).value == 0);

  mi = co_await (move_only_ex() || int_1());
  BOOST_ASIO_CHECK(mi.index() == 1);
  BOOST_ASIO_CHECK(std::get<1>(mi) == 1);

  std::variant<move_only, move_only> mm =
    co_await (move_only_0() || move_only_1());
  BOOST_ASIO_CHECK(mm.index() == 0);
  BOOST_ASIO_CHECK(std::get<0>(mm).value == 0);

  mm = co_await (move_only_ex() || move_only_1());
  BOOST_ASIO_CHECK(mm.index() == 1);
  BOOST_ASIO_CHECK(std::get<1>(mm).value == 1);

  std::variant<move_only, move_only, move_only> mmm =
    co_await (move_only_0() || move_only_1() || move_only_2());
  BOOST_ASIO_CHECK(mmm.index() == 0);
  BOOST_ASIO_CHECK(std::get<0>(mmm).value == 0);

  mmm = co_await (move_only_ex() || move_only_1() || move_only_2());
  BOOST_ASIO_CHECK(mmm.index() == 1);
  BOOST_ASIO_CHECK(std::get<1>(mmm).value == 1);

  mmm = co_await (move_only_ex() || move_only_ex() || move_only_2());
  BOOST_ASIO_CHECK(mmm.index() == 2);
  BOOST_ASIO_CHECK(std::get<2>(mmm).value == 2);
}

void test_or_awaitable_operator()
{
  boost::asio::io_context ctx;
  co_spawn(ctx, do_test_or_awaitable_operator(), boost::asio::detached);
  ctx.run();
}

BOOST_ASIO_TEST_SUITE
(
  "experimental/awaitable_operators",
  BOOST_ASIO_TEST_CASE(test_and_awaitable_operator)
  BOOST_ASIO_TEST_CASE(test_or_awaitable_operator)
)
