//
// posix_chat_client.cpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include "asio.hpp"
#include "chat_message.hpp"

#if defined(ASIO_HAS_POSIX_STREAM_DESCRIPTOR)

using asio::ip::tcp;
namespace posix = asio::posix;

class posix_chat_client
{
public:
  posix_chat_client(asio::io_context& io_context,
      const tcp::resolver::results_type& endpoints)
    : socket_(io_context),
      input_(io_context, ::dup(STDIN_FILENO)),
      output_(io_context, ::dup(STDOUT_FILENO)),
      input_buffer_(chat_message::max_body_length)
  {
    asio::async_connect(socket_, endpoints,
        boost::bind(&posix_chat_client::handle_connect, this,
          asio::placeholders::error));
  }

private:

  void handle_connect(const asio::error_code& error)
  {
    if (!error)
    {
      // Read the fixed-length header of the next message from the server.
      asio::async_read(socket_,
          asio::buffer(read_msg_.data(), chat_message::header_length),
          boost::bind(&posix_chat_client::handle_read_header, this,
            asio::placeholders::error));

      // Read a line of input entered by the user.
      asio::async_read_until(input_, input_buffer_, '\n',
          boost::bind(&posix_chat_client::handle_read_input, this,
            asio::placeholders::error,
            asio::placeholders::bytes_transferred));
    }
  }

  void handle_read_header(const asio::error_code& error)
  {
    if (!error && read_msg_.decode_header())
    {
      // Read the variable-length body of the message from the server.
      asio::async_read(socket_,
          asio::buffer(read_msg_.body(), read_msg_.body_length()),
          boost::bind(&posix_chat_client::handle_read_body, this,
            asio::placeholders::error));
    }
    else
    {
      close();
    }
  }

  void handle_read_body(const asio::error_code& error)
  {
    if (!error)
    {
      // Write out the message we just received, terminated by a newline.
      static char eol[] = { '\n' };
      boost::array<asio::const_buffer, 2> buffers = {{
        asio::buffer(read_msg_.body(), read_msg_.body_length()),
        asio::buffer(eol) }};
      asio::async_write(output_, buffers,
          boost::bind(&posix_chat_client::handle_write_output, this,
            asio::placeholders::error));
    }
    else
    {
      close();
    }
  }

  void handle_write_output(const asio::error_code& error)
  {
    if (!error)
    {
      // Read the fixed-length header of the next message from the server.
      asio::async_read(socket_,
          asio::buffer(read_msg_.data(), chat_message::header_length),
          boost::bind(&posix_chat_client::handle_read_header, this,
            asio::placeholders::error));
    }
    else
    {
      close();
    }
  }

  void handle_read_input(const asio::error_code& error,
      std::size_t length)
  {
    if (!error)
    {
      // Write the message (minus the newline) to the server.
      write_msg_.body_length(length - 1);
      input_buffer_.sgetn(write_msg_.body(), length - 1);
      input_buffer_.consume(1); // Remove newline from input.
      write_msg_.encode_header();
      asio::async_write(socket_,
          asio::buffer(write_msg_.data(), write_msg_.length()),
          boost::bind(&posix_chat_client::handle_write, this,
            asio::placeholders::error));
    }
    else if (error == asio::error::not_found)
    {
      // Didn't get a newline. Send whatever we have.
      write_msg_.body_length(input_buffer_.size());
      input_buffer_.sgetn(write_msg_.body(), input_buffer_.size());
      write_msg_.encode_header();
      asio::async_write(socket_,
          asio::buffer(write_msg_.data(), write_msg_.length()),
          boost::bind(&posix_chat_client::handle_write, this,
            asio::placeholders::error));
    }
    else
    {
      close();
    }
  }

  void handle_write(const asio::error_code& error)
  {
    if (!error)
    {
      // Read a line of input entered by the user.
      asio::async_read_until(input_, input_buffer_, '\n',
          boost::bind(&posix_chat_client::handle_read_input, this,
            asio::placeholders::error,
            asio::placeholders::bytes_transferred));
    }
    else
    {
      close();
    }
  }

  void close()
  {
    // Cancel all outstanding asynchronous operations.
    socket_.close();
    input_.close();
    output_.close();
  }

private:
  tcp::socket socket_;
  posix::stream_descriptor input_;
  posix::stream_descriptor output_;
  chat_message read_msg_;
  chat_message write_msg_;
  asio::streambuf input_buffer_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 3)
    {
      std::cerr << "Usage: posix_chat_client <host> <port>\n";
      return 1;
    }

    asio::io_context io_context;

    tcp::resolver resolver(io_context);
    tcp::resolver::results_type endpoints = resolver.resolve(argv[1], argv[2]);

    posix_chat_client c(io_context, endpoints);

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}

#else // defined(ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
int main() {}
#endif // defined(ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
