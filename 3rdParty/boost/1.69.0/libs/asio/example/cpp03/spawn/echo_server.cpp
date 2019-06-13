//
// echo_server.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <iostream>

using boost::asio::ip::tcp;

class session : public boost::enable_shared_from_this<session>
{
public:
  explicit session(boost::asio::io_context& io_context)
    : strand_(io_context),
      socket_(io_context),
      timer_(io_context)
  {
  }

  tcp::socket& socket()
  {
    return socket_;
  }

  void go()
  {
    boost::asio::spawn(strand_,
        boost::bind(&session::echo,
          shared_from_this(), _1));
    boost::asio::spawn(strand_,
        boost::bind(&session::timeout,
          shared_from_this(), _1));
  }

private:
  void echo(boost::asio::yield_context yield)
  {
    try
    {
      char data[128];
      for (;;)
      {
        timer_.expires_after(boost::asio::chrono::seconds(10));
        std::size_t n = socket_.async_read_some(boost::asio::buffer(data), yield);
        boost::asio::async_write(socket_, boost::asio::buffer(data, n), yield);
      }
    }
    catch (std::exception& e)
    {
      socket_.close();
      timer_.cancel();
    }
  }

  void timeout(boost::asio::yield_context yield)
  {
    while (socket_.is_open())
    {
      boost::system::error_code ignored_ec;
      timer_.async_wait(yield[ignored_ec]);
      if (timer_.expiry() <= boost::asio::steady_timer::clock_type::now())
        socket_.close();
    }
  }

  boost::asio::io_context::strand strand_;
  tcp::socket socket_;
  boost::asio::steady_timer timer_;
};

void do_accept(boost::asio::io_context& io_context,
    unsigned short port, boost::asio::yield_context yield)
{
  tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));

  for (;;)
  {
    boost::system::error_code ec;
    boost::shared_ptr<session> new_session(new session(io_context));
    acceptor.async_accept(new_session->socket(), yield[ec]);
    if (!ec) new_session->go();
  }
}

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: echo_server <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    boost::asio::spawn(io_context,
        boost::bind(do_accept,
          boost::ref(io_context), atoi(argv[1]), _1));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
