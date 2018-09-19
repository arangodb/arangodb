//
// double_buffered_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

awaitable<void> echo(tcp::socket s)
{
 auto token = co_await this_coro::token();

 try
 {
   char data1[1024];
   char data2[1024];

   char* p1 = data1;
   char* p2 = data2;

   // Perform initial read into first buffer.
   size_t n = co_await s.async_read_some(boost::asio::buffer(p1, 1024), token);

   for (;;)
   {
     // Swap received data to other buffer and initiate write operation.
     std::swap(p1, p2);
     auto write_result = boost::asio::async_write(s, boost::asio::buffer(p2, n), token);

     // Perform next read while write operation is in progress.
     n = co_await s.async_read_some(boost::asio::buffer(p1, 1024), token);

     // Wait for write operation to complete before proceeding.
     co_await write_result;
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
