//
// chat_server.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

using boost::asio::ip::tcp;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::redirect_error;
using boost::asio::use_awaitable;

//----------------------------------------------------------------------

class chat_participant
{
public:
  virtual ~chat_participant() {}
  virtual void deliver(const std::string& msg) = 0;
};

typedef std::shared_ptr<chat_participant> chat_participant_ptr;

//----------------------------------------------------------------------

class chat_room
{
public:
  void join(chat_participant_ptr participant)
  {
    participants_.insert(participant);
    for (auto msg: recent_msgs_)
      participant->deliver(msg);
  }

  void leave(chat_participant_ptr participant)
  {
    participants_.erase(participant);
  }

  void deliver(const std::string& msg)
  {
    recent_msgs_.push_back(msg);
    while (recent_msgs_.size() > max_recent_msgs)
      recent_msgs_.pop_front();

    for (auto participant: participants_)
      participant->deliver(msg);
  }

private:
  std::set<chat_participant_ptr> participants_;
  enum { max_recent_msgs = 100 };
  std::deque<std::string> recent_msgs_;
};

//----------------------------------------------------------------------

class chat_session
  : public chat_participant,
    public std::enable_shared_from_this<chat_session>
{
public:
  chat_session(tcp::socket socket, chat_room& room)
    : socket_(std::move(socket)),
      timer_(socket_.get_executor()),
      room_(room)
  {
    timer_.expires_at(std::chrono::steady_clock::time_point::max());
  }

  void start()
  {
    room_.join(shared_from_this());

    co_spawn(socket_.get_executor(),
        [self = shared_from_this()]{ return self->reader(); },
        detached);

    co_spawn(socket_.get_executor(),
        [self = shared_from_this()]{ return self->writer(); },
        detached);
  }

  void deliver(const std::string& msg)
  {
    write_msgs_.push_back(msg);
    timer_.cancel_one();
  }

private:
  awaitable<void> reader()
  {
    try
    {
      for (std::string read_msg;;)
      {
        std::size_t n = co_await boost::asio::async_read_until(socket_,
            boost::asio::dynamic_buffer(read_msg, 1024), "\n", use_awaitable);

        room_.deliver(read_msg.substr(0, n));
        read_msg.erase(0, n);
      }
    }
    catch (std::exception&)
    {
      stop();
    }
  }

  awaitable<void> writer()
  {
    try
    {
      while (socket_.is_open())
      {
        if (write_msgs_.empty())
        {
          boost::system::error_code ec;
          co_await timer_.async_wait(redirect_error(use_awaitable, ec));
        }
        else
        {
          co_await boost::asio::async_write(socket_,
              boost::asio::buffer(write_msgs_.front()), use_awaitable);
          write_msgs_.pop_front();
        }
      }
    }
    catch (std::exception&)
    {
      stop();
    }
  }

  void stop()
  {
    room_.leave(shared_from_this());
    socket_.close();
    timer_.cancel();
  }

  tcp::socket socket_;
  boost::asio::steady_timer timer_;
  chat_room& room_;
  std::deque<std::string> write_msgs_;
};

//----------------------------------------------------------------------

awaitable<void> listener(tcp::acceptor acceptor)
{
  chat_room room;

  for (;;)
  {
    std::make_shared<chat_session>(
        co_await acceptor.async_accept(use_awaitable),
        room
      )->start();
  }
}

//----------------------------------------------------------------------

int main(int argc, char* argv[])
{
  try
  {
    if (argc < 2)
    {
      std::cerr << "Usage: chat_server <port> [<port> ...]\n";
      return 1;
    }

    boost::asio::io_context io_context(1);

    for (int i = 1; i < argc; ++i)
    {
      unsigned short port = std::atoi(argv[i]);
      co_spawn(io_context,
          listener(tcp::acceptor(io_context, {tcp::v4(), port})),
          detached);
    }

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](auto, auto){ io_context.stop(); });

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
