//
// process_per_connection.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <cstdlib>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using boost::asio::ip::tcp;

class server
{
public:
  server(boost::asio::io_context& io_context, unsigned short port)
    : io_context_(io_context),
      signal_(io_context, SIGCHLD),
      acceptor_(io_context, {tcp::v4(), port}),
      socket_(io_context)
  {
    wait_for_signal();
    accept();
  }

private:
  void wait_for_signal()
  {
    signal_.async_wait(
        [this](boost::system::error_code /*ec*/, int /*signo*/)
        {
          // Only the parent process should check for this signal. We can
          // determine whether we are in the parent by checking if the acceptor
          // is still open.
          if (acceptor_.is_open())
          {
            // Reap completed child processes so that we don't end up with
            // zombies.
            int status = 0;
            while (waitpid(-1, &status, WNOHANG) > 0) {}

            wait_for_signal();
          }
        });
  }

  void accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket new_socket)
        {
          if (!ec)
          {
            // Take ownership of the newly accepted socket.
            socket_ = std::move(new_socket);

            // Inform the io_context that we are about to fork. The io_context
            // cleans up any internal resources, such as threads, that may
            // interfere with forking.
            io_context_.notify_fork(boost::asio::io_context::fork_prepare);

            if (fork() == 0)
            {
              // Inform the io_context that the fork is finished and that this
              // is the child process. The io_context uses this opportunity to
              // create any internal file descriptors that must be private to
              // the new process.
              io_context_.notify_fork(boost::asio::io_context::fork_child);

              // The child won't be accepting new connections, so we can close
              // the acceptor. It remains open in the parent.
              acceptor_.close();

              // The child process is not interested in processing the SIGCHLD
              // signal.
              signal_.cancel();

              read();
            }
            else
            {

              // Inform the io_context that the fork is finished (or failed)
              // and that this is the parent process. The io_context uses this
              // opportunity to recreate any internal resources that were
              // cleaned up during preparation for the fork.
              io_context_.notify_fork(boost::asio::io_context::fork_parent);

              // The parent process can now close the newly accepted socket. It
              // remains open in the child.
              socket_.close();

              accept();
            }
          }
          else
          {
            std::cerr << "Accept error: " << ec.message() << std::endl;
            accept();
          }
        });
  }

  void read()
  {
    socket_.async_read_some(boost::asio::buffer(data_),
        [this](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
            write(length);
        });
  }

  void write(std::size_t length)
  {
    boost::asio::async_write(socket_, boost::asio::buffer(data_, length),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
            read();
        });
  }

  boost::asio::io_context& io_context_;
  boost::asio::signal_set signal_;
  tcp::acceptor acceptor_;
  tcp::socket socket_;
  std::array<char, 1024> data_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: process_per_connection <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    using namespace std; // For atoi.
    server s(io_context, atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
}
