//
// writable_pipe.cpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header pipe is self-contained.
#include <boost/asio/writable_pipe.hpp>

#include "archetypes/async_result.hpp"
#include <boost/asio/io_context.hpp>
#include "unit_test.hpp"

// writable_pipe_compile test
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks that all public member functions on the class
// writable_pipe compile and link correctly. Runtime failures are ignored.

namespace writable_pipe_compile {

struct write_some_handler
{
  write_some_handler() {}
  void operator()(const boost::system::error_code&, std::size_t) {}
#if defined(BOOST_ASIO_HAS_MOVE)
  write_some_handler(write_some_handler&&) {}
private:
  write_some_handler(const write_some_handler&);
#endif // defined(BOOST_ASIO_HAS_MOVE)
};

struct read_some_handler
{
  read_some_handler() {}
  void operator()(const boost::system::error_code&, std::size_t) {}
#if defined(BOOST_ASIO_HAS_MOVE)
  read_some_handler(read_some_handler&&) {}
private:
  read_some_handler(const read_some_handler&);
#endif // defined(BOOST_ASIO_HAS_MOVE)
};

void test()
{
#if defined(BOOST_ASIO_HAS_PIPE)
  using namespace boost::asio;

  try
  {
    io_context ioc;
    const io_context::executor_type ioc_ex = ioc.get_executor();
    char mutable_char_buffer[128] = "";
    const char const_char_buffer[128] = "";
    archetypes::lazy_handler lazy;
    boost::system::error_code ec;

    // basic_writable_pipe constructors.

    writable_pipe pipe1(ioc);
    writable_pipe::native_handle_type native_pipe1 = pipe1.native_handle();
    writable_pipe pipe2(ioc, native_pipe1);

    writable_pipe pipe3(ioc_ex);
    writable_pipe::native_handle_type native_pipe2 = pipe1.native_handle();
    writable_pipe pipe4(ioc_ex, native_pipe2);

#if defined(BOOST_ASIO_HAS_MOVE)
    writable_pipe pipe5(std::move(pipe4));
#endif // defined(BOOST_ASIO_HAS_MOVE)

    // basic_writable_pipe operators.

#if defined(BOOST_ASIO_HAS_MOVE)
    pipe1 = writable_pipe(ioc);
    pipe1 = std::move(pipe2);
#endif // defined(BOOST_ASIO_HAS_MOVE)

    // basic_io_object functions.

    writable_pipe::executor_type ex = pipe1.get_executor();
    (void)ex;

    // basic_writable_pipe functions.

    writable_pipe::native_handle_type native_pipe3 = pipe1.native_handle();
    pipe1.assign(native_pipe3);
    writable_pipe::native_handle_type native_pipe4 = pipe1.native_handle();
    pipe1.assign(native_pipe4, ec);

    bool is_open = pipe1.is_open();
    (void)is_open;

    pipe1.close();
    pipe1.close(ec);

    writable_pipe::native_handle_type native_pipe5 = pipe1.native_handle();
    (void)native_pipe5;

    pipe1.cancel();
    pipe1.cancel(ec);

    pipe1.write_some(buffer(mutable_char_buffer));
    pipe1.write_some(buffer(const_char_buffer));
    pipe1.write_some(buffer(mutable_char_buffer), ec);
    pipe1.write_some(buffer(const_char_buffer), ec);

    pipe1.async_write_some(buffer(mutable_char_buffer), write_some_handler());
    pipe1.async_write_some(buffer(const_char_buffer), write_some_handler());
    int i1 = pipe1.async_write_some(buffer(mutable_char_buffer), lazy);
    (void)i1;
    int i2 = pipe1.async_write_some(buffer(const_char_buffer), lazy);
    (void)i2;
  }
  catch (std::exception&)
  {
  }
#endif // defined(BOOST_ASIO_HAS_PIPE)
}

} // namespace writable_pipe_compile

BOOST_ASIO_TEST_SUITE
(
  "writable_pipe",
  BOOST_ASIO_TEST_CASE(writable_pipe_compile::test)
)
