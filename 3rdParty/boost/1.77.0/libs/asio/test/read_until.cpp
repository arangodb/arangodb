//
// read_until.cpp
// ~~~~~~~~~~~~~~
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

// Test that header file is self-contained.
#include <boost/asio/read_until.hpp>

#include <cstring>
#include "archetypes/async_result.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/streambuf.hpp>
#include "unit_test.hpp"

#if defined(BOOST_ASIO_HAS_BOOST_BIND)
# include <boost/bind/bind.hpp>
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
# include <functional>
#endif // defined(BOOST_ASIO_HAS_BOOST_BIND)

class test_stream
{
public:
  typedef boost::asio::io_context::executor_type executor_type;

  test_stream(boost::asio::io_context& io_context)
    : io_context_(io_context),
      length_(0),
      position_(0),
      next_read_length_(0)
  {
  }

  executor_type get_executor() BOOST_ASIO_NOEXCEPT
  {
    return io_context_.get_executor();
  }

  void reset(const void* data, size_t length)
  {
    using namespace std; // For memcpy.

    BOOST_ASIO_CHECK(length <= max_length);

    memcpy(data_, data, length);
    length_ = length;
    position_ = 0;
    next_read_length_ = length;
  }

  void next_read_length(size_t length)
  {
    next_read_length_ = length;
  }

  template <typename Mutable_Buffers>
  size_t read_some(const Mutable_Buffers& buffers)
  {
    size_t n = boost::asio::buffer_copy(buffers,
        boost::asio::buffer(data_, length_) + position_,
        next_read_length_);
    position_ += n;
    return n;
  }

  template <typename Mutable_Buffers>
  size_t read_some(const Mutable_Buffers& buffers,
      boost::system::error_code& ec)
  {
    ec = boost::system::error_code();
    return read_some(buffers);
  }

  template <typename Mutable_Buffers, typename Handler>
  void async_read_some(const Mutable_Buffers& buffers, Handler handler)
  {
    size_t bytes_transferred = read_some(buffers);
    boost::asio::post(get_executor(),
        boost::asio::detail::bind_handler(
          BOOST_ASIO_MOVE_CAST(Handler)(handler),
          boost::system::error_code(), bytes_transferred));
  }

private:
  boost::asio::io_context& io_context_;
  enum { max_length = 8192 };
  char data_[max_length];
  size_t length_;
  size_t position_;
  size_t next_read_length_;
};

static const char read_data[]
  = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

void test_dynamic_string_read_until_char()
{
  boost::asio::io_context ioc;
  test_stream s(ioc);
  std::string data1, data2;
  boost::asio::dynamic_string_buffer<char, std::string::traits_type,
    std::string::allocator_type> sb1 = boost::asio::dynamic_buffer(data1);
  boost::asio::dynamic_string_buffer<char, std::string::traits_type,
    std::string::allocator_type> sb2 = boost::asio::dynamic_buffer(data2, 25);
  boost::system::error_code ec;

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  std::size_t length = boost::asio::read_until(s, sb1, 'Z');
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, 'Z');
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, 'Z');
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, 'Z', ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, 'Z', ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, 'Z', ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, 'Z', ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, 'Z', ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, 'Z', ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, 'Y', ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, 'Y', ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, 'Y', ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);
}

void test_streambuf_read_until_char()
{
#if !defined(BOOST_ASIO_NO_DYNAMIC_BUFFER_V1)
  boost::asio::io_context ioc;
  test_stream s(ioc);
  boost::asio::streambuf sb1;
  boost::asio::streambuf sb2(25);
  boost::system::error_code ec;

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  std::size_t length = boost::asio::read_until(s, sb1, 'Z');
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, 'Z');
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, 'Z');
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, 'Z', ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, 'Z', ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, 'Z', ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, 'Z', ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, 'Z', ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, 'Z', ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, 'Y', ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, 'Y', ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, 'Y', ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);
#endif // !defined(BOOST_ASIO_NO_DYNAMIC_BUFFER_V1)
}

void test_dynamic_string_read_until_string()
{
  boost::asio::io_context ioc;
  test_stream s(ioc);
  std::string data1, data2;
  boost::asio::dynamic_string_buffer<char, std::string::traits_type,
    std::string::allocator_type> sb1 = boost::asio::dynamic_buffer(data1);
  boost::asio::dynamic_string_buffer<char, std::string::traits_type,
    std::string::allocator_type> sb2 = boost::asio::dynamic_buffer(data2, 25);
  boost::system::error_code ec;

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  std::size_t length = boost::asio::read_until(s, sb1, "XYZ");
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, "XYZ");
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, "XYZ");
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, "XYZ", ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, "XYZ", ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, "XYZ", ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, "XYZ", ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, "XYZ", ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, "XYZ", ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, "WXY", ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, "WXY", ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, "WXY", ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);
}

void test_streambuf_read_until_string()
{
#if !defined(BOOST_ASIO_NO_DYNAMIC_BUFFER_V1)
  boost::asio::io_context ioc;
  test_stream s(ioc);
  boost::asio::streambuf sb1;
  boost::asio::streambuf sb2(25);
  boost::system::error_code ec;

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  std::size_t length = boost::asio::read_until(s, sb1, "XYZ");
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, "XYZ");
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, "XYZ");
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, "XYZ", ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, "XYZ", ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, "XYZ", ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, "XYZ", ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, "XYZ", ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, "XYZ", ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, "WXY", ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, "WXY", ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, "WXY", ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);
#endif // !defined(BOOST_ASIO_NO_DYNAMIC_BUFFER_V1)
}

class match_char
{
public:
  explicit match_char(char c) : c_(c) {}

  template <typename Iterator>
  std::pair<Iterator, bool> operator()(
      Iterator begin, Iterator end) const
  {
    Iterator i = begin;
    while (i != end)
      if (c_ == *i++)
        return std::make_pair(i, true);
    return std::make_pair(i, false);
  }

private:
  char c_;
};

namespace boost {
namespace asio {
  template <> struct is_match_condition<match_char>
  {
    enum { value = true };
  };
} // namespace asio
} // namespace boost

void test_dynamic_string_read_until_match_condition()
{
  boost::asio::io_context ioc;
  test_stream s(ioc);
  std::string data1, data2;
  boost::asio::dynamic_string_buffer<char, std::string::traits_type,
    std::string::allocator_type> sb1 = boost::asio::dynamic_buffer(data1);
  boost::asio::dynamic_string_buffer<char, std::string::traits_type,
    std::string::allocator_type> sb2 = boost::asio::dynamic_buffer(data2, 25);
  boost::system::error_code ec;

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  std::size_t length = boost::asio::read_until(s, sb1, match_char('Z'));
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, match_char('Z'));
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, match_char('Z'));
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, match_char('Z'), ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, match_char('Z'), ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, match_char('Z'), ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, match_char('Z'), ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, match_char('Z'), ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, match_char('Z'), ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, match_char('Y'), ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, match_char('Y'), ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, match_char('Y'), ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);
}

void test_streambuf_read_until_match_condition()
{
#if !defined(BOOST_ASIO_NO_DYNAMIC_BUFFER_V1)
  boost::asio::io_context ioc;
  test_stream s(ioc);
  boost::asio::streambuf sb1;
  boost::asio::streambuf sb2(25);
  boost::system::error_code ec;

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  std::size_t length = boost::asio::read_until(s, sb1, match_char('Z'));
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, match_char('Z'));
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, match_char('Z'));
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, match_char('Z'), ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, match_char('Z'), ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = boost::asio::read_until(s, sb1, match_char('Z'), ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, match_char('Z'), ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, match_char('Z'), ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, match_char('Z'), ec);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, match_char('Y'), ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, match_char('Y'), ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = boost::asio::read_until(s, sb2, match_char('Y'), ec);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);
#endif // !defined(BOOST_ASIO_NO_DYNAMIC_BUFFER_V1)
}

void async_read_handler(
    const boost::system::error_code& err, boost::system::error_code* err_out,
    std::size_t bytes_transferred, std::size_t* bytes_out, bool* called)
{
  *err_out = err;
  *bytes_out = bytes_transferred;
  *called = true;
}

void test_dynamic_string_async_read_until_char()
{
#if defined(BOOST_ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
#endif // defined(BOOST_ASIO_HAS_BOOST_BIND)
  using bindns::placeholders::_1;
  using bindns::placeholders::_2;

  boost::asio::io_context ioc;
  test_stream s(ioc);
  std::string data1, data2;
  boost::asio::dynamic_string_buffer<char, std::string::traits_type,
    std::string::allocator_type> sb1 = boost::asio::dynamic_buffer(data1);
  boost::asio::dynamic_string_buffer<char, std::string::traits_type,
    std::string::allocator_type> sb2 = boost::asio::dynamic_buffer(data2, 25);
  boost::system::error_code ec;
  std::size_t length;
  bool called;

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, 'Y',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, 'Y',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, 'Y',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  int i = boost::asio::async_read_until(s, sb2, 'Y',
      archetypes::lazy_handler());
  BOOST_ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
}

void test_streambuf_async_read_until_char()
{
#if !defined(BOOST_ASIO_NO_DYNAMIC_BUFFER_V1)
#if defined(BOOST_ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
#endif // defined(BOOST_ASIO_HAS_BOOST_BIND)
  using bindns::placeholders::_1;
  using bindns::placeholders::_2;

  boost::asio::io_context ioc;
  test_stream s(ioc);
  boost::asio::streambuf sb1;
  boost::asio::streambuf sb2(25);
  boost::system::error_code ec;
  std::size_t length;
  bool called;

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, 'Y',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, 'Y',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, 'Y',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  int i = boost::asio::async_read_until(s, sb2, 'Y',
      archetypes::lazy_handler());
  BOOST_ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
#endif // !defined(BOOST_ASIO_NO_DYNAMIC_BUFFER_V1)
}

void test_dynamic_string_async_read_until_string()
{
#if defined(BOOST_ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
#endif // defined(BOOST_ASIO_HAS_BOOST_BIND)
  using bindns::placeholders::_1;
  using bindns::placeholders::_2;

  boost::asio::io_context ioc;
  test_stream s(ioc);
  std::string data1, data2;
  boost::asio::dynamic_string_buffer<char, std::string::traits_type,
    std::string::allocator_type> sb1 = boost::asio::dynamic_buffer(data1);
  boost::asio::dynamic_string_buffer<char, std::string::traits_type,
    std::string::allocator_type> sb2 = boost::asio::dynamic_buffer(data2, 25);
  boost::system::error_code ec;
  std::size_t length;
  bool called;

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, "WXY",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, "WXY",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, "WXY",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  int i = boost::asio::async_read_until(s, sb2, "WXY",
      archetypes::lazy_handler());
  BOOST_ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
}

void test_streambuf_async_read_until_string()
{
#if !defined(BOOST_ASIO_NO_DYNAMIC_BUFFER_V1)
#if defined(BOOST_ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
#endif // defined(BOOST_ASIO_HAS_BOOST_BIND)
  using bindns::placeholders::_1;
  using bindns::placeholders::_2;

  boost::asio::io_context ioc;
  test_stream s(ioc);
  boost::asio::streambuf sb1;
  boost::asio::streambuf sb2(25);
  boost::system::error_code ec;
  std::size_t length;
  bool called;

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, "WXY",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, "WXY",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, "WXY",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  int i = boost::asio::async_read_until(s, sb2, "WXY",
      archetypes::lazy_handler());
  BOOST_ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
#endif // !defined(BOOST_ASIO_NO_DYNAMIC_BUFFER_V1)
}

void test_dynamic_string_async_read_until_match_condition()
{
#if defined(BOOST_ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
#endif // defined(BOOST_ASIO_HAS_BOOST_BIND)
  using bindns::placeholders::_1;
  using bindns::placeholders::_2;

  boost::asio::io_context ioc;
  test_stream s(ioc);
  std::string data1, data2;
  boost::asio::dynamic_string_buffer<char, std::string::traits_type,
    std::string::allocator_type> sb1 = boost::asio::dynamic_buffer(data1);
  boost::asio::dynamic_string_buffer<char, std::string::traits_type,
    std::string::allocator_type> sb2 = boost::asio::dynamic_buffer(data2, 25);
  boost::system::error_code ec;
  std::size_t length;
  bool called;

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, match_char('Y'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, match_char('Y'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, match_char('Y'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  int i = boost::asio::async_read_until(s, sb2, match_char('Y'),
      archetypes::lazy_handler());
  BOOST_ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
}

void test_streambuf_async_read_until_match_condition()
{
#if !defined(BOOST_ASIO_NO_DYNAMIC_BUFFER_V1)
#if defined(BOOST_ASIO_HAS_BOOST_BIND)
  namespace bindns = boost;
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
  namespace bindns = std;
#endif // defined(BOOST_ASIO_HAS_BOOST_BIND)
  using bindns::placeholders::_1;
  using bindns::placeholders::_2;

  boost::asio::io_context ioc;
  test_stream s(ioc);
  boost::asio::streambuf sb1;
  boost::asio::streambuf sb2(25);
  boost::system::error_code ec;
  std::size_t length;
  bool called;

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  boost::asio::async_read_until(s, sb1, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
  BOOST_ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, match_char('Y'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, match_char('Y'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = boost::system::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  boost::asio::async_read_until(s, sb2, match_char('Y'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  BOOST_ASIO_CHECK(called);
  BOOST_ASIO_CHECK(!ec);
  BOOST_ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  int i = boost::asio::async_read_until(s, sb2, match_char('Y'),
      archetypes::lazy_handler());
  BOOST_ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
#endif // !defined(BOOST_ASIO_NO_DYNAMIC_BUFFER_V1)
}

BOOST_ASIO_TEST_SUITE
(
  "read_until",
  BOOST_ASIO_TEST_CASE(test_dynamic_string_read_until_char)
  BOOST_ASIO_TEST_CASE(test_streambuf_read_until_char)
  BOOST_ASIO_TEST_CASE(test_dynamic_string_read_until_string)
  BOOST_ASIO_TEST_CASE(test_streambuf_read_until_string)
  BOOST_ASIO_TEST_CASE(test_dynamic_string_read_until_match_condition)
  BOOST_ASIO_TEST_CASE(test_streambuf_read_until_match_condition)
  BOOST_ASIO_TEST_CASE(test_dynamic_string_async_read_until_char)
  BOOST_ASIO_TEST_CASE(test_streambuf_async_read_until_char)
  BOOST_ASIO_TEST_CASE(test_dynamic_string_async_read_until_string)
  BOOST_ASIO_TEST_CASE(test_streambuf_async_read_until_string)
  BOOST_ASIO_TEST_CASE(test_dynamic_string_async_read_until_match_condition)
  BOOST_ASIO_TEST_CASE(test_streambuf_async_read_until_match_condition)
)
