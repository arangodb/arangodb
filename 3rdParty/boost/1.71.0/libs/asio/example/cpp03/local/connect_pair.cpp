//
// connect_pair.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <string>
#include <cctype>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>

#if defined(BOOST_ASIO_HAS_LOCAL_SOCKETS)

using boost::asio::local::stream_protocol;

class uppercase_filter
{
public:
  uppercase_filter(boost::asio::io_context& io_context)
    : socket_(io_context)
  {
  }

  stream_protocol::socket& socket()
  {
    return socket_;
  }

  void start()
  {
    // Wait for request.
    socket_.async_read_some(boost::asio::buffer(data_),
        boost::bind(&uppercase_filter::handle_read,
          this, boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
  }

private:
  void handle_read(const boost::system::error_code& ec, std::size_t size)
  {
    if (!ec)
    {
      // Compute result.
      for (std::size_t i = 0; i < size; ++i)
        data_[i] = std::toupper(data_[i]);

      // Send result.
      boost::asio::async_write(socket_, boost::asio::buffer(data_, size),
          boost::bind(&uppercase_filter::handle_write,
            this, boost::asio::placeholders::error));
    }
    else
    {
      throw boost::system::system_error(ec);
    }
  }

  void handle_write(const boost::system::error_code& ec)
  {
    if (!ec)
    {
      // Wait for request.
      socket_.async_read_some(boost::asio::buffer(data_),
          boost::bind(&uppercase_filter::handle_read,
            this, boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
    }
    else
    {
      throw boost::system::system_error(ec);
    }
  }

  stream_protocol::socket socket_;
  boost::array<char, 512> data_;
};

void run(boost::asio::io_context* io_context)
{
  try
  {
    io_context->run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception in thread: " << e.what() << "\n";
    std::exit(1);
  }
}

int main()
{
  try
  {
    boost::asio::io_context io_context;

    // Create filter and establish a connection to it.
    uppercase_filter filter(io_context);
    stream_protocol::socket socket(io_context);
    boost::asio::local::connect_pair(socket, filter.socket());
    filter.start();

    // The io_context runs in a background thread to perform filtering.
    boost::thread thread(boost::bind(run, &io_context));

    for (;;)
    {
      // Collect request from user.
      std::cout << "Enter a string: ";
      std::string request;
      std::getline(std::cin, request);

      // Send request to filter.
      boost::asio::write(socket, boost::asio::buffer(request));

      // Wait for reply from filter.
      std::vector<char> reply(request.size());
      boost::asio::read(socket, boost::asio::buffer(reply));

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

#else // defined(BOOST_ASIO_HAS_LOCAL_SOCKETS)
# error Local sockets not available on this platform.
#endif // defined(BOOST_ASIO_HAS_LOCAL_SOCKETS)
