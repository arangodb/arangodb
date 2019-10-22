//
// composed_8.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/compose.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/write.hpp>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

using boost::asio::ip::tcp;

// NOTE: This example requires the new boost::asio::async_compose function. For
// an example that works with the Networking TS style of completion tokens,
// please see an older version of asio.

//------------------------------------------------------------------------------

// This composed operation shows composition of multiple underlying operations,
// using asio's stackless coroutines support to express the flow of control. It
// automatically serialises a message, using its I/O streams insertion
// operator, before sending it N times on the socket. To do this, it must
// allocate a buffer for the encoded message and ensure this buffer's validity
// until all underlying async_write operation complete. A one second delay is
// inserted prior to each write operation, using a steady_timer.

#include <boost/asio/yield.hpp>

template <typename T, typename CompletionToken>
auto async_write_messages(tcp::socket& socket,
    const T& message, std::size_t repeat_count,
    CompletionToken&& token)
  // The return type of the initiating function is deduced from the combination
  // of CompletionToken type and the completion handler's signature. When the
  // completion token is a simple callback, the return type is always void.
  // In this example, when the completion token is boost::asio::yield_context
  // (used for stackful coroutines) the return type would be also be void, as
  // there is no non-error argument to the completion handler. When the
  // completion token is boost::asio::use_future it would be std::future<void>.
  //
  // In C++14 we can omit the return type as it is automatically deduced from
  // the return type of boost::asio::async_initiate.
{
  // Encode the message and copy it into an allocated buffer. The buffer will
  // be maintained for the lifetime of the composed asynchronous operation.
  std::ostringstream os;
  os << message;
  std::unique_ptr<std::string> encoded_message(new std::string(os.str()));

  // Create a steady_timer to be used for the delay between messages.
  std::unique_ptr<boost::asio::steady_timer> delay_timer(
      new boost::asio::steady_timer(socket.get_executor()));

  // The boost::asio::async_compose function takes:
  //
  // - our asynchronous operation implementation,
  // - the completion token,
  // - the completion handler signature, and
  // - any I/O objects (or executors) used by the operation
  //
  // It then wraps our implementation, which is implemented here as a stackless
  // coroutine in a lambda, in an intermediate completion handler that meets the
  // requirements of a conforming asynchronous operation. This includes
  // tracking outstanding work against the I/O executors associated with the
  // operation (in this example, this is the socket's executor).
  //
  // The first argument to our lambda is a reference to the enclosing
  // intermediate completion handler. This intermediate completion handler is
  // provided for us by the boost::asio::async_compose function, and takes care
  // of all the details required to implement a conforming asynchronous
  // operation. When calling an underlying asynchronous operation, we pass it
  // this enclosing intermediate completion handler as the completion token.
  //
  // All arguments to our lambda after the first must be defaulted to allow the
  // state machine to be started, as well as to allow the completion handler to
  // match the completion signature of both the async_write and
  // steady_timer::async_wait operations.
  return boost::asio::async_compose<
    CompletionToken, void(boost::system::error_code)>(
      [
        // The implementation holds a reference to the socket as it is used for
        // multiple async_write operations.
        &socket,

        // The allocated buffer for the encoded message. The std::unique_ptr
        // smart pointer is move-only, and as a consequence our lambda
        // implementation is also move-only.
        encoded_message = std::move(encoded_message),

        // The repeat count remaining.
        repeat_count,

        // A steady timer used for introducing a delay.
        delay_timer = std::move(delay_timer),

        // The coroutine state.
        coro = boost::asio::coroutine()
      ]
      (
        auto& self,
        const boost::system::error_code& error = {},
        std::size_t = 0
      ) mutable
      {
        reenter (coro)
        {
          while (repeat_count > 0)
          {
            --repeat_count;

            delay_timer->expires_after(std::chrono::seconds(1));
            yield delay_timer->async_wait(std::move(self));
            if (error)
              break;

            yield boost::asio::async_write(socket,
                boost::asio::buffer(*encoded_message), std::move(self));
            if (error)
              break;
          }

          // Deallocate the encoded message and delay timer before calling the
          // user-supplied completion handler.
          encoded_message.reset();
          delay_timer.reset();

          // Call the user-supplied handler with the result of the operation.
          self.complete(error);
        }
      },
      token, socket);
}

#include <boost/asio/unyield.hpp>

//------------------------------------------------------------------------------

void test_callback()
{
  boost::asio::io_context io_context;

  tcp::acceptor acceptor(io_context, {tcp::v4(), 55555});
  tcp::socket socket = acceptor.accept();

  // Test our asynchronous operation using a lambda as a callback.
  async_write_messages(socket, "Testing callback\r\n", 5,
      [](const boost::system::error_code& error)
      {
        if (!error)
        {
          std::cout << "Messages sent\n";
        }
        else
        {
          std::cout << "Error: " << error.message() << "\n";
        }
      });

  io_context.run();
}

//------------------------------------------------------------------------------

void test_future()
{
  boost::asio::io_context io_context;

  tcp::acceptor acceptor(io_context, {tcp::v4(), 55555});
  tcp::socket socket = acceptor.accept();

  // Test our asynchronous operation using the use_future completion token.
  // This token causes the operation's initiating function to return a future,
  // which may be used to synchronously wait for the result of the operation.
  std::future<void> f = async_write_messages(
      socket, "Testing future\r\n", 5, boost::asio::use_future);

  io_context.run();

  try
  {
    // Get the result of the operation.
    f.get();
    std::cout << "Messages sent\n";
  }
  catch (const std::exception& e)
  {
    std::cout << "Error: " << e.what() << "\n";
  }
}

//------------------------------------------------------------------------------

int main()
{
  test_callback();
  test_future();
}
