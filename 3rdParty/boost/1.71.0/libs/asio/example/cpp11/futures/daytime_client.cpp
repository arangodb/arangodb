//
// daytime_client.cpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <array>
#include <future>
#include <iostream>
#include <thread>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/use_future.hpp>

using boost::asio::ip::udp;

void get_daytime(boost::asio::io_context& io_context, const char* hostname)
{
  try
  {
    udp::resolver resolver(io_context);

    std::future<udp::resolver::results_type> endpoints =
      resolver.async_resolve(
          udp::v4(), hostname, "daytime",
          boost::asio::use_future);

    // The async_resolve operation above returns the endpoints as a future
    // value that is not retrieved ...

    udp::socket socket(io_context, udp::v4());

    std::array<char, 1> send_buf  = {{ 0 }};
    std::future<std::size_t> send_length =
      socket.async_send_to(boost::asio::buffer(send_buf),
          *endpoints.get().begin(), // ... until here. This call may block.
          boost::asio::use_future);

    // Do other things here while the send completes.

    send_length.get(); // Blocks until the send is complete. Throws any errors.

    std::array<char, 128> recv_buf;
    udp::endpoint sender_endpoint;
    std::future<std::size_t> recv_length =
      socket.async_receive_from(
          boost::asio::buffer(recv_buf),
          sender_endpoint,
          boost::asio::use_future);

    // Do other things here while the receive completes.

    std::cout.write(
        recv_buf.data(),
        recv_length.get()); // Blocks until receive is complete.
  }
  catch (std::system_error& e)
  {
    std::cerr << e.what() << std::endl;
  }
}

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: daytime_client <host>" << std::endl;
      return 1;
    }

    // We run the io_context off in its own thread so that it operates
    // completely asynchronously with respect to the rest of the program.
    boost::asio::io_context io_context;
    auto work = boost::asio::make_work_guard(io_context);
    std::thread thread([&io_context](){ io_context.run(); });

    get_daytime(io_context, argv[1]);

    io_context.stop();
    thread.join();
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
