//
// blocking_tcp_client.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/system/system_error.hpp>
#include <boost/asio/write.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

using boost::asio::ip::tcp;

//----------------------------------------------------------------------

//
// This class manages socket timeouts by running the io_context using the timed
// io_context::run_for() member function. Each asynchronous operation is given
// a timeout within which it must complete. The socket operations themselves
// use lambdas as completion handlers. For a given socket operation, the client
// object runs the io_context to block thread execution until the operation
// completes or the timeout is reached. If the io_context::run_for() function
// times out, the socket is closed and the outstanding asynchronous operation
// is cancelled.
//
class client
{
public:
  void connect(const std::string& host, const std::string& service,
      std::chrono::steady_clock::duration timeout)
  {
    // Resolve the host name and service to a list of endpoints.
    auto endpoints = tcp::resolver(io_context_).resolve(host, service);

    // Start the asynchronous operation itself. The lambda that is used as a
    // callback will update the error variable when the operation completes.
    // The blocking_udp_client.cpp example shows how you can use std::bind
    // rather than a lambda.
    boost::system::error_code error;
    boost::asio::async_connect(socket_, endpoints,
        [&](const boost::system::error_code& result_error,
            const tcp::endpoint& /*result_endpoint*/)
        {
          error = result_error;
        });

    // Run the operation until it completes, or until the timeout.
    run(timeout);

    // Determine whether a connection was successfully established.
    if (error)
      throw std::system_error(error);
  }

  std::string read_line(std::chrono::steady_clock::duration timeout)
  {
    // Start the asynchronous operation. The lambda that is used as a callback
    // will update the error and n variables when the operation completes. The
    // blocking_udp_client.cpp example shows how you can use std::bind rather
    // than a lambda.
    boost::system::error_code error;
    std::size_t n = 0;
    boost::asio::async_read_until(socket_,
        boost::asio::dynamic_buffer(input_buffer_), '\n',
        [&](const boost::system::error_code& result_error,
            std::size_t result_n)
        {
          error = result_error;
          n = result_n;
        });

    // Run the operation until it completes, or until the timeout.
    run(timeout);

    // Determine whether the read completed successfully.
    if (error)
      throw std::system_error(error);

    std::string line(input_buffer_.substr(0, n - 1));
    input_buffer_.erase(0, n);
    return line;
  }

  void write_line(const std::string& line,
      std::chrono::steady_clock::duration timeout)
  {
    std::string data = line + "\n";

    // Start the asynchronous operation itself. The lambda that is used as a
    // callback will update the error variable when the operation completes.
    // The blocking_udp_client.cpp example shows how you can use std::bind
    // rather than a lambda.
    boost::system::error_code error;
    boost::asio::async_write(socket_, boost::asio::buffer(data),
        [&](const boost::system::error_code& result_error,
            std::size_t /*result_n*/)
        {
          error = result_error;
        });

    // Run the operation until it completes, or until the timeout.
    run(timeout);

    // Determine whether the read completed successfully.
    if (error)
      throw std::system_error(error);
  }

private:
  void run(std::chrono::steady_clock::duration timeout)
  {
    // Restart the io_context, as it may have been left in the "stopped" state
    // by a previous operation.
    io_context_.restart();

    // Block until the asynchronous operation has completed, or timed out. If
    // the pending asynchronous operation is a composed operation, the deadline
    // applies to the entire operation, rather than individual operations on
    // the socket.
    io_context_.run_for(timeout);

    // If the asynchronous operation completed successfully then the io_context
    // would have been stopped due to running out of work. If it was not
    // stopped, then the io_context::run_for call must have timed out.
    if (!io_context_.stopped())
    {
      // Close the socket to cancel the outstanding asynchronous operation.
      socket_.close();

      // Run the io_context again until the operation completes.
      io_context_.run();
    }
  }

  boost::asio::io_context io_context_;
  tcp::socket socket_{io_context_};
  std::string input_buffer_;
};

//----------------------------------------------------------------------

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 4)
    {
      std::cerr << "Usage: blocking_tcp_client <host> <port> <message>\n";
      return 1;
    }

    client c;
    c.connect(argv[1], argv[2], std::chrono::seconds(10));

    auto time_sent = std::chrono::steady_clock::now();

    c.write_line(argv[3], std::chrono::seconds(10));

    for (;;)
    {
      std::string line = c.read_line(std::chrono::seconds(10));

      // Keep going until we get back the line that was sent.
      if (line == argv[3])
        break;
    }

    auto time_received = std::chrono::steady_clock::now();

    std::cout << "Round trip time: ";
    std::cout << std::chrono::duration_cast<
      std::chrono::microseconds>(
        time_received - time_sent).count();
    std::cout << " microseconds\n";
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
