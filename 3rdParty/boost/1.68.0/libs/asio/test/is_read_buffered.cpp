//
// is_read_buffered.cpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include <boost/asio/is_read_buffered.hpp>

#include <boost/asio/buffered_read_stream.hpp>
#include <boost/asio/buffered_write_stream.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include "unit_test.hpp"

using namespace std; // For memcmp, memcpy and memset.

class test_stream
{
public:
  typedef boost::asio::io_context io_context_type;

  typedef test_stream lowest_layer_type;

  typedef io_context_type::executor_type executor_type;

  test_stream(boost::asio::io_context& io_context)
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
  size_t write(const Const_Buffers&, boost::system::error_code& ec)
  {
    ec = boost::system::error_code();
    return 0;
  }

  template <typename Const_Buffers, typename Handler>
  void async_write(const Const_Buffers&, Handler handler)
  {
    boost::system::error_code error;
    boost::asio::post(io_context_,
        boost::asio::detail::bind_handler(handler, error, 0));
  }

  template <typename Mutable_Buffers>
  size_t read(const Mutable_Buffers&)
  {
    return 0;
  }

  template <typename Mutable_Buffers>
  size_t read(const Mutable_Buffers&, boost::system::error_code& ec)
  {
    ec = boost::system::error_code();
    return 0;
  }

  template <typename Mutable_Buffers, typename Handler>
  void async_read(const Mutable_Buffers&, Handler handler)
  {
    boost::system::error_code error;
    boost::asio::post(io_context_,
        boost::asio::detail::bind_handler(handler, error, 0));
  }

private:
  io_context_type& io_context_;
};

void is_read_buffered_test()
{
  BOOST_ASIO_CHECK(!boost::asio::is_read_buffered<
      boost::asio::ip::tcp::socket>::value);

  BOOST_ASIO_CHECK(!!boost::asio::is_read_buffered<
      boost::asio::buffered_read_stream<
        boost::asio::ip::tcp::socket> >::value);

  BOOST_ASIO_CHECK(!boost::asio::is_read_buffered<
      boost::asio::buffered_write_stream<
        boost::asio::ip::tcp::socket> >::value);

  BOOST_ASIO_CHECK(!!boost::asio::is_read_buffered<
      boost::asio::buffered_stream<boost::asio::ip::tcp::socket> >::value);

  BOOST_ASIO_CHECK(!boost::asio::is_read_buffered<test_stream>::value);

  BOOST_ASIO_CHECK(!!boost::asio::is_read_buffered<
      boost::asio::buffered_read_stream<test_stream> >::value);

  BOOST_ASIO_CHECK(!boost::asio::is_read_buffered<
      boost::asio::buffered_write_stream<test_stream> >::value);

  BOOST_ASIO_CHECK(!!boost::asio::is_read_buffered<
      boost::asio::buffered_stream<test_stream> >::value);
}

BOOST_ASIO_TEST_SUITE
(
  "is_read_buffered",
  BOOST_ASIO_TEST_CASE(is_read_buffered_test)
)
