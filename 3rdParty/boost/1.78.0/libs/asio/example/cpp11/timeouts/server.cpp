//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>

using boost::asio::steady_timer;
using boost::asio::ip::tcp;
using boost::asio::ip::udp;

//----------------------------------------------------------------------

class subscriber
{
public:
  virtual ~subscriber() = default;
  virtual void deliver(const std::string& msg) = 0;
};

typedef std::shared_ptr<subscriber> subscriber_ptr;

//----------------------------------------------------------------------

class channel
{
public:
  void join(subscriber_ptr subscriber)
  {
    subscribers_.insert(subscriber);
  }

  void leave(subscriber_ptr subscriber)
  {
    subscribers_.erase(subscriber);
  }

  void deliver(const std::string& msg)
  {
    for (const auto& s : subscribers_)
    {
      s->deliver(msg);
    }
  }

private:
  std::set<subscriber_ptr> subscribers_;
};

//----------------------------------------------------------------------

//
// This class manages socket timeouts by applying the concept of a deadline.
// Some asynchronous operations are given deadlines by which they must complete.
// Deadlines are enforced by two "actors" that persist for the lifetime of the
// session object, one for input and one for output:
//
//  +----------------+                      +----------------+
//  |                |                      |                |
//  | check_deadline |<-------+             | check_deadline |<-------+
//  |                |        |             |                |        |
//  +----------------+        |             +----------------+        |
//               |            |                          |            |
//  async_wait() |    +----------------+    async_wait() |    +----------------+
//   on input    |    |     lambda     |     on output   |    |     lambda     |
//   deadline    +--->|       in       |     deadline    +--->|       in       |
//                    | check_deadline |                      | check_deadline |
//                    +----------------+                      +----------------+
//
// If either deadline actor determines that the corresponding deadline has
// expired, the socket is closed and any outstanding operations are cancelled.
//
// The input actor reads messages from the socket, where messages are delimited
// by the newline character:
//
//  +-------------+
//  |             |
//  |  read_line  |<----+
//  |             |     |
//  +-------------+     |
//          |           |
//  async_- |    +-------------+
//   read_- |    |   lambda    |
//  until() +--->|     in      |
//               |  read_line  |
//               +-------------+
//
// The deadline for receiving a complete message is 30 seconds. If a non-empty
// message is received, it is delivered to all subscribers. If a heartbeat (a
// message that consists of a single newline character) is received, a heartbeat
// is enqueued for the client, provided there are no other messages waiting to
// be sent.
//
// The output actor is responsible for sending messages to the client:
//
//  +----------------+
//  |                |<---------------------+
//  |  await_output  |                      |
//  |                |<-------+             |
//  +----------------+        |             |
//    |            |          |             |
//    |    async_- |  +----------------+    |
//    |     wait() |  |     lambda     |    |
//    |            +->|       in       |    |
//    |               |  await_output  |    |
//    |               +----------------+    |
//    V                                     |
//  +--------------+               +--------------+
//  |              | async_write() |    lambda    |
//  |  write_line  |-------------->|      in      |
//  |              |               |  write_line  |
//  +--------------+               +--------------+
//
// The output actor first waits for an output message to be enqueued. It does
// this by using a steady_timer as an asynchronous condition variable. The
// steady_timer will be signalled whenever the output queue is non-empty.
//
// Once a message is available, it is sent to the client. The deadline for
// sending a complete message is 30 seconds. After the message is successfully
// sent, the output actor again waits for the output queue to become non-empty.
//
class tcp_session
  : public subscriber,
    public std::enable_shared_from_this<tcp_session>
{
public:
  tcp_session(tcp::socket socket, channel& ch)
    : channel_(ch),
      socket_(std::move(socket))
  {
    input_deadline_.expires_at(steady_timer::time_point::max());
    output_deadline_.expires_at(steady_timer::time_point::max());

    // The non_empty_output_queue_ steady_timer is set to the maximum time
    // point whenever the output queue is empty. This ensures that the output
    // actor stays asleep until a message is put into the queue.
    non_empty_output_queue_.expires_at(steady_timer::time_point::max());
  }

  // Called by the server object to initiate the four actors.
  void start()
  {
    channel_.join(shared_from_this());

    read_line();
    check_deadline(input_deadline_);

    await_output();
    check_deadline(output_deadline_);
  }

private:
  void stop()
  {
    channel_.leave(shared_from_this());

    boost::system::error_code ignored_error;
    socket_.close(ignored_error);
    input_deadline_.cancel();
    non_empty_output_queue_.cancel();
    output_deadline_.cancel();
  }

  bool stopped() const
  {
    return !socket_.is_open();
  }

  void deliver(const std::string& msg) override
  {
    output_queue_.push_back(msg + "\n");

    // Signal that the output queue contains messages. Modifying the expiry
    // will wake the output actor, if it is waiting on the timer.
    non_empty_output_queue_.expires_at(steady_timer::time_point::min());
  }

  void read_line()
  {
    // Set a deadline for the read operation.
    input_deadline_.expires_after(std::chrono::seconds(30));

    // Start an asynchronous operation to read a newline-delimited message.
    auto self(shared_from_this());
    boost::asio::async_read_until(socket_,
        boost::asio::dynamic_buffer(input_buffer_), '\n',
        [this, self](const boost::system::error_code& error, std::size_t n)
        {
          // Check if the session was stopped while the operation was pending.
          if (stopped())
            return;

          if (!error)
          {
            // Extract the newline-delimited message from the buffer.
            std::string msg(input_buffer_.substr(0, n - 1));
            input_buffer_.erase(0, n);

            if (!msg.empty())
            {
              channel_.deliver(msg);
            }
            else
            {

              // We received a heartbeat message from the client. If there's
              // nothing else being sent or ready to be sent, send a heartbeat
              // right back.
              if (output_queue_.empty())
              {
                output_queue_.push_back("\n");

                // Signal that the output queue contains messages. Modifying
                // the expiry will wake the output actor, if it is waiting on
                // the timer.
                non_empty_output_queue_.expires_at(
                    steady_timer::time_point::min());
              }
            }

            read_line();
          }
          else
          {
            stop();
          }
        });
  }

  void await_output()
  {
    auto self(shared_from_this());
    non_empty_output_queue_.async_wait(
        [this, self](const boost::system::error_code& /*error*/)
        {
          // Check if the session was stopped while the operation was pending.
          if (stopped())
            return;

          if (output_queue_.empty())
          {
            // There are no messages that are ready to be sent. The actor goes
            // to sleep by waiting on the non_empty_output_queue_ timer. When a
            // new message is added, the timer will be modified and the actor
            // will wake.
            non_empty_output_queue_.expires_at(steady_timer::time_point::max());
            await_output();
          }
          else
          {
            write_line();
          }
        });
  }

  void write_line()
  {
    // Set a deadline for the write operation.
    output_deadline_.expires_after(std::chrono::seconds(30));

    // Start an asynchronous operation to send a message.
    auto self(shared_from_this());
    boost::asio::async_write(socket_,
        boost::asio::buffer(output_queue_.front()),
        [this, self](const boost::system::error_code& error, std::size_t /*n*/)
        {
          // Check if the session was stopped while the operation was pending.
          if (stopped())
            return;

          if (!error)
          {
            output_queue_.pop_front();

            await_output();
          }
          else
          {
            stop();
          }
        });
  }

  void check_deadline(steady_timer& deadline)
  {
    auto self(shared_from_this());
    deadline.async_wait(
        [this, self, &deadline](const boost::system::error_code& /*error*/)
        {
          // Check if the session was stopped while the operation was pending.
          if (stopped())
            return;

          // Check whether the deadline has passed. We compare the deadline
          // against the current time since a new asynchronous operation may
          // have moved the deadline before this actor had a chance to run.
          if (deadline.expiry() <= steady_timer::clock_type::now())
          {
            // The deadline has passed. Stop the session. The other actors will
            // terminate as soon as possible.
            stop();
          }
          else
          {
            // Put the actor back to sleep.
            check_deadline(deadline);
          }
        });
  }

  channel& channel_;
  tcp::socket socket_;
  std::string input_buffer_;
  steady_timer input_deadline_{socket_.get_executor()};
  std::deque<std::string> output_queue_;
  steady_timer non_empty_output_queue_{socket_.get_executor()};
  steady_timer output_deadline_{socket_.get_executor()};
};

typedef std::shared_ptr<tcp_session> tcp_session_ptr;

//----------------------------------------------------------------------

class udp_broadcaster
  : public subscriber
{
public:
  udp_broadcaster(boost::asio::io_context& io_context,
      const udp::endpoint& broadcast_endpoint)
    : socket_(io_context)
  {
    socket_.connect(broadcast_endpoint);
    socket_.set_option(udp::socket::broadcast(true));
  }

private:
  void deliver(const std::string& msg)
  {
    boost::system::error_code ignored_error;
    socket_.send(boost::asio::buffer(msg), 0, ignored_error);
  }

  udp::socket socket_;
};

//----------------------------------------------------------------------

class server
{
public:
  server(boost::asio::io_context& io_context,
      const tcp::endpoint& listen_endpoint,
      const udp::endpoint& broadcast_endpoint)
    : io_context_(io_context),
      acceptor_(io_context, listen_endpoint)
  {
    channel_.join(
        std::make_shared<udp_broadcaster>(
          io_context_, broadcast_endpoint));

    accept();
  }

private:
  void accept()
  {
    acceptor_.async_accept(
        [this](const boost::system::error_code& error, tcp::socket socket)
        {
          if (!error)
          {
            std::make_shared<tcp_session>(std::move(socket), channel_)->start();
          }

          accept();
        });
  }

  boost::asio::io_context& io_context_;
  tcp::acceptor acceptor_;
  channel channel_;
};

//----------------------------------------------------------------------

int main(int argc, char* argv[])
{
  try
  {
    using namespace std; // For atoi.

    if (argc != 4)
    {
      std::cerr << "Usage: server <listen_port> <bcast_address> <bcast_port>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    tcp::endpoint listen_endpoint(tcp::v4(), atoi(argv[1]));

    udp::endpoint broadcast_endpoint(
        boost::asio::ip::make_address(argv[2]), atoi(argv[3]));

    server s(io_context, listen_endpoint, broadcast_endpoint);

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
