//
// read_until.cpp
// ~~~~~~~~~~~~~~
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
#include "asio/read_until.hpp"

#include <cstring>
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
    using namespace std; // For memcpy.

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

void test_char_read_until()
{
  asio::io_context ioc;
  test_stream s(ioc);
  asio::streambuf sb1;
  asio::streambuf sb2(25);
  asio::error_code ec;

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  std::size_t length = asio::read_until(s, sb1, 'Z');
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = asio::read_until(s, sb1, 'Z');
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = asio::read_until(s, sb1, 'Z');
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  length = asio::read_until(s, sb1, 'Z', ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = asio::read_until(s, sb1, 'Z', ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = asio::read_until(s, sb1, 'Z', ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, 'Z', ec);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, 'Z', ec);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, 'Z', ec);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, 'Y', ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, 'Y', ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, 'Y', ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);
}

void test_string_read_until()
{
  asio::io_context ioc;
  test_stream s(ioc);
  asio::streambuf sb1;
  asio::streambuf sb2(25);
  asio::error_code ec;

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  std::size_t length = asio::read_until(s, sb1, "XYZ");
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = asio::read_until(s, sb1, "XYZ");
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = asio::read_until(s, sb1, "XYZ");
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  length = asio::read_until(s, sb1, "XYZ", ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = asio::read_until(s, sb1, "XYZ", ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = asio::read_until(s, sb1, "XYZ", ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, "XYZ", ec);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, "XYZ", ec);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, "XYZ", ec);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, "WXY", ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, "WXY", ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, "WXY", ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);
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

namespace asio {
  template <> struct is_match_condition<match_char>
  {
    enum { value = true };
  };
} // namespace asio

void test_match_condition_read_until()
{
  asio::io_context ioc;
  test_stream s(ioc);
  asio::streambuf sb1;
  asio::streambuf sb2(25);
  asio::error_code ec;

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  std::size_t length = asio::read_until(s, sb1, match_char('Z'));
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = asio::read_until(s, sb1, match_char('Z'));
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = asio::read_until(s, sb1, match_char('Z'));
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb1.consume(sb1.size());
  length = asio::read_until(s, sb1, match_char('Z'), ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb1.consume(sb1.size());
  length = asio::read_until(s, sb1, match_char('Z'), ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb1.consume(sb1.size());
  length = asio::read_until(s, sb1, match_char('Z'), ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, match_char('Z'), ec);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, match_char('Z'), ec);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, match_char('Z'), ec);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, match_char('Y'), ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, match_char('Y'), ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  sb2.consume(sb2.size());
  length = asio::read_until(s, sb2, match_char('Y'), ec);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);
}

void async_read_handler(
    const asio::error_code& err, asio::error_code* err_out,
    std::size_t bytes_transferred, std::size_t* bytes_out, bool* called)
{
  *err_out = err;
  *bytes_out = bytes_transferred;
  *called = true;
}

void test_char_async_read_until()
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
  asio::streambuf sb1;
  asio::streambuf sb2(25);
  asio::error_code ec;
  std::size_t length;
  bool called;

  s.reset(read_data, sizeof(read_data));
  ec = asio::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  asio::async_read_until(s, sb1, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  asio::async_read_until(s, sb1, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  asio::async_read_until(s, sb1, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, 'Z',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, 'Y',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, 'Y',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, 'Y',
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  int i = asio::async_read_until(s, sb2, 'Y',
      archetypes::lazy_handler());
  ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
}

void test_string_async_read_until()
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
  asio::streambuf sb1;
  asio::streambuf sb2(25);
  asio::error_code ec;
  std::size_t length;
  bool called;

  s.reset(read_data, sizeof(read_data));
  ec = asio::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  asio::async_read_until(s, sb1, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  asio::async_read_until(s, sb1, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  asio::async_read_until(s, sb1, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, "XYZ",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, "WXY",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, "WXY",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, "WXY",
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  int i = asio::async_read_until(s, sb2, "WXY",
      archetypes::lazy_handler());
  ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
}

void test_match_condition_async_read_until()
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
  asio::streambuf sb1;
  asio::streambuf sb2(25);
  asio::error_code ec;
  std::size_t length;
  bool called;

  s.reset(read_data, sizeof(read_data));
  ec = asio::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  asio::async_read_until(s, sb1, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  asio::async_read_until(s, sb1, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb1.consume(sb1.size());
  asio::async_read_until(s, sb1, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 26);

  s.reset(read_data, sizeof(read_data));
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, match_char('Z'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(ec == asio::error::not_found);
  ASIO_CHECK(length == 0);

  s.reset(read_data, sizeof(read_data));
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, match_char('Y'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(1);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, match_char('Y'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  s.next_read_length(10);
  ec = asio::error_code();
  length = 0;
  called = false;
  sb2.consume(sb2.size());
  asio::async_read_until(s, sb2, match_char('Y'),
      bindns::bind(async_read_handler, _1, &ec,
        _2, &length, &called));
  ioc.restart();
  ioc.run();
  ASIO_CHECK(called);
  ASIO_CHECK(!ec);
  ASIO_CHECK(length == 25);

  s.reset(read_data, sizeof(read_data));
  sb2.consume(sb2.size());
  int i = asio::async_read_until(s, sb2, match_char('Y'),
      archetypes::lazy_handler());
  ASIO_CHECK(i == 42);
  ioc.restart();
  ioc.run();
}

ASIO_TEST_SUITE
(
  "read_until",
  ASIO_TEST_CASE(test_char_read_until)
  ASIO_TEST_CASE(test_string_read_until)
  ASIO_TEST_CASE(test_match_condition_read_until)
  ASIO_TEST_CASE(test_char_async_read_until)
  ASIO_TEST_CASE(test_string_async_read_until)
  ASIO_TEST_CASE(test_match_condition_async_read_until)
)
