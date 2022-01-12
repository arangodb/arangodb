//
// experimental/concurrent_channel.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
#include <boost/asio/experimental/concurrent_channel.hpp>

#include <utility>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include "../unit_test.hpp"

using namespace boost::asio;
using namespace boost::asio::experimental;

void unbuffered_concurrent_channel_test()
{
  io_context ctx;

  concurrent_channel<void(boost::system::error_code, std::string)> ch1(ctx);

  BOOST_ASIO_CHECK(ch1.is_open());
  BOOST_ASIO_CHECK(!ch1.ready());

  bool b1 = ch1.try_send(boost::asio::error::eof, "hello");

  BOOST_ASIO_CHECK(!b1);

  std::string s1 = "abcdefghijklmnopqrstuvwxyz";
  bool b2 = ch1.try_send(boost::asio::error::eof, std::move(s1));

  BOOST_ASIO_CHECK(!b2);
  BOOST_ASIO_CHECK(!s1.empty());

  boost::system::error_code ec1;
  std::string s2;
  ch1.async_receive(
      [&](boost::system::error_code ec, std::string s)
      {
        ec1 = ec;
        s2 = std::move(s);
      });

  bool b3 = ch1.try_send(boost::asio::error::eof, std::move(s1));

  BOOST_ASIO_CHECK(b3);
  BOOST_ASIO_CHECK(s1.empty());

  ctx.run();

  BOOST_ASIO_CHECK(ec1 == boost::asio::error::eof);
  BOOST_ASIO_CHECK(s2 == "abcdefghijklmnopqrstuvwxyz");

  bool b4 = ch1.try_receive([](boost::system::error_code, std::string){});

  BOOST_ASIO_CHECK(!b4);

  boost::system::error_code ec2 = boost::asio::error::would_block;
  std::string s3 = "zyxwvutsrqponmlkjihgfedcba";
  ch1.async_send(boost::asio::error::eof, std::move(s3),
      [&](boost::system::error_code ec)
      {
        ec2 = ec;
      });

  boost::system::error_code ec3;
  std::string s4;
  bool b5 = ch1.try_receive(
      [&](boost::system::error_code ec, std::string s)
      {
        ec3 = ec;
        s4 = s;
      });

  BOOST_ASIO_CHECK(b5);
  BOOST_ASIO_CHECK(ec3 == boost::asio::error::eof);
  BOOST_ASIO_CHECK(s4 == "zyxwvutsrqponmlkjihgfedcba");

  ctx.restart();
  ctx.run();

  BOOST_ASIO_CHECK(!ec2);
};

void buffered_concurrent_channel_test()
{
  io_context ctx;

  concurrent_channel<void(boost::system::error_code, std::string)> ch1(ctx, 1);

  BOOST_ASIO_CHECK(ch1.is_open());
  BOOST_ASIO_CHECK(!ch1.ready());

  bool b1 = ch1.try_send(boost::asio::error::eof, "hello");

  BOOST_ASIO_CHECK(b1);

  std::string s1 = "abcdefghijklmnopqrstuvwxyz";
  bool b2 = ch1.try_send(boost::asio::error::eof, std::move(s1));

  BOOST_ASIO_CHECK(!b2);
  BOOST_ASIO_CHECK(!s1.empty());

  boost::system::error_code ec1;
  std::string s2;
  ch1.async_receive(
      [&](boost::system::error_code ec, std::string s)
      {
        ec1 = ec;
        s2 = std::move(s);
      });

  ctx.run();

  BOOST_ASIO_CHECK(ec1 == boost::asio::error::eof);
  BOOST_ASIO_CHECK(s2 == "hello");

  bool b4 = ch1.try_receive([](boost::system::error_code, std::string){});

  BOOST_ASIO_CHECK(!b4);

  boost::system::error_code ec2 = boost::asio::error::would_block;
  std::string s3 = "zyxwvutsrqponmlkjihgfedcba";
  ch1.async_send(boost::asio::error::eof, std::move(s3),
      [&](boost::system::error_code ec)
      {
        ec2 = ec;
      });

  boost::system::error_code ec3;
  std::string s4;
  bool b5 = ch1.try_receive(
      [&](boost::system::error_code ec, std::string s)
      {
        ec3 = ec;
        s4 = s;
      });

  BOOST_ASIO_CHECK(b5);
  BOOST_ASIO_CHECK(ec3 == boost::asio::error::eof);
  BOOST_ASIO_CHECK(s4 == "zyxwvutsrqponmlkjihgfedcba");

  ctx.restart();
  ctx.run();

  BOOST_ASIO_CHECK(!ec2);
};

BOOST_ASIO_TEST_SUITE
(
  "experimental/concurrent_channel",
  BOOST_ASIO_TEST_CASE(unbuffered_concurrent_channel_test)
  BOOST_ASIO_TEST_CASE(buffered_concurrent_channel_test)
)
