//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <set>
#include "protocol.hpp"

using asio::ip::tcp;
using asio::ip::udp;

typedef boost::shared_ptr<tcp::socket> tcp_socket_ptr;
typedef boost::shared_ptr<asio::deadline_timer> timer_ptr;
typedef boost::shared_ptr<control_request> control_request_ptr;

class server
{
public:
  // Construct the server to wait for incoming control connections.
  server(asio::io_context& io_context, unsigned short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
      timer_(io_context),
      udp_socket_(io_context, udp::endpoint(udp::v4(), 0)),
      next_frame_number_(1)
  {
    // Start waiting for a new control connection.
    tcp_socket_ptr new_socket(
        new tcp::socket(acceptor_.get_executor().context()));
    acceptor_.async_accept(*new_socket,
        boost::bind(&server::handle_accept, this,
          asio::placeholders::error, new_socket));

    // Start the timer used to generate outgoing frames.
    timer_.expires_from_now(boost::posix_time::milliseconds(100));
    timer_.async_wait(boost::bind(&server::handle_timer, this));
  }

  // Handle a new control connection.
  void handle_accept(const asio::error_code& ec, tcp_socket_ptr socket)
  {
    if (!ec)
    {
      // Start receiving control requests on the connection.
      control_request_ptr request(new control_request);
      asio::async_read(*socket, request->to_buffers(),
          boost::bind(&server::handle_control_request, this,
            asio::placeholders::error, socket, request));
    }

    // Start waiting for a new control connection.
    tcp_socket_ptr new_socket(
        new tcp::socket(acceptor_.get_executor().context()));
    acceptor_.async_accept(*new_socket,
        boost::bind(&server::handle_accept, this,
          asio::placeholders::error, new_socket));
  }

  // Handle a new control request.
  void handle_control_request(const asio::error_code& ec,
      tcp_socket_ptr socket, control_request_ptr request)
  {
    if (!ec)
    {
      // Delay handling of the control request to simulate network latency.
      timer_ptr delay_timer(
          new asio::deadline_timer(acceptor_.get_executor().context()));
      delay_timer->expires_from_now(boost::posix_time::seconds(2));
      delay_timer->async_wait(
          boost::bind(&server::handle_control_request_timer, this,
            socket, request, delay_timer));
    }
  }

  void handle_control_request_timer(tcp_socket_ptr socket,
      control_request_ptr request, timer_ptr /*delay_timer*/)
  {
    // Determine what address this client is connected from, since
    // subscriptions must be stored on the server as a complete endpoint, not
    // just a port. We use the non-throwing overload of remote_endpoint() since
    // it may fail if the socket is no longer connected.
    asio::error_code ec;
    tcp::endpoint remote_endpoint = socket->remote_endpoint(ec);
    if (!ec)
    {
      // Remove old port subscription, if any.
      if (unsigned short old_port = request->old_port())
      {
        udp::endpoint old_endpoint(remote_endpoint.address(), old_port);
        subscribers_.erase(old_endpoint);
        std::cout << "Removing subscription " << old_endpoint << std::endl;
      }

      // Add new port subscription, if any.
      if (unsigned short new_port = request->new_port())
      {
        udp::endpoint new_endpoint(remote_endpoint.address(), new_port);
        subscribers_.insert(new_endpoint);
        std::cout << "Adding subscription " << new_endpoint << std::endl;
      }
    }

    // Wait for next control request on this connection.
    asio::async_read(*socket, request->to_buffers(),
        boost::bind(&server::handle_control_request, this,
          asio::placeholders::error, socket, request));
  }

  // Every time the timer fires we will generate a new frame and send it to all
  // subscribers.
  void handle_timer()
  {
    // Generate payload.
    double x = next_frame_number_ * 0.2;
    double y = std::sin(x);
    int char_index = static_cast<int>((y + 1.0) * (frame::payload_size / 2));
    std::string payload;
    for (int i = 0; i < frame::payload_size; ++i)
      payload += (i == char_index ? '*' : '.');

    // Create the frame to be sent to all subscribers.
    frame f(next_frame_number_++, payload);

    // Send frame to all subscribers. We can use synchronous calls here since
    // UDP send operations typically do not block.
    std::set<udp::endpoint>::iterator j;
    for (j = subscribers_.begin(); j != subscribers_.end(); ++j)
    {
      asio::error_code ec;
      udp_socket_.send_to(f.to_buffers(), *j, 0, ec);
    }

    // Wait for next timeout.
    timer_.expires_from_now(boost::posix_time::milliseconds(100));
    timer_.async_wait(boost::bind(&server::handle_timer, this));
  }

private:
  // The acceptor used to accept incoming control connections.
  tcp::acceptor acceptor_;

  // The timer used for generating data.
  asio::deadline_timer timer_;

  // The socket used to send data to subscribers.
  udp::socket udp_socket_;

  // The next frame number.
  unsigned long next_frame_number_;

  // The set of endpoints that are subscribed.
  std::set<udp::endpoint> subscribers_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: server <port>\n";
      return 1;
    }

    asio::io_context io_context;

    using namespace std; // For atoi.
    server s(io_context, atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << std::endl;
  }

  return 0;
}
