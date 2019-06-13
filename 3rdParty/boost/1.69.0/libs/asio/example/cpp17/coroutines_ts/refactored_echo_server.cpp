//
// refactored_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/experimental/co_spawn.hpp>
#include <boost/asio/experimental/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <cstdio>

using boost::asio::ip::tcp;
using boost::asio::experimental::co_spawn;
using boost::asio::experimental::detached;
namespace this_coro = boost::asio::experimental::this_coro;

template <typename T>
  using awaitable = boost::asio::experimental::awaitable<
    T, boost::asio::io_context::executor_type>;

awaitable<void> echo_once(tcp::socket& socket)
{
  auto token = co_await this_coro::token();

  char data[128];
  std::size_t n = co_await socket.async_read_some(boost::asio::buffer(data), token);
  co_await async_write(socket, boost::asio::buffer(data, n), token);
}

awaitable<void> echo(tcp::socket socket)
{
  try
  {
    for (;;)
    {
      // The asynchronous operations to echo a single chunk of data have been
      // refactored into a separate function. When this function is called, the
      // operations are still performed in the context of the current
      // coroutine, and the behaviour is functionally equivalent.
      co_await echo_once(socket);
    }
  }
  catch (std::exception& e)
  {
    std::printf("echo Exception: %s\n", e.what());
  }
}

awaitable<void> listener()
{
  auto executor = co_await this_coro::executor();
  auto token = co_await this_coro::token();

  tcp::acceptor acceptor(executor.context(), {tcp::v4(), 55555});
  for (;;)
  {
    tcp::socket socket = co_await acceptor.async_accept(token);
    co_spawn(executor,
        [socket = std::move(socket)]() mutable
        {
          return echo(std::move(socket));
        },
        detached);
  }
}

int main()
{
  try
  {
    boost::asio::io_context io_context(1);

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto){ io_context.stop(); });

    co_spawn(io_context, listener, detached);

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::printf("Exception: %s\n", e.what());
  }
}
