//
// connect.cpp
// ~~~~~~~~~~~
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
#include "asio/connect.hpp"

#include <vector>
#include "asio/detail/thread.hpp"
#include "asio/ip/tcp.hpp"

#if defined(ASIO_HAS_BOOST_BIND)
# include <boost/bind.hpp>
#else // defined(ASIO_HAS_BOOST_BIND)
# include <functional>
#endif // defined(ASIO_HAS_BOOST_BIND)

#include "unit_test.hpp"

#if defined(ASIO_HAS_BOOST_BIND)
namespace bindns = boost;
#else // defined(ASIO_HAS_BOOST_BIND)
namespace bindns = std;
using std::placeholders::_1;
using std::placeholders::_2;
#endif // defined(ASIO_HAS_BOOST_BIND)

class connection_sink
{
public:
  connection_sink()
    : acceptor_(io_context_,
        asio::ip::tcp::endpoint(
          asio::ip::address_v4::loopback(), 0)),
      target_endpoint_(acceptor_.local_endpoint()),
      socket_(io_context_),
      thread_(bindns::bind(&connection_sink::run, this))
  {
  }

  ~connection_sink()
  {
    io_context_.stop();
    thread_.join();
  }

  asio::ip::tcp::endpoint target_endpoint()
  {
    return target_endpoint_;
  }

private:
  void run()
  {
    io_context_.run();
  }

  void handle_accept()
  {
    socket_.close();
    acceptor_.async_accept(socket_,
        bindns::bind(&connection_sink::handle_accept, this));
  }

  asio::io_context io_context_;
  asio::ip::tcp::acceptor acceptor_;
  asio::ip::tcp::endpoint target_endpoint_;
  asio::ip::tcp::socket socket_;
  asio::detail::thread thread_;
};

bool true_cond_1(const asio::error_code& /*ec*/,
    const asio::ip::tcp::endpoint& /*endpoint*/)
{
  return true;
}

struct true_cond_2
{
  template <typename Endpoint>
  bool operator()(const asio::error_code& /*ec*/,
      const Endpoint& /*endpoint*/)
  {
    return true;
  }
};

std::vector<asio::ip::tcp::endpoint>::const_iterator legacy_true_cond_1(
    const asio::error_code& /*ec*/,
    std::vector<asio::ip::tcp::endpoint>::const_iterator next)
{
  return next;
}

struct legacy_true_cond_2
{
  template <typename Iterator>
  Iterator operator()(const asio::error_code& /*ec*/, Iterator next)
  {
    return next;
  }
};

bool false_cond(const asio::error_code& /*ec*/,
    const asio::ip::tcp::endpoint& /*endpoint*/)
{
  return false;
}

void range_handler(const asio::error_code& ec,
    const asio::ip::tcp::endpoint& endpoint,
    asio::error_code* out_ec,
    asio::ip::tcp::endpoint* out_endpoint)
{
  *out_ec = ec;
  *out_endpoint = endpoint;
}

void iter_handler(const asio::error_code& ec,
    std::vector<asio::ip::tcp::endpoint>::const_iterator iter,
    asio::error_code* out_ec,
    std::vector<asio::ip::tcp::endpoint>::const_iterator* out_iter)
{
  *out_ec = ec;
  *out_iter = iter;
}

void test_connect_range()
{
  connection_sink sink;
  asio::io_context io_context;
  asio::ip::tcp::socket socket(io_context);
  std::vector<asio::ip::tcp::endpoint> endpoints;
  asio::ip::tcp::endpoint result;

  try
  {
    result = asio::connect(socket, endpoints);
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }

  endpoints.push_back(sink.target_endpoint());

  result = asio::connect(socket, endpoints);
  ASIO_CHECK(result == endpoints[0]);

  endpoints.push_back(sink.target_endpoint());

  result = asio::connect(socket, endpoints);
  ASIO_CHECK(result == endpoints[0]);

  endpoints.insert(endpoints.begin(), asio::ip::tcp::endpoint());

  result = asio::connect(socket, endpoints);
  ASIO_CHECK(result == endpoints[1]);
}

void test_connect_range_ec()
{
  connection_sink sink;
  asio::io_context io_context;
  asio::ip::tcp::socket socket(io_context);
  std::vector<asio::ip::tcp::endpoint> endpoints;
  asio::ip::tcp::endpoint result;
  asio::error_code ec;

  result = asio::connect(socket, endpoints, ec);
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  result = asio::connect(socket, endpoints, ec);
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  endpoints.push_back(sink.target_endpoint());

  result = asio::connect(socket, endpoints, ec);
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  endpoints.insert(endpoints.begin(), asio::ip::tcp::endpoint());

  result = asio::connect(socket, endpoints, ec);
  ASIO_CHECK(result == endpoints[1]);
  ASIO_CHECK(!ec);
}

void test_connect_range_cond()
{
  connection_sink sink;
  asio::io_context io_context;
  asio::ip::tcp::socket socket(io_context);
  std::vector<asio::ip::tcp::endpoint> endpoints;
  asio::ip::tcp::endpoint result;

  try
  {
    result = asio::connect(socket, endpoints, true_cond_1);
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }

  try
  {
    result = asio::connect(socket, endpoints, true_cond_2());
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }

  try
  {
    result = asio::connect(socket, endpoints, legacy_true_cond_1);
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }

  try
  {
    result = asio::connect(socket, endpoints, legacy_true_cond_2());
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }

  try
  {
    result = asio::connect(socket, endpoints, false_cond);
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }

  endpoints.push_back(sink.target_endpoint());

  result = asio::connect(socket, endpoints, true_cond_1);
  ASIO_CHECK(result == endpoints[0]);

  result = asio::connect(socket, endpoints, true_cond_2());
  ASIO_CHECK(result == endpoints[0]);

  result = asio::connect(socket, endpoints, legacy_true_cond_1);
  ASIO_CHECK(result == endpoints[0]);

  result = asio::connect(socket, endpoints, legacy_true_cond_2());
  ASIO_CHECK(result == endpoints[0]);

  try
  {
    result = asio::connect(socket, endpoints, false_cond);
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }

  endpoints.push_back(sink.target_endpoint());

  result = asio::connect(socket, endpoints, true_cond_1);
  ASIO_CHECK(result == endpoints[0]);

  result = asio::connect(socket, endpoints, true_cond_2());
  ASIO_CHECK(result == endpoints[0]);

  result = asio::connect(socket, endpoints, legacy_true_cond_1);
  ASIO_CHECK(result == endpoints[0]);

  result = asio::connect(socket, endpoints, legacy_true_cond_2());
  ASIO_CHECK(result == endpoints[0]);

  try
  {
    result = asio::connect(socket, endpoints, false_cond);
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }

  endpoints.insert(endpoints.begin(), asio::ip::tcp::endpoint());

  result = asio::connect(socket, endpoints, true_cond_1);
  ASIO_CHECK(result == endpoints[1]);

  result = asio::connect(socket, endpoints, true_cond_2());
  ASIO_CHECK(result == endpoints[1]);

  result = asio::connect(socket, endpoints, legacy_true_cond_1);
  ASIO_CHECK(result == endpoints[1]);

  result = asio::connect(socket, endpoints, legacy_true_cond_2());
  ASIO_CHECK(result == endpoints[1]);

  try
  {
    result = asio::connect(socket, endpoints, false_cond);
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }
}

void test_connect_range_cond_ec()
{
  connection_sink sink;
  asio::io_context io_context;
  asio::ip::tcp::socket socket(io_context);
  std::vector<asio::ip::tcp::endpoint> endpoints;
  asio::ip::tcp::endpoint result;
  asio::error_code ec;

  result = asio::connect(socket, endpoints, true_cond_1, ec);
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);

  result = asio::connect(socket, endpoints, true_cond_2(), ec);
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);

  result = asio::connect(socket, endpoints, legacy_true_cond_1, ec);
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);

  result = asio::connect(socket, endpoints, legacy_true_cond_2(), ec);
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);

  result = asio::connect(socket, endpoints, false_cond, ec);
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  result = asio::connect(socket, endpoints, true_cond_1, ec);
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  result = asio::connect(socket, endpoints, true_cond_2(), ec);
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  result = asio::connect(socket, endpoints, legacy_true_cond_1, ec);
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  result = asio::connect(socket, endpoints, legacy_true_cond_2(), ec);
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  result = asio::connect(socket, endpoints, false_cond, ec);
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  result = asio::connect(socket, endpoints, true_cond_1, ec);
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  result = asio::connect(socket, endpoints, true_cond_2(), ec);
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  result = asio::connect(socket, endpoints, legacy_true_cond_1, ec);
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  result = asio::connect(socket, endpoints, legacy_true_cond_2(), ec);
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  result = asio::connect(socket, endpoints, false_cond, ec);
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);

  endpoints.insert(endpoints.begin(), asio::ip::tcp::endpoint());

  result = asio::connect(socket, endpoints, true_cond_1, ec);
  ASIO_CHECK(result == endpoints[1]);
  ASIO_CHECK(!ec);

  result = asio::connect(socket, endpoints, true_cond_2(), ec);
  ASIO_CHECK(result == endpoints[1]);
  ASIO_CHECK(!ec);

  result = asio::connect(socket, endpoints, legacy_true_cond_1, ec);
  ASIO_CHECK(result == endpoints[1]);
  ASIO_CHECK(!ec);

  result = asio::connect(socket, endpoints, legacy_true_cond_2(), ec);
  ASIO_CHECK(result == endpoints[1]);
  ASIO_CHECK(!ec);

  result = asio::connect(socket, endpoints, false_cond, ec);
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);
}

void test_connect_iter()
{
  connection_sink sink;
  asio::io_context io_context;
  asio::ip::tcp::socket socket(io_context);
  std::vector<asio::ip::tcp::endpoint> endpoints;
  const std::vector<asio::ip::tcp::endpoint>& cendpoints = endpoints;
  std::vector<asio::ip::tcp::endpoint>::const_iterator result;

  try
  {
    result = asio::connect(socket, cendpoints.begin(), cendpoints.end());
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }

  endpoints.push_back(sink.target_endpoint());

  result = asio::connect(socket, cendpoints.begin(), cendpoints.end());
  ASIO_CHECK(result == cendpoints.begin());

  endpoints.push_back(sink.target_endpoint());

  result = asio::connect(socket, cendpoints.begin(), cendpoints.end());
  ASIO_CHECK(result == cendpoints.begin());

  endpoints.insert(endpoints.begin(), asio::ip::tcp::endpoint());

  result = asio::connect(socket, cendpoints.begin(), cendpoints.end());
  ASIO_CHECK(result == cendpoints.begin() + 1);
}

void test_connect_iter_ec()
{
  connection_sink sink;
  asio::io_context io_context;
  asio::ip::tcp::socket socket(io_context);
  std::vector<asio::ip::tcp::endpoint> endpoints;
  const std::vector<asio::ip::tcp::endpoint>& cendpoints = endpoints;
  std::vector<asio::ip::tcp::endpoint>::const_iterator result;
  asio::error_code ec;

  result = asio::connect(socket,
      cendpoints.begin(), cendpoints.end(), ec);
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  result = asio::connect(socket,
      cendpoints.begin(), cendpoints.end(), ec);
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  endpoints.push_back(sink.target_endpoint());

  result = asio::connect(socket,
      cendpoints.begin(), cendpoints.end(), ec);
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  endpoints.insert(endpoints.begin(), asio::ip::tcp::endpoint());

  result = asio::connect(socket,
      cendpoints.begin(), cendpoints.end(), ec);
  ASIO_CHECK(result == cendpoints.begin() + 1);
  ASIO_CHECK(!ec);
}

void test_connect_iter_cond()
{
  connection_sink sink;
  asio::io_context io_context;
  asio::ip::tcp::socket socket(io_context);
  std::vector<asio::ip::tcp::endpoint> endpoints;
  const std::vector<asio::ip::tcp::endpoint>& cendpoints = endpoints;
  std::vector<asio::ip::tcp::endpoint>::const_iterator result;

  try
  {
    result = asio::connect(socket, cendpoints.begin(),
        cendpoints.end(), true_cond_1);
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }

  try
  {
    result = asio::connect(socket, cendpoints.begin(),
        cendpoints.end(), true_cond_2());
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }

  try
  {
    result = asio::connect(socket, cendpoints.begin(),
        cendpoints.end(), legacy_true_cond_1);
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }

  try
  {
    result = asio::connect(socket, cendpoints.begin(),
        cendpoints.end(), legacy_true_cond_2());
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }

  try
  {
    result = asio::connect(socket, cendpoints.begin(),
        cendpoints.end(), false_cond);
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }

  endpoints.push_back(sink.target_endpoint());

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_1);
  ASIO_CHECK(result == cendpoints.begin());

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_2());
  ASIO_CHECK(result == cendpoints.begin());

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_1);
  ASIO_CHECK(result == cendpoints.begin());

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_2());
  ASIO_CHECK(result == cendpoints.begin());

  try
  {
    result = asio::connect(socket, cendpoints.begin(),
        cendpoints.end(), false_cond);
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }

  endpoints.push_back(sink.target_endpoint());

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_1);
  ASIO_CHECK(result == cendpoints.begin());

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_2());
  ASIO_CHECK(result == cendpoints.begin());

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_1);
  ASIO_CHECK(result == cendpoints.begin());

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_2());
  ASIO_CHECK(result == cendpoints.begin());

  try
  {
    result = asio::connect(socket, cendpoints.begin(),
        cendpoints.end(), false_cond);
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }

  endpoints.insert(endpoints.begin(), asio::ip::tcp::endpoint());

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_1);
  ASIO_CHECK(result == cendpoints.begin() + 1);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_2());
  ASIO_CHECK(result == cendpoints.begin() + 1);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_1);
  ASIO_CHECK(result == cendpoints.begin() + 1);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_2());
  ASIO_CHECK(result == cendpoints.begin() + 1);

  try
  {
    result = asio::connect(socket, cendpoints.begin(),
        cendpoints.end(), false_cond);
    ASIO_CHECK(false);
  }
  catch (asio::system_error& e)
  {
    ASIO_CHECK(e.code() == asio::error::not_found);
  }
}

void test_connect_iter_cond_ec()
{
  connection_sink sink;
  asio::io_context io_context;
  asio::ip::tcp::socket socket(io_context);
  std::vector<asio::ip::tcp::endpoint> endpoints;
  const std::vector<asio::ip::tcp::endpoint>& cendpoints = endpoints;
  std::vector<asio::ip::tcp::endpoint>::const_iterator result;
  asio::error_code ec;

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_1, ec);
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_2(), ec);
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_1, ec);
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_2(), ec);
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), false_cond, ec);
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_1, ec);
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_2(), ec);
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_1, ec);
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_2(), ec);
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), false_cond, ec);
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_1, ec);
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_2(), ec);
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_1, ec);
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_2(), ec);
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), false_cond, ec);
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);

  endpoints.insert(endpoints.begin(), asio::ip::tcp::endpoint());

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_1, ec);
  ASIO_CHECK(result == cendpoints.begin() + 1);
  ASIO_CHECK(!ec);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_2(), ec);
  ASIO_CHECK(result == cendpoints.begin() + 1);
  ASIO_CHECK(!ec);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_1, ec);
  ASIO_CHECK(result == cendpoints.begin() + 1);
  ASIO_CHECK(!ec);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_2(), ec);
  ASIO_CHECK(result == cendpoints.begin() + 1);
  ASIO_CHECK(!ec);

  result = asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), false_cond, ec);
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);
}

void test_async_connect_range()
{
  connection_sink sink;
  asio::io_context io_context;
  asio::ip::tcp::socket socket(io_context);
  std::vector<asio::ip::tcp::endpoint> endpoints;
  asio::ip::tcp::endpoint result;
  asio::error_code ec;

  asio::async_connect(socket, endpoints,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  asio::async_connect(socket, endpoints,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  endpoints.push_back(sink.target_endpoint());

  asio::async_connect(socket, endpoints,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  endpoints.insert(endpoints.begin(), asio::ip::tcp::endpoint());

  asio::async_connect(socket, endpoints,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == endpoints[1]);
  ASIO_CHECK(!ec);
}

void test_async_connect_range_cond()
{
  connection_sink sink;
  asio::io_context io_context;
  asio::ip::tcp::socket socket(io_context);
  std::vector<asio::ip::tcp::endpoint> endpoints;
  asio::ip::tcp::endpoint result;
  asio::error_code ec;

  asio::async_connect(socket, endpoints, true_cond_1,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);

  asio::async_connect(socket, endpoints, true_cond_2(),
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);

  asio::async_connect(socket, endpoints, legacy_true_cond_1,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);

  asio::async_connect(socket, endpoints, legacy_true_cond_2(),
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);

  asio::async_connect(socket, endpoints, false_cond,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  asio::async_connect(socket, endpoints, true_cond_1,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  asio::async_connect(socket, endpoints, true_cond_2(),
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  asio::async_connect(socket, endpoints, legacy_true_cond_1,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  asio::async_connect(socket, endpoints, legacy_true_cond_2(),
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  asio::async_connect(socket, endpoints, false_cond,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  asio::async_connect(socket, endpoints, true_cond_1,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  asio::async_connect(socket, endpoints, true_cond_2(),
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  asio::async_connect(socket, endpoints, legacy_true_cond_1,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  asio::async_connect(socket, endpoints, legacy_true_cond_2(),
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == endpoints[0]);
  ASIO_CHECK(!ec);

  asio::async_connect(socket, endpoints, false_cond,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);

  endpoints.insert(endpoints.begin(), asio::ip::tcp::endpoint());

  asio::async_connect(socket, endpoints, true_cond_1,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == endpoints[1]);
  ASIO_CHECK(!ec);

  asio::async_connect(socket, endpoints, true_cond_2(),
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == endpoints[1]);
  ASIO_CHECK(!ec);

  asio::async_connect(socket, endpoints, legacy_true_cond_1,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == endpoints[1]);
  ASIO_CHECK(!ec);

  asio::async_connect(socket, endpoints, legacy_true_cond_2(),
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == endpoints[1]);
  ASIO_CHECK(!ec);

  asio::async_connect(socket, endpoints, false_cond,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == asio::ip::tcp::endpoint());
  ASIO_CHECK(ec == asio::error::not_found);
}

void test_async_connect_iter()
{
  connection_sink sink;
  asio::io_context io_context;
  asio::ip::tcp::socket socket(io_context);
  std::vector<asio::ip::tcp::endpoint> endpoints;
  const std::vector<asio::ip::tcp::endpoint>& cendpoints = endpoints;
  std::vector<asio::ip::tcp::endpoint>::const_iterator result;
  asio::error_code ec;

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  endpoints.push_back(sink.target_endpoint());

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  endpoints.insert(endpoints.begin(), asio::ip::tcp::endpoint());

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.begin() + 1);
  ASIO_CHECK(!ec);
}

void test_async_connect_iter_cond()
{
  connection_sink sink;
  asio::io_context io_context;
  asio::ip::tcp::socket socket(io_context);
  std::vector<asio::ip::tcp::endpoint> endpoints;
  const std::vector<asio::ip::tcp::endpoint>& cendpoints = endpoints;
  std::vector<asio::ip::tcp::endpoint>::const_iterator result;
  asio::error_code ec;

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      true_cond_1, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      true_cond_2(), bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      legacy_true_cond_1, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      legacy_true_cond_2(), bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      false_cond, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      true_cond_1, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      true_cond_2(), bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      legacy_true_cond_1, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      legacy_true_cond_2(), bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      false_cond, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      true_cond_1, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      true_cond_2(), bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      legacy_true_cond_1, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      legacy_true_cond_2(), bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.begin());
  ASIO_CHECK(!ec);

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      false_cond, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);

  endpoints.insert(endpoints.begin(), asio::ip::tcp::endpoint());

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      true_cond_1, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.begin() + 1);
  ASIO_CHECK(!ec);

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      true_cond_2(), bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.begin() + 1);
  ASIO_CHECK(!ec);

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      legacy_true_cond_1, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.begin() + 1);
  ASIO_CHECK(!ec);

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      legacy_true_cond_2(), bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.begin() + 1);
  ASIO_CHECK(!ec);

  asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      false_cond, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  ASIO_CHECK(result == cendpoints.end());
  ASIO_CHECK(ec == asio::error::not_found);
}

ASIO_TEST_SUITE
(
  "connect",
  ASIO_TEST_CASE(test_connect_range)
  ASIO_TEST_CASE(test_connect_range_ec)
  ASIO_TEST_CASE(test_connect_range_cond)
  ASIO_TEST_CASE(test_connect_range_cond_ec)
  ASIO_TEST_CASE(test_connect_iter)
  ASIO_TEST_CASE(test_connect_iter_ec)
  ASIO_TEST_CASE(test_connect_iter_cond)
  ASIO_TEST_CASE(test_connect_iter_cond_ec)
  ASIO_TEST_CASE(test_async_connect_range)
  ASIO_TEST_CASE(test_async_connect_range_cond)
  ASIO_TEST_CASE(test_async_connect_iter)
  ASIO_TEST_CASE(test_async_connect_iter_cond)
)
