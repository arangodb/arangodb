//
// composed_4.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
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

//------------------------------------------------------------------------------

// This composed operation automatically serialises a message, using its I/O
// streams insertion operator, before sending it on the socket. To do this, it
// must allocate a buffer for the encoded message and ensure this buffer's
// validity until the underlying async_write operation completes.
template <typename T, typename CompletionToken>
auto async_write_message(tcp::socket& socket,
    const T& message, CompletionToken&& token)
  // The return type of the initiating function is deduced from the combination
  // of CompletionToken type and the completion handler's signature. When the
  // completion token is a simple callback, the return type is always void.
  // In this example, when the completion token is boost::asio::yield_context
  // (used for stackful coroutines) the return type would be also be void, as
  // there is no non-error argument to the completion handler. When the
  // completion token is boost::asio::use_future it would be std::future<void>.
  -> typename boost::asio::async_result<
    typename std::decay<CompletionToken>::type,
    void(boost::system::error_code)>::return_type
{
  // Define a type alias for the concrete completion handler, as we will use
  // the type in several places in the implementation below.
  using completion_handler_type =
    typename boost::asio::async_completion<CompletionToken,
      void(boost::system::error_code)>::completion_handler_type;

  // In this example, the composed operation's intermediate completion handler
  // is implemented as a hand-crafted function object, rather than a lambda.
  struct intermediate_completion_handler
  {
    // The intermediate completion handler holds a reference to the socket so
    // that it can obtain the I/O executor (see get_executor below).
    tcp::socket& socket_;

    // The allocated buffer for the encoded message. The std::unique_ptr smart
    // pointer is move-only, and as a consequence our intermediate completion
    // handler is also move-only.
    std::unique_ptr<std::string> encoded_message_;

    // The user-supplied completion handler.
    completion_handler_type handler_;

    // The function call operator matches the completion signature of the
    // async_write operation.
    void operator()(const boost::system::error_code& error, std::size_t /*n*/)
    {
      // Deallocate the encoded message before calling the user-supplied
      // completion handler.
      encoded_message_.reset();

      // Call the user-supplied handler with the result of the operation.
      // The arguments must match the completion signature of our composed
      // operation.
      handler_(error);
    }

    // It is essential to the correctness of our composed operation that we
    // preserve the executor of the user-supplied completion handler. With a
    // hand-crafted function object we can do this by defining a nested type
    // executor_type and member function get_executor. These obtain the
    // completion handler's associated executor, and default to the I/O
    // executor - in this case the executor of the socket - if the completion
    // handler does not have its own.
    using executor_type = boost::asio::associated_executor_t<
      completion_handler_type, tcp::socket::executor_type>;

    executor_type get_executor() const noexcept
    {
      return boost::asio::get_associated_executor(
          handler_, socket_.get_executor());
    }

    // Although not necessary for correctness, we may also preserve the
    // allocator of the user-supplied completion handler. This is achieved by
    // defining a nested type allocator_type and member function get_allocator.
    // These obtain the completion handler's associated allocator, and default
    // to std::allocator<void> if the completion handler does not have its own.
    using allocator_type = boost::asio::associated_allocator_t<
      completion_handler_type, std::allocator<void>>;

    allocator_type get_allocator() const noexcept
    {
      return boost::asio::get_associated_allocator(
          handler_, std::allocator<void>{});
    }
  };

  // The boost::asio::async_completion object takes the completion token and
  // from it creates:
  //
  // - completion.completion_handler:
  //     A completion handler (i.e. a callback) with the specified signature.
  //
  // - completion.result:
  //     An object from which we obtain the result of the initiating function.
  boost::asio::async_completion<CompletionToken,
    void(boost::system::error_code)> completion(token);

  // Encode the message and copy it into an allocated buffer. The buffer will
  // be maintained for the lifetime of the asynchronous operation.
  std::ostringstream os;
  os << message;
  std::unique_ptr<std::string> encoded_message(new std::string(os.str()));

  // Initiate the underlying async_write operation using our intermediate
  // completion handler.
  boost::asio::async_write(socket, boost::asio::buffer(*encoded_message),
      intermediate_completion_handler{socket, std::move(encoded_message),
        std::move(completion.completion_handler)});

  // Finally, we return the result of the initiating function.
  return completion.result.get();
}

//------------------------------------------------------------------------------

void test_callback()
{
  boost::asio::io_context io_context;

  tcp::acceptor acceptor(io_context, {tcp::v4(), 55555});
  tcp::socket socket = acceptor.accept();

  // Test our asynchronous operation using a lambda as a callback.
  async_write_message(socket, "Testing callback\r\n",
      [](const boost::system::error_code& error)
      {
        if (!error)
        {
          std::cout << "Message sent\n";
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
  std::future<void> f = async_write_message(
      socket, "Testing future\r\n", boost::asio::use_future);

  io_context.run();

  try
  {
    // Get the result of the operation.
    f.get();
    std::cout << "Message sent\n";
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
