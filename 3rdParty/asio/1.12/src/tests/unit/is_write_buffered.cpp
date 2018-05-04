//
// is_write_buffered.cpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include "asio/is_write_buffered.hpp"

#include "asio/buffered_read_stream.hpp"
#include "asio/buffered_write_stream.hpp"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "unit_test.hpp"

using namespace std; // For memcmp, memcpy and memset.

class test_stream
{
public:
  typedef asio::io_context io_context_type;

  typedef test_stream lowest_layer_type;

  typedef io_context_type::executor_type executor_type;

  test_stream(asio::io_context& io_context)
    : io_context_(io_context)
  {
  }

  io_context_type& io_context()
  {
    return io_context_;
  }

  lowest_layer_type& lowest_layer()
  {
    return *this;
  }

  template <typename Const_Buffers>
  size_t write(const Const_Buffers&)
  {
    return 0;
  }

  template <typename Const_Buffers>
  size_t write(const Const_Buffers&, asio::error_code& ec)
  {
    ec = asio::error_code();
    return 0;
  }

  template <typename Const_Buffers, typename Handler>
  void async_write(const Const_Buffers&, Handler handler)
  {
    asio::error_code error;
    asio::post(io_context_,
        asio::detail::bind_handler(handler, error, 0));
  }

  template <typename Mutable_Buffers>
  size_t read(const Mutable_Buffers&)
  {
    return 0;
  }

  template <typename Mutable_Buffers>
  size_t read(const Mutable_Buffers&, asio::error_code& ec)
  {
    ec = asio::error_code();
    return 0;
  }

  template <typename Mutable_Buffers, typename Handler>
  void async_read(const Mutable_Buffers&, Handler handler)
  {
    asio::error_code error;
    asio::post(io_context_,
        asio::detail::bind_handler(handler, error, 0));
  }

private:
  io_context_type& io_context_;
};

void is_write_buffered_test()
{
  ASIO_CHECK(!asio::is_write_buffered<
      asio::ip::tcp::socket>::value);

  ASIO_CHECK(!asio::is_write_buffered<
      asio::buffered_read_stream<
        asio::ip::tcp::socket> >::value);

  ASIO_CHECK(!!asio::is_write_buffered<
      asio::buffered_write_stream<
        asio::ip::tcp::socket> >::value);

  ASIO_CHECK(!!asio::is_write_buffered<
      asio::buffered_stream<asio::ip::tcp::socket> >::value);

  ASIO_CHECK(!asio::is_write_buffered<test_stream>::value);

  ASIO_CHECK(!asio::is_write_buffered<
      asio::buffered_read_stream<test_stream> >::value);

  ASIO_CHECK(!!asio::is_write_buffered<
      asio::buffered_write_stream<test_stream> >::value);

  ASIO_CHECK(!!asio::is_write_buffered<
      asio::buffered_stream<test_stream> >::value);
}

ASIO_TEST_SUITE
(
  "is_write_buffered",
  ASIO_TEST_CASE(is_write_buffered_test)
)
