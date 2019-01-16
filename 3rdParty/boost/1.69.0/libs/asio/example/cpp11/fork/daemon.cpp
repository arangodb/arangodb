//
// daemon.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/signal_set.hpp>
#include <array>
#include <ctime>
#include <iostream>
#include <syslog.h>
#include <unistd.h>

using boost::asio::ip::udp;

class udp_daytime_server
{
public:
  udp_daytime_server(boost::asio::io_context& io_context)
    : socket_(io_context, {udp::v4(), 13})
  {
    receive();
  }

private:
  void receive()
  {
    socket_.async_receive_from(
        boost::asio::buffer(recv_buffer_), remote_endpoint_,
        [this](boost::system::error_code ec, std::size_t /*n*/)
        {
          if (!ec)
          {
            using namespace std; // For time_t, time and ctime;
            time_t now = time(0);
            std::string message = ctime(&now);

            boost::system::error_code ignored_ec;
            socket_.send_to(boost::asio::buffer(message),
                remote_endpoint_, 0, ignored_ec);
          }

          receive();
        });
  }

  udp::socket socket_;
  udp::endpoint remote_endpoint_;
  std::array<char, 1> recv_buffer_;
};

int main()
{
  try
  {
    boost::asio::io_context io_context;

    // Initialise the server before becoming a daemon. If the process is
    // started from a shell, this means any errors will be reported back to the
    // user.
    udp_daytime_server server(io_context);

    // Register signal handlers so that the daemon may be shut down. You may
    // also want to register for other signals, such as SIGHUP to trigger a
    // re-read of a configuration file.
    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait(
        [&](boost::system::error_code /*ec*/, int /*signo*/)
        {
          io_context.stop();
        });

    // Inform the io_context that we are about to become a daemon. The
    // io_context cleans up any internal resources, such as threads, that may
    // interfere with forking.
    io_context.notify_fork(boost::asio::io_context::fork_prepare);

    // Fork the process and have the parent exit. If the process was started
    // from a shell, this returns control to the user. Forking a new process is
    // also a prerequisite for the subsequent call to setsid().
    if (pid_t pid = fork())
    {
      if (pid > 0)
      {
        // We're in the parent process and need to exit.
        //
        // When the exit() function is used, the program terminates without
        // invoking local variables' destructors. Only global variables are
        // destroyed. As the io_context object is a local variable, this means
        // we do not have to call:
        //
        //   io_context.notify_fork(boost::asio::io_context::fork_parent);
        //
        // However, this line should be added before each call to exit() if
        // using a global io_context object. An additional call:
        //
        //   io_context.notify_fork(boost::asio::io_context::fork_prepare);
        //
        // should also precede the second fork().
        exit(0);
      }
      else
      {
        syslog(LOG_ERR | LOG_USER, "First fork failed: %m");
        return 1;
      }
    }

    // Make the process a new session leader. This detaches it from the
    // terminal.
    setsid();

    // A process inherits its working directory from its parent. This could be
    // on a mounted filesystem, which means that the running daemon would
    // prevent this filesystem from being unmounted. Changing to the root
    // directory avoids this problem.
    chdir("/");

    // The file mode creation mask is also inherited from the parent process.
    // We don't want to restrict the permissions on files created by the
    // daemon, so the mask is cleared.
    umask(0);

    // A second fork ensures the process cannot acquire a controlling terminal.
    if (pid_t pid = fork())
    {
      if (pid > 0)
      {
        exit(0);
      }
      else
      {
        syslog(LOG_ERR | LOG_USER, "Second fork failed: %m");
        return 1;
      }
    }

    // Close the standard streams. This decouples the daemon from the terminal
    // that started it.
    close(0);
    close(1);
    close(2);

    // We don't want the daemon to have any standard input.
    if (open("/dev/null", O_RDONLY) < 0)
    {
      syslog(LOG_ERR | LOG_USER, "Unable to open /dev/null: %m");
      return 1;
    }

    // Send standard output to a log file.
    const char* output = "/tmp/asio.daemon.out";
    const int flags = O_WRONLY | O_CREAT | O_APPEND;
    const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    if (open(output, flags, mode) < 0)
    {
      syslog(LOG_ERR | LOG_USER, "Unable to open output file %s: %m", output);
      return 1;
    }

    // Also send standard error to the same log file.
    if (dup(1) < 0)
    {
      syslog(LOG_ERR | LOG_USER, "Unable to dup output descriptor: %m");
      return 1;
    }

    // Inform the io_context that we have finished becoming a daemon. The
    // io_context uses this opportunity to create any internal file descriptors
    // that need to be private to the new process.
    io_context.notify_fork(boost::asio::io_context::fork_child);

    // The io_context can now be used normally.
    syslog(LOG_INFO | LOG_USER, "Daemon started");
    io_context.run();
    syslog(LOG_INFO | LOG_USER, "Daemon stopped");
  }
  catch (std::exception& e)
  {
    syslog(LOG_ERR | LOG_USER, "Exception: %s", e.what());
    std::cerr << "Exception: " << e.what() << std::endl;
  }
}
