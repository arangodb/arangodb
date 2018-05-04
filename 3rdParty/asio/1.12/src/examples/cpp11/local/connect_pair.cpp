//
// connect_pair.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <array>
#include <iostream>
#include <string>
#include <cctype>
#include <asio.hpp>

#if defined(ASIO_HAS_LOCAL_SOCKETS)

using asio::local::stream_protocol;

class uppercase_filter
{
public:
  uppercase_filter(stream_protocol::socket sock)
    : socket_(std::move(sock))
  {
    read();
  }

private:
  void read()
  {
    socket_.async_read_some(asio::buffer(data_),
        [this](std::error_code ec, std::size_t size)
        {
          if (!ec)
          {
            // Compute result.
            for (std::size_t i = 0; i < size; ++i)
              data_[i] = std::toupper(data_[i]);

            // Send result.
            write(size);
          }
          else
          {
            throw asio::system_error(ec);
          }
        });
  }

  void write(std::size_t size)
  {
    asio::async_write(socket_, asio::buffer(data_, size),
        [this](std::error_code ec, std::size_t /*size*/)
        {
          if (!ec)
          {
            // Wait for request.
            read();
          }
          else
          {
            throw asio::system_error(ec);
          }
        });
  }

  stream_protocol::socket socket_;
  std::array<char, 512> data_;
};

int main()
{
  try
  {
    asio::io_context io_context;

    // Create a connected pair and pass one end to a filter.
    stream_protocol::socket socket(io_context);
    stream_protocol::socket filter_socket(io_context);
    asio::local::connect_pair(socket, filter_socket);
    uppercase_filter filter(std::move(filter_socket));

    // The io_context runs in a background thread to perform filtering.
    asio::thread thread(
        [&io_context]()
        {
          try
          {
            io_context.run();
          }
          catch (std::exception& e)
          {
            std::cerr << "Exception in thread: " << e.what() << "\n";
            std::exit(1);
          }
        });

    for (;;)
    {
      // Collect request from user.
      std::cout << "Enter a string: ";
      std::string request;
      std::getline(std::cin, request);

      // Send request to filter.
      asio::write(socket, asio::buffer(request));

      // Wait for reply from filter.
      std::vector<char> reply(request.size());
      asio::read(socket, asio::buffer(reply));

      // Show reply to user.
      std::cout << "Result: ";
      std::cout.write(&reply[0], request.size());
      std::cout << std::endl;
    }
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
    std::exit(1);
  }
}

#else // defined(ASIO_HAS_LOCAL_SOCKETS)
# error Local sockets not available on this platform.
#endif // defined(ASIO_HAS_LOCAL_SOCKETS)
