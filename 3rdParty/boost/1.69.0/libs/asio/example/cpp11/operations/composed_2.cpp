//
// composed_2.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/write.hpp>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>

using boost::asio::ip::tcp;

//------------------------------------------------------------------------------

// In this composed operation we repackage an existing operation, but with a
// different completion handler signature. The asynchronous operation
// requirements are met by delegating responsibility to the underlying
// operation.
template <typename CompletionToken>
auto async_write_message(tcp::socket& socket,
    const char* message, CompletionToken&& token)
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

  // The async_write operation has a completion handler signature of:
  //
  //   void(boost::system::error_code error, std::size n)
  //
  // This differs from our operation's signature in that it is also passed the
  // number of bytes transferred as an argument of type std::size_t. We will
  // adapt our completion handler to this signature by using std::bind, which
  // drops the additional argument.
  //
  // However, it is essential to the correctness of our composed operation that
  // we preserve the executor of the user-supplied completion handler. The
  // std::bind function will not do this for us, so we must do this by first
  // obtaining the completion handler's associated executor (defaulting to the
  // I/O executor - in this case the executor of the socket - if the completion
  // handler does not have its own) ...
  auto executor = boost::asio::get_associated_executor(
      completion.completion_handler, socket.get_executor());

  // ... and then binding this executor to our adapted completion handler using
  // the boost::asio::bind_executor function.
  boost::asio::async_write(socket,
      boost::asio::buffer(message, std::strlen(message)),
      boost::asio::bind_executor(executor,
        std::bind(std::move(completion.completion_handler),
          std::placeholders::_1)));

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

  // Get the result of the operation.
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
