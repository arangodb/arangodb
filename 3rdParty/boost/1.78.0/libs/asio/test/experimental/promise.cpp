//
// promise.cpp
// ~~~~~~~~~~~
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
#include <boost/asio/experimental/promise.hpp>

#include <boost/asio/steady_timer.hpp>
#include "../unit_test.hpp"

namespace promise {

void promise_tester()
{
  using namespace boost::asio;
  using boost::system::error_code;
  using namespace std::chrono;

  io_context ctx;

  steady_timer timer1{ctx}, timer2{ctx};

  const auto started_when = steady_clock::now();
  timer1.expires_at(started_when + milliseconds(2000));
  timer2.expires_at(started_when + milliseconds(1000));
  auto p = timer1.async_wait(experimental::use_promise);

  steady_clock::time_point completed_when;
  error_code ec;
  bool called = false;

  p.async_wait(
      [&](auto ec_)
      {
        ec = ec_;
        called = true;
        completed_when = steady_clock::now();
      });

  steady_clock::time_point timer2_done;
  timer2.async_wait([&](auto) {
    timer2_done = steady_clock::now();;
    p.cancel();
  });

  ctx.run();

  BOOST_ASIO_CHECK(timer2_done + milliseconds(1) > completed_when);
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == error::operation_aborted);
}

void promise_race_tester()
{
  using namespace boost::asio;
  using boost::system::error_code;
  using namespace std::chrono;

  io_context ctx;

  steady_timer timer1{ctx}, timer2{ctx};

  const auto started_when = steady_clock::now();
  timer1.expires_at(started_when + milliseconds(2000));
  timer2.expires_at(started_when + milliseconds(1000));

  experimental::promise<void(std::variant<error_code, error_code>)> p =
    experimental::promise<>::race(
        timer1.async_wait(experimental::use_promise),
        timer2.async_wait(experimental::use_promise));

  auto called = false;
  error_code ec;
  steady_clock::time_point completed_when;
  p.async_wait(
      [&](auto v)
      {
        BOOST_ASIO_CHECK(v.index() == 1);
        ec = get<1>(v);
        called = true;
        completed_when = steady_clock::now();
      });

  ctx.run();

  BOOST_ASIO_CHECK(started_when + milliseconds(1000) <= completed_when);
  BOOST_ASIO_CHECK(started_when + milliseconds(1500) > completed_when);
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
}

void promise_all_tester()
{
  using namespace boost::asio;
  using boost::system::error_code;
  using namespace std::chrono;

  io_context ctx;

  steady_timer timer1{ctx},
         timer2{ctx};

  const auto started_when = steady_clock::now();
  timer1.expires_at(started_when + milliseconds(2000));
  timer2.expires_at(started_when + milliseconds(1000));

  experimental::promise<void(error_code, error_code)> p =
    experimental::promise<>::all(
        timer1.async_wait(experimental::use_promise),
        timer2.async_wait(experimental::use_promise));

  bool called = false;
  steady_clock::time_point completed_when;

  p.async_wait(
      [&](auto ec1, auto ec2)
      {
        BOOST_ASIO_CHECK(!ec1);
        BOOST_ASIO_CHECK(!ec2);
        called = true;
        completed_when = steady_clock::now();
      });

  ctx.run();

  BOOST_ASIO_CHECK(started_when + milliseconds(2000) <= completed_when);
  BOOST_ASIO_CHECK(started_when + milliseconds(2500) > completed_when);
  BOOST_ASIO_CHECK(called);
}

void promise_race_ranged_tester()
{
  using namespace boost::asio;
  using boost::system::error_code;
  using namespace std::chrono;

  io_context ctx;

  steady_timer timer1{ctx}, timer2{ctx};

  const auto started_when = steady_clock::now();
  timer1.expires_at(started_when + milliseconds(2000));
  timer2.expires_at(started_when + milliseconds(1000));

  // promise<
  //   std::variant<
  //     tuple<error_code, std::size_t>,
  //     tuple<error_code, std::size_t>>>
  experimental::promise<void(std::size_t, error_code)> p =
    experimental::promise<>::race(
        std::array{
          timer1.async_wait(experimental::use_promise),
          timer2.async_wait(experimental::use_promise)
        });

  auto called = false;
  auto completed_when = steady_clock::time_point();

  p.async_wait([&](auto idx, auto ec )
      {
        BOOST_ASIO_CHECK(idx == 1);
        called = true;
        completed_when = steady_clock::now();
        BOOST_ASIO_CHECK(!ec);
      });

  std::array<experimental::promise<void()>, 0u> arr;

  experimental::promise<>::race(
      ctx.get_executor(), std::move(arr)
    ).async_wait(
      [](std::size_t idx) {BOOST_ASIO_CHECK(idx == std::size_t(-1));}
    );

  ctx.run();

  BOOST_ASIO_CHECK(started_when + milliseconds(1000) <= completed_when);
  BOOST_ASIO_CHECK(started_when + milliseconds(1500) > completed_when);
  BOOST_ASIO_CHECK(called);

  std::exception_ptr ex;

  try
  {
    experimental::promise<>::race(std::move(arr));
  }
  catch (...)
  {
    ex = std::current_exception();
  }

  BOOST_ASIO_CHECK(ex);
}

void promise_all_ranged_tester()
{
  using namespace boost::asio;
  using boost::system::error_code;
  using namespace std::chrono;

  io_context ctx;

  steady_timer timer1{ctx}, timer2{ctx};

  const auto started_when = steady_clock::now();
  timer1.expires_at(started_when + milliseconds(2000));
  timer2.expires_at(started_when + milliseconds(1000));

  // promise<
  //   std::variant<
  //     tuple<error_code, std::size_t>,
  //     tuple<error_code, std::size_t>>>
  experimental::promise<void(std::vector<error_code>)> p =
    experimental::promise<>::all(
      std::array{
        timer1.async_wait(experimental::use_promise),
        timer2.async_wait(experimental::use_promise)
      });

  auto called = false;
  auto completed_when = steady_clock::time_point();

  p.async_wait(
      [&](auto v){
        BOOST_ASIO_CHECK(v.size() == 2u);
        completed_when = steady_clock::now();
        BOOST_ASIO_CHECK(!v[0]);
        BOOST_ASIO_CHECK(!v[1]);
        called = true;
      });

  std::array<experimental::promise<void()>, 0u> arr;
  experimental::promise<>::all(
      ctx.get_executor(), std::move(arr)
    ).async_wait(
      [](auto v) {BOOST_ASIO_CHECK(v.size() == 0);}
    );

  ctx.run();

  BOOST_ASIO_CHECK(started_when + milliseconds(2000) <= completed_when);
  BOOST_ASIO_CHECK(started_when + milliseconds(2500) > completed_when);
  BOOST_ASIO_CHECK(called == true);

  std::exception_ptr ex;
  try
  {
    experimental::promise<>::all(std::move(arr));
  }
  catch (...)
  {
    ex = std::current_exception();
  }

  BOOST_ASIO_CHECK(ex);
}

void promise_cancel_tester()
{
  using namespace boost::asio;
  using boost::system::error_code;
  using namespace std::chrono;

  io_context ctx;

  steady_timer timer1{ctx}, timer2{ctx};

  const auto started_when = steady_clock::now();
  timer1.expires_at(started_when + milliseconds(2000));
  timer2.expires_at(started_when + milliseconds(1000));

  // promise<
  //   std::variant<
  //     tuple<error_code, std::size_t>,
  //     tuple<error_code, std::size_t>>>
  experimental::promise<void(error_code, error_code)> p =
      experimental::promise<>::all(
          timer1.async_wait(experimental::use_promise),
          timer2.async_wait(experimental::use_promise));

  bool called = false;
  p.async_wait(
      [&](auto ec1, auto ec2)
      {
        called = true;
        BOOST_ASIO_CHECK(ec1 == error::operation_aborted);
        BOOST_ASIO_CHECK(ec2 == error::operation_aborted);
      });

  post(ctx, [&]{p.cancel();});

  ctx.run();

  BOOST_ASIO_CHECK(called);
}

} // namespace promise

BOOST_ASIO_TEST_SUITE
(
    "promise",
    BOOST_ASIO_TEST_CASE(promise::promise_tester)
    BOOST_ASIO_TEST_CASE(promise::promise_race_tester)
    BOOST_ASIO_TEST_CASE(promise::promise_all_tester)
    BOOST_ASIO_TEST_CASE(promise::promise_race_ranged_tester)
    BOOST_ASIO_TEST_CASE(promise::promise_all_ranged_tester)
    BOOST_ASIO_TEST_CASE(promise::promise_cancel_tester)
)
