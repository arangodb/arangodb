//
// composed_4.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
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

// NOTE: This example requires the new boost::asio::async_initiate function. For
// an example that works with the Networking TS style of completion tokens,
// please see an older version of asio.

//------------------------------------------------------------------------------

// In this composed operation we repackage an existing operation, but with a
// different completion handler signature. We will also intercept an empty
// message as an invalid argument, and propagate the corresponding error to the
// user. The asynchronous operation requirements are met by delegating
// responsibility to the underlying operation.

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
  //
  // In C++14 we can omit the return type as it is automatically deduced from
  // the return type of boost::asio::async_initiate.
{
  // In addition to determining the mechanism by which an asynchronous
  // operation delivers its result, a completion token also determines the time
  // when the operation commences. For example, when the completion token is a
  // simple callback the operation commences before the initiating function
  // returns. However, if the completion token's delivery mechanism uses a
  // future, we might instead want to defer initiation of the operation until
  // the returned future object is waited upon.
  //
  // To enable this, when implementing an asynchronous operation we must
  // package the initiation step as a function object. The initiation function
  // object's call operator is passed the concrete completion handler produced
  // by the completion token. This completion handler matches the asynchronous
  // operation's completion handler signature, which in this example is:
  //
  //   void(boost::system::error_code error)
  //
  // The initiation function object also receives any additional arguments
  // required to start the operation. (Note: We could have instead passed these
  // arguments in the lambda capture set. However, we should prefer to
  // propagate them as function call arguments as this allows the completion
  // token to optimise how they are passed. For example, a lazy future which
  // defers initiation would need to make a decay-copy of the arguments, but
  // when using a simple callback the arguments can be trivially forwarded
  // straight through.)
  auto initiation = [](auto&& completion_handler,
      tcp::socket& socket, const char* message)
  {
    // The post operation has a completion handler signature of:
    //
    //   void()
    //
    // and the async_write operation has a completion handler signature of:
    //
    //   void(boost::system::error_code error, std::size n)
    //
    // Both of these operations' completion handler signatures differ from our
    // operation's completion handler signature. We will adapt our completion
    // handler to these signatures by using std::bind, which drops the
    // additional arguments.
    //
    // However, it is essential to the correctness of our composed operation
    // that we preserve the executor of the user-supplied completion handler.
    // The std::bind function will not do this for us, so we must do this by
    // first obtaining the completion handler's associated executor (defaulting
    // to the I/O executor - in this case the executor of the socket - if the
    // completion handler does not have its own) ...
    auto executor = boost::asio::get_associated_executor(
        completion_handler, socket.get_executor());

    // ... and then binding this executor to our adapted completion handler
    // using the boost::asio::bind_executor function.
    std::size_t length = std::strlen(message);
    if (length == 0)
    {
      boost::asio::post(
          boost::asio::bind_executor(executor,
            std::bind(std::forward<decltype(completion_handler)>(
                completion_handler), boost::asio::error::invalid_argument)));
    }
    else
    {
      boost::asio::async_write(socket,
          boost::asio::buffer(message, length),
          boost::asio::bind_executor(executor,
            std::bind(std::forward<decltype(completion_handler)>(
                completion_handler), std::placeholders::_1)));
    }
  };

  // The boost::asio::async_initiate function takes:
  //
  // - our initiation function object,
  // - the completion token,
  // - the completion handler signature, and
  // - any additional arguments we need to initiate the operation.
  //
  // It then asks the completion token to create a completion handler (i.e. a
  // callback) with the specified signature, and invoke the initiation function
  // object with this completion handler as well as the additional arguments.
  // The return value of async_initiate is the result of our operation's
  // initiating function.
  //
  // Note that we wrap non-const reference arguments in std::reference_wrapper
  // to prevent incorrect decay-copies of these objects.
  return boost::asio::async_initiate<
    CompletionToken, void(boost::system::error_code)>(
      initiation, token, std::ref(socket), message);
}

//------------------------------------------------------------------------------

void test_callback()
{
  boost::asio::io_context io_context;

  tcp::acceptor acceptor(io_context, {tcp::v4(), 55555});
  tcp::socket socket = acceptor.accept();

  // Test our asynchronous operation using a lambda as a callback.
  async_write_message(socket, "",
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
      socket, "", boost::asio::use_future);

  io_context.run();

  try
  {
    // Get the result of the operation.
    f.get();
    std::cout << "Message sent\n";
  }
  catch (const std::exception& e)
  {
    std::cout << "Exception: " << e.what() << "\n";
  }
}

//------------------------------------------------------------------------------

int main()
{
  test_callback();
  test_future();
}
