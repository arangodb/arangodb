//
// stream_server.cpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <array>
#include <cstdio>
#include <iostream>
#include <memory>
#include "asio.hpp"

#if defined(ASIO_HAS_LOCAL_SOCKETS)

using asio::local::stream_protocol;

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(stream_protocol::socket sock)
    : socket_(std::move(sock))
  {
  }

  void start()
  {
    do_read();
  }

private:
  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(asio::buffer(data_),
        [this, self](std::error_code ec, std::size_t length)
        {
          if (!ec)
            do_write(length);
        });
  }

  void do_write(std::size_t length)
  {
    auto self(shared_from_this());
    asio::async_write(socket_,
        asio::buffer(data_, length),
        [this, self](std::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
            do_read();
        });
  }

  // The socket used to communicate with the client.
  stream_protocol::socket socket_;

  // Buffer used to store data received from the client.
  std::array<char, 1024> data_;
};

class server
{
public:
  server(asio::io_context& io_context, const std::string& file)
    : acceptor_(io_context, stream_protocol::endpoint(file))
  {
    do_accept();
  }

private:
  void do_accept()
  {
    acceptor_.async_accept(
        [this](std::error_code ec, stream_protocol::socket socket)
        {
          if (!ec)
          {
            std::make_shared<session>(std::move(socket))->start();
          }

          do_accept();
        });
  }

  stream_protocol::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: stream_server <file>\n";
      std::cerr << "*** WARNING: existing file is removed ***\n";
      return 1;
    }

    asio::io_context io_context;

    std::remove(argv[1]);
    server s(io_context, argv[1]);

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}

#else // defined(ASIO_HAS_LOCAL_SOCKETS)
# error Local sockets not available on this platform.
#endif // defined(ASIO_HAS_LOCAL_SOCKETS)
