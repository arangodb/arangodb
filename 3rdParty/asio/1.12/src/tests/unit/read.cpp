//
// read.cpp
// ~~~~~~~~
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
#include "asio/read.hpp"

#include <cstring>
#include <vector>
#include "archetypes/async_result.hpp"
#include "asio/io_context.hpp"
#include "asio/post.hpp"
#include "asio/streambuf.hpp"
#include "unit_test.hpp"

#if defined(ASIO_HAS_BOOST_BIND)
# include <boost/bind.hpp>
#else // defined(ASIO_HAS_BOOST_BIND)
# include <functional>
#endif // defined(ASIO_HAS_BOOST_BIND)

#if defined(ASIO_HAS_BOOST_ARRAY)
#include <boost/array.hpp>
#endif // defined(ASIO_HAS_BOOST_ARRAY)

#if defined(ASIO_HAS_STD_ARRAY)
# include <array>
#endif // defined(ASIO_HAS_STD_ARRAY)

using namespace std; // For memcmp, memcpy and memset.

class test_stream
{
public:
  typedef asio::io_context::executor_type executor_type;

  test_stream(asio::io_context& io_context)
    : io_context_(io_context),
      length_(0),
      position_(0),
      next_read_length_(0)
  {
  }

  executor_type get_executor() ASIO_NOEXCEPT
  {
    return io_context_.get_executor();
  }

  void reset(const void* data, size_t length)
  {
    ASIO_CHECK(length <= max_length);

    memcpy(data_, data, length);
    length_ = length;
    position_ = 0;
    next_read_length_ = length;
  }

  void next_read_length(size_t length)
  {
    next_read_length_ = length;
  }

  template <typename Iterator>
  bool check_buffers(Iterator begin, Iterator end, size_t length)
  {
    if (length != position_)
      return false;

    Iterator iter = begin;
    size_t checked_length = 0;
    for (; iter != end && checked_length < length; ++iter)
    {
      size_t buffer_length = asio::buffer_size(*iter);
      if (buffer_length > length - checked_length)
        buffer_length = length - checked_length;
      if (memcmp(data_ + checked_length, iter->data(), buffer_length) != 0)
        return false;
      checked_length += buffer_length;
    }

    return true;
  }

  template <typename Const_Buffers>
  bool check_buffers(const Const_Buffers& buffers, size_t length)
  {
    return check_buffers(asio::buffer_sequence_begin(buffers),
        asio::buffer_sequence_end(buffers), length);
  }

  template <typename Mutable_Buffers>
  size_t read_some(const Mutable_Buffers& buffers)
  {
    size_t n = asio::buffer_copy(buffers,
        asio::buffer(data_, length_) + position_,
        next_read_length_);
    position_ += n;
    return n;
  }

  template <typename Mutable_Buffers>
  size_t read_some(const Mutable_Buffers& buffers,
      asio::error_code& ec)
  {
    ec = asio::error_code();
    return read_some(buffers);
  }

  template <typename Mutable_Buffers, typename Handler>
  void async_read_some(const Mutable_Buffers& buffers, Handler handler)
  {
    size_t bytes_transferred = read_some(buffers);
    asio::post(get_executor(),
        asio::detail::bind_handler(
          ASIO_MOVE_CAST(Handler)(handler),
          asio::error_code(), bytes_transferred));
  }

private:
  asio::io_context& io_context_;
  enum { max_length = 8192 };
  char data_[max_length];
  size_t length_;
  size_t position_;
  size_t next_read_length_;
};

static const char read_data[]
  = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

void test_2_arg_zero_buffers_read()
{
  asio::io_context ioc;
  test_stream s(ioc);
  std::vector<asio::mutable_buffer> buffers;

  size_t bytes_transferred = asio::read(s, buffers);
  ASIO_CHECK(bytes_transferred == 0);
}

void test_2_arg_mutable_buffer_read()
{
  asio::io_context ioc;
  test_stream s(ioc);
  char read_buf[sizeof(read_data)];
  asio::mutable_buffer buffers
    = asio::buffer(read_buf, sizeof(read_buf));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  size_t bytes_transferred = asio::read(s, buffers);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
}

void test_2_arg_vector_buffers_read()
{
  asio::io_context ioc;
  test_stream s(ioc);
  char read_buf[sizeof(read_data)];
  std::vector<asio::mutable_buffer> buffers;
  buffers.push_back(asio::buffer(read_buf, 32));
  buffers.push_back(asio::buffer(read_buf, 39) + 32);
  buffers.push_back(asio::buffer(read_buf) + 39);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  size_t bytes_transferred = asio::read(s, buffers);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
}

void test_2_arg_streambuf_read()
{
  asio::io_context ioc;
  test_stream s(ioc);
  asio::streambuf sb(sizeof(read_data));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  size_t bytes_transferred = asio::read(s, sb);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
}

void test_3_arg_nothrow_zero_buffers_read()
{
  asio::io_context ioc;
  test_stream s(ioc);
  std::vector<asio::mutable_buffer> buffers;

  asio::error_code error;
  size_t bytes_transferred = asio::read(s, buffers, error);
  ASIO_CHECK(bytes_transferred == 0);
  ASIO_CHECK(!error);
}

void test_3_arg_nothrow_mutable_buffer_read()
{
  asio::io_context ioc;
  test_stream s(ioc);
  char read_buf[sizeof(read_data)];
  asio::mutable_buffer buffers
    = asio::buffer(read_buf, sizeof(read_buf));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  asio::error_code error;
  size_t bytes_transferred = asio::read(s, buffers, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);
}

void test_3_arg_nothrow_vector_buffers_read()
{
  asio::io_context ioc;
  test_stream s(ioc);
  char read_buf[sizeof(read_data)];
  std::vector<asio::mutable_buffer> buffers;
  buffers.push_back(asio::buffer(read_buf, 32));
  buffers.push_back(asio::buffer(read_buf, 39) + 32);
  buffers.push_back(asio::buffer(read_buf) + 39);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  asio::error_code error;
  size_t bytes_transferred = asio::read(s, buffers, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);
}

void test_3_arg_nothrow_streambuf_read()
{
  asio::io_context ioc;
  test_stream s(ioc);
  asio::streambuf sb(sizeof(read_data));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  asio::error_code error;
  size_t bytes_transferred = asio::read(s, sb, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
  ASIO_CHECK(!error);
}

bool old_style_transfer_all(const asio::error_code& ec,
    size_t /*bytes_transferred*/)
{
  return !!ec;
}

size_t short_transfer(const asio::error_code& ec,
    size_t /*bytes_transferred*/)
{
  return !!ec ? 0 : 3;
}

void test_3_arg_mutable_buffer_read()
{
  asio::io_context ioc;
  test_stream s(ioc);
  char read_buf[sizeof(read_data)];
  asio::mutable_buffer buffers
    = asio::buffer(read_buf, sizeof(read_buf));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  size_t bytes_transferred = asio::read(s, buffers,
      asio::transfer_all());
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_all());
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_all());
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(1));
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(1));
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(1));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(10));
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(10));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(10));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(42));
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(42));
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(42));
  ASIO_CHECK(bytes_transferred == 50);
  ASIO_CHECK(s.check_buffers(buffers, 50));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(1));
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(1));
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(1));
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(10));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(10));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(10));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(42));
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(42));
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(42));
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, old_style_transfer_all);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, old_style_transfer_all);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, old_style_transfer_all);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, short_transfer);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, short_transfer);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, short_transfer);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
}

void test_3_arg_vector_buffers_read()
{
  asio::io_context ioc;
  test_stream s(ioc);
  char read_buf[sizeof(read_data)];
  std::vector<asio::mutable_buffer> buffers;
  buffers.push_back(asio::buffer(read_buf, 32));
  buffers.push_back(asio::buffer(read_buf, 39) + 32);
  buffers.push_back(asio::buffer(read_buf) + 39);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  size_t bytes_transferred = asio::read(s, buffers,
      asio::transfer_all());
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_all());
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_all());
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(1));
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(1));
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(1));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(10));
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(10));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(10));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(42));
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(42));
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(42));
  ASIO_CHECK(bytes_transferred == 50);
  ASIO_CHECK(s.check_buffers(buffers, 50));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(1));
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(1));
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(1));
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(10));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(10));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(10));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(42));
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(42));
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(42));
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, old_style_transfer_all);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, old_style_transfer_all);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, old_style_transfer_all);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, short_transfer);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, short_transfer);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, short_transfer);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
}

void test_3_arg_streambuf_read()
{
  asio::io_context ioc;
  test_stream s(ioc);
  asio::streambuf sb(sizeof(read_data));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  size_t bytes_transferred = asio::read(s, sb,
      asio::transfer_all());
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_all());
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_all());
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(1));
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(1));
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(sb.size() == 1);
  ASIO_CHECK(s.check_buffers(sb.data(), 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(1));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(10));
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(10));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(10));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(42));
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(42));
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(sb.size() == 42);
  ASIO_CHECK(s.check_buffers(sb.data(), 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(42));
  ASIO_CHECK(bytes_transferred == 50);
  ASIO_CHECK(sb.size() == 50);
  ASIO_CHECK(s.check_buffers(sb.data(), 50));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(1));
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(sb.size() == 1);
  ASIO_CHECK(s.check_buffers(sb.data(), 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(1));
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(sb.size() == 1);
  ASIO_CHECK(s.check_buffers(sb.data(), 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(1));
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(sb.size() == 1);
  ASIO_CHECK(s.check_buffers(sb.data(), 1));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(10));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(10));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(10));
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(42));
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(sb.size() == 42);
  ASIO_CHECK(s.check_buffers(sb.data(), 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(42));
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(sb.size() == 42);
  ASIO_CHECK(s.check_buffers(sb.data(), 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(42));
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(sb.size() == 42);
  ASIO_CHECK(s.check_buffers(sb.data(), 42));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb, old_style_transfer_all);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb, old_style_transfer_all);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb, old_style_transfer_all);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb, short_transfer);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb, short_transfer);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb, short_transfer);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
}

void test_4_arg_mutable_buffer_read()
{
  asio::io_context ioc;
  test_stream s(ioc);
  char read_buf[sizeof(read_data)];
  asio::mutable_buffer buffers
    = asio::buffer(read_buf, sizeof(read_buf));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  asio::error_code error;
  size_t bytes_transferred = asio::read(s, buffers,
      asio::transfer_all(), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_all(), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_all(), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(1), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(1), error);
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(s.check_buffers(buffers, 1));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(1), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(10), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(10), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(10), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(42), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(42), error);
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(s.check_buffers(buffers, 42));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(42), error);
  ASIO_CHECK(bytes_transferred == 50);
  ASIO_CHECK(s.check_buffers(buffers, 50));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(1), error);
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(s.check_buffers(buffers, 1));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(1), error);
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(s.check_buffers(buffers, 1));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(1), error);
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(s.check_buffers(buffers, 1));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(10), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(10), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(10), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(42), error);
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(s.check_buffers(buffers, 42));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(42), error);
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(s.check_buffers(buffers, 42));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(42), error);
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(s.check_buffers(buffers, 42));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      old_style_transfer_all, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      old_style_transfer_all, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      old_style_transfer_all, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, short_transfer, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers, short_transfer, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers, short_transfer, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);
}

void test_4_arg_vector_buffers_read()
{
  asio::io_context ioc;
  test_stream s(ioc);
  char read_buf[sizeof(read_data)];
  std::vector<asio::mutable_buffer> buffers;
  buffers.push_back(asio::buffer(read_buf, 32));
  buffers.push_back(asio::buffer(read_buf, 39) + 32);
  buffers.push_back(asio::buffer(read_buf) + 39);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  asio::error_code error;
  size_t bytes_transferred = asio::read(s, buffers,
      asio::transfer_all(), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_all(), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_all(), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(1), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(1), error);
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(s.check_buffers(buffers, 1));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(1), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(10), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(10), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(10), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(42), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(42), error);
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(s.check_buffers(buffers, 42));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_at_least(42), error);
  ASIO_CHECK(bytes_transferred == 50);
  ASIO_CHECK(s.check_buffers(buffers, 50));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(1), error);
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(s.check_buffers(buffers, 1));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(1), error);
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(s.check_buffers(buffers, 1));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(1), error);
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(s.check_buffers(buffers, 1));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(10), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(10), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(10), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(s.check_buffers(buffers, 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(42), error);
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(s.check_buffers(buffers, 42));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(42), error);
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(s.check_buffers(buffers, 42));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      asio::transfer_exactly(42), error);
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(s.check_buffers(buffers, 42));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers,
      old_style_transfer_all, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      old_style_transfer_all, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers,
      old_style_transfer_all, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bytes_transferred = asio::read(s, buffers, short_transfer, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers, short_transfer, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  error = asio::error_code();
  bytes_transferred = asio::read(s, buffers, short_transfer, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
  ASIO_CHECK(!error);
}

void test_4_arg_streambuf_read()
{
  asio::io_context ioc;
  test_stream s(ioc);
  asio::streambuf sb(sizeof(read_data));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  asio::error_code error;
  size_t bytes_transferred = asio::read(s, sb,
      asio::transfer_all(), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_all(), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_all(), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(1), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(1), error);
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(sb.size() == 1);
  ASIO_CHECK(s.check_buffers(sb.data(), 1));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(1), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(10), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(10), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(10), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(42), error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(42), error);
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(sb.size() == 42);
  ASIO_CHECK(s.check_buffers(sb.data(), 42));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_at_least(42), error);
  ASIO_CHECK(bytes_transferred == 50);
  ASIO_CHECK(sb.size() == 50);
  ASIO_CHECK(s.check_buffers(sb.data(), 50));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(1), error);
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(sb.size() == 1);
  ASIO_CHECK(s.check_buffers(sb.data(), 1));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(1), error);
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(sb.size() == 1);
  ASIO_CHECK(s.check_buffers(sb.data(), 1));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(1), error);
  ASIO_CHECK(bytes_transferred == 1);
  ASIO_CHECK(sb.size() == 1);
  ASIO_CHECK(s.check_buffers(sb.data(), 1));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(10), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(10), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(10), error);
  ASIO_CHECK(bytes_transferred == 10);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(42), error);
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(sb.size() == 42);
  ASIO_CHECK(s.check_buffers(sb.data(), 42));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(42), error);
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(sb.size() == 42);
  ASIO_CHECK(s.check_buffers(sb.data(), 42));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      asio::transfer_exactly(42), error);
  ASIO_CHECK(bytes_transferred == 42);
  ASIO_CHECK(sb.size() == 42);
  ASIO_CHECK(s.check_buffers(sb.data(), 42));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb,
      old_style_transfer_all, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      old_style_transfer_all, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb,
      old_style_transfer_all, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  bytes_transferred = asio::read(s, sb, short_transfer, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb, short_transfer, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
  ASIO_CHECK(!error);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  error = asio::error_code();
  bytes_transferred = asio::read(s, sb, short_transfer, error);
  ASIO_CHECK(bytes_transferred == sizeof(read_data));
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
  ASIO_CHECK(!error);
}

void async_read_handler(const asio::error_code& e,
    size_t bytes_transferred, size_t expected_bytes_transferred, bool* called)
{
  *called = true;
  ASIO_CHECK(!e);
  ASIO_CHECK(bytes_transferred == expected_bytes_transferred);
}

void test_3_arg_mutable_buffer_async_read()
{
#if defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
  using std::placeholders::_1;
  using std::placeholders::_2;
#endif // defined(ASIO_HAS_BOOST_BIND)

  asio::io_context ioc;
  test_stream s(ioc);
  char read_buf[sizeof(read_data)];
  asio::mutable_buffer buffers
    = asio::buffer(read_buf, sizeof(read_buf));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bool called = false;
  asio::async_read(s, buffers,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  int i = asio::async_read(s, buffers, archetypes::lazy_handler());
  ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
}

void test_3_arg_boost_array_buffers_async_read()
{
#if defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
  using std::placeholders::_1;
  using std::placeholders::_2;
#endif // defined(ASIO_HAS_BOOST_BIND)

#if defined(ASIO_HAS_BOOST_ARRAY)
  asio::io_context ioc;
  test_stream s(ioc);
  char read_buf[sizeof(read_data)];
  boost::array<asio::mutable_buffer, 2> buffers = { {
    asio::buffer(read_buf, 32),
    asio::buffer(read_buf) + 32 } };

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bool called = false;
  asio::async_read(s, buffers,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  int i = asio::async_read(s, buffers, archetypes::lazy_handler());
  ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
#endif // defined(ASIO_HAS_BOOST_ARRAY)
}

void test_3_arg_std_array_buffers_async_read()
{
#if defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
  using std::placeholders::_1;
  using std::placeholders::_2;
#endif // defined(ASIO_HAS_BOOST_BIND)

#if defined(ASIO_HAS_STD_ARRAY)
  asio::io_context ioc;
  test_stream s(ioc);
  char read_buf[sizeof(read_data)];
  std::array<asio::mutable_buffer, 2> buffers = { {
    asio::buffer(read_buf, 32),
    asio::buffer(read_buf) + 32 } };

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bool called = false;
  asio::async_read(s, buffers,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  int i = asio::async_read(s, buffers, archetypes::lazy_handler());
  ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
#endif // defined(ASIO_HAS_STD_ARRAY)
}

void test_3_arg_vector_buffers_async_read()
{
#if defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
  using std::placeholders::_1;
  using std::placeholders::_2;
#endif // defined(ASIO_HAS_BOOST_BIND)

  asio::io_context ioc;
  test_stream s(ioc);
  char read_buf[sizeof(read_data)];
  std::vector<asio::mutable_buffer> buffers;
  buffers.push_back(asio::buffer(read_buf, 32));
  buffers.push_back(asio::buffer(read_buf, 39) + 32);
  buffers.push_back(asio::buffer(read_buf) + 39);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bool called = false;
  asio::async_read(s, buffers,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  int i = asio::async_read(s, buffers, archetypes::lazy_handler());
  ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
}

void test_3_arg_streambuf_async_read()
{
#if defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
  using std::placeholders::_1;
  using std::placeholders::_2;
#endif // defined(ASIO_HAS_BOOST_BIND)

  asio::io_context ioc;
  test_stream s(ioc);
  asio::streambuf sb(sizeof(read_data));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  bool called = false;
  asio::async_read(s, sb,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  int i = asio::async_read(s, sb, archetypes::lazy_handler());
  ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
}

void test_4_arg_mutable_buffer_async_read()
{
#if defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
  using std::placeholders::_1;
  using std::placeholders::_2;
#endif // defined(ASIO_HAS_BOOST_BIND)

  asio::io_context ioc;
  test_stream s(ioc);
  char read_buf[sizeof(read_data)];
  asio::mutable_buffer buffers
    = asio::buffer(read_buf, sizeof(read_buf));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bool called = false;
  asio::async_read(s, buffers, asio::transfer_all(),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_all(),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_all(),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(1),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(1),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(10),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(42),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(42),
      bindns::bind(async_read_handler,
        _1, _2, 50, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 50));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, old_style_transfer_all,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, old_style_transfer_all,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, old_style_transfer_all,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, short_transfer,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, short_transfer,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, short_transfer,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  int i = asio::async_read(s, buffers,
      short_transfer, archetypes::lazy_handler());
  ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
}

void test_4_arg_boost_array_buffers_async_read()
{
#if defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
  using std::placeholders::_1;
  using std::placeholders::_2;
#endif // defined(ASIO_HAS_BOOST_BIND)

#if defined(ASIO_HAS_BOOST_ARRAY)
  asio::io_context ioc;
  test_stream s(ioc);
  char read_buf[sizeof(read_data)];
  boost::array<asio::mutable_buffer, 2> buffers = { {
    asio::buffer(read_buf, 32),
    asio::buffer(read_buf) + 32 } };

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bool called = false;
  asio::async_read(s, buffers, asio::transfer_all(),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_all(),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_all(),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(1),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(1),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(10),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(42),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(42),
      bindns::bind(async_read_handler,
        _1, _2, 50, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 50));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, old_style_transfer_all,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, old_style_transfer_all,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, old_style_transfer_all,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, short_transfer,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, short_transfer,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, short_transfer,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  int i = asio::async_read(s, buffers,
      short_transfer, archetypes::lazy_handler());
  ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
#endif // defined(ASIO_HAS_BOOST_ARRAY)
}

void test_4_arg_std_array_buffers_async_read()
{
#if defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
  using std::placeholders::_1;
  using std::placeholders::_2;
#endif // defined(ASIO_HAS_BOOST_BIND)

#if defined(ASIO_HAS_STD_ARRAY)
  asio::io_context ioc;
  test_stream s(ioc);
  char read_buf[sizeof(read_data)];
  std::array<asio::mutable_buffer, 2> buffers = { {
    asio::buffer(read_buf, 32),
    asio::buffer(read_buf) + 32 } };

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bool called = false;
  asio::async_read(s, buffers, asio::transfer_all(),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_all(),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_all(),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(1),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(1),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(10),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(42),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(42),
      bindns::bind(async_read_handler,
        _1, _2, 50, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 50));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, old_style_transfer_all,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, old_style_transfer_all,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, old_style_transfer_all,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, short_transfer,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, short_transfer,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, short_transfer,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  int i = asio::async_read(s, buffers,
      short_transfer, archetypes::lazy_handler());
  ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
#endif // defined(ASIO_HAS_STD_ARRAY)
}

void test_4_arg_vector_buffers_async_read()
{
#if defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
  using std::placeholders::_1;
  using std::placeholders::_2;
#endif // defined(ASIO_HAS_BOOST_BIND)

  asio::io_context ioc;
  test_stream s(ioc);
  char read_buf[sizeof(read_data)];
  std::vector<asio::mutable_buffer> buffers;
  buffers.push_back(asio::buffer(read_buf, 32));
  buffers.push_back(asio::buffer(read_buf, 39) + 32);
  buffers.push_back(asio::buffer(read_buf) + 39);

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  bool called = false;
  asio::async_read(s, buffers, asio::transfer_all(),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_all(),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_all(),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(1),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(1),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(10),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(42),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_at_least(42),
      bindns::bind(async_read_handler,
        _1, _2, 50, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 50));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 1));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 10));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, asio::transfer_exactly(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, 42));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, old_style_transfer_all,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, old_style_transfer_all,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, old_style_transfer_all,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, short_transfer,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, short_transfer,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  memset(read_buf, 0, sizeof(read_buf));
  called = false;
  asio::async_read(s, buffers, short_transfer,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  memset(read_buf, 0, sizeof(read_buf));
  int i = asio::async_read(s, buffers,
      short_transfer, archetypes::lazy_handler());
  ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
  ASIO_CHECK(s.check_buffers(buffers, sizeof(read_data)));
}

void test_4_arg_streambuf_async_read()
{
#if defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
  using std::placeholders::_1;
  using std::placeholders::_2;
#endif // defined(ASIO_HAS_BOOST_BIND)

  asio::io_context ioc;
  test_stream s(ioc);
  asio::streambuf sb(sizeof(read_data));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  bool called = false;
  asio::async_read(s, sb, asio::transfer_all(),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_all(),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_all(),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_at_least(1),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_at_least(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == 1);
  ASIO_CHECK(s.check_buffers(sb.data(), 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_at_least(1),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_at_least(10),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_at_least(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_at_least(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_at_least(42),
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_at_least(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == 42);
  ASIO_CHECK(s.check_buffers(sb.data(), 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_at_least(42),
      bindns::bind(async_read_handler,
        _1, _2, 50, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == 50);
  ASIO_CHECK(s.check_buffers(sb.data(), 50));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_exactly(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == 1);
  ASIO_CHECK(s.check_buffers(sb.data(), 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_exactly(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == 1);
  ASIO_CHECK(s.check_buffers(sb.data(), 1));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_exactly(1),
      bindns::bind(async_read_handler,
        _1, _2, 1, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == 1);
  ASIO_CHECK(s.check_buffers(sb.data(), 1));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_exactly(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_exactly(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_exactly(10),
      bindns::bind(async_read_handler,
        _1, _2, 10, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == 10);
  ASIO_CHECK(s.check_buffers(sb.data(), 10));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_exactly(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == 42);
  ASIO_CHECK(s.check_buffers(sb.data(), 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_exactly(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == 42);
  ASIO_CHECK(s.check_buffers(sb.data(), 42));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, asio::transfer_exactly(42),
      bindns::bind(async_read_handler,
        _1, _2, 42, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == 42);
  ASIO_CHECK(s.check_buffers(sb.data(), 42));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, old_style_transfer_all,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, old_style_transfer_all,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, old_style_transfer_all,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, short_transfer,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, short_transfer,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb.consume(sb.size());
  called = false;
  asio::async_read(s, sb, short_transfer,
      bindns::bind(async_read_handler,
        _1, _2, sizeof(read_data), &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));

  s.reset(read_data, sizeof(read_data));
  sb.consume(sb.size());
  int i = asio::async_read(s, sb,
      short_transfer, archetypes::lazy_handler());
  ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
  ASIO_CHECK(sb.size() == sizeof(read_data));
  ASIO_CHECK(s.check_buffers(sb.data(), sizeof(read_data)));
}

ASIO_TEST_SUITE
(
  "read",
  ASIO_TEST_CASE(test_2_arg_zero_buffers_read)
  ASIO_TEST_CASE(test_2_arg_mutable_buffer_read)
  ASIO_TEST_CASE(test_2_arg_vector_buffers_read)
  ASIO_TEST_CASE(test_2_arg_streambuf_read)
  ASIO_TEST_CASE(test_3_arg_nothrow_zero_buffers_read)
  ASIO_TEST_CASE(test_3_arg_nothrow_mutable_buffer_read)
  ASIO_TEST_CASE(test_3_arg_nothrow_vector_buffers_read)
  ASIO_TEST_CASE(test_3_arg_nothrow_streambuf_read)
  ASIO_TEST_CASE(test_3_arg_mutable_buffer_read)
  ASIO_TEST_CASE(test_3_arg_vector_buffers_read)
  ASIO_TEST_CASE(test_3_arg_streambuf_read)
  ASIO_TEST_CASE(test_4_arg_mutable_buffer_read)
  ASIO_TEST_CASE(test_4_arg_vector_buffers_read)
  ASIO_TEST_CASE(test_4_arg_streambuf_read)
  ASIO_TEST_CASE(test_3_arg_mutable_buffer_async_read)
  ASIO_TEST_CASE(test_3_arg_boost_array_buffers_async_read)
  ASIO_TEST_CASE(test_3_arg_std_array_buffers_async_read)
  ASIO_TEST_CASE(test_3_arg_vector_buffers_async_read)
  ASIO_TEST_CASE(test_3_arg_streambuf_async_read)
  ASIO_TEST_CASE(test_4_arg_mutable_buffer_async_read)
  ASIO_TEST_CASE(test_4_arg_vector_buffers_async_read)
  ASIO_TEST_CASE(test_4_arg_boost_array_buffers_async_read)
  ASIO_TEST_CASE(test_4_arg_std_array_buffers_async_read)
  ASIO_TEST_CASE(test_4_arg_streambuf_async_read)
)
