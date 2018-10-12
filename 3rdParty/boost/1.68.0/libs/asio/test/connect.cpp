//
// connect.cpp
// ~~~~~~~~~~~
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
#include <boost/asio/connect.hpp>

#include <vector>
#include <boost/asio/detail/thread.hpp>
#include <boost/asio/ip/tcp.hpp>

#if defined(BOOST_ASIO_HAS_BOOST_BIND)
# include <boost/bind.hpp>
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
# include <functional>
#endif // defined(BOOST_ASIO_HAS_BOOST_BIND)

#include "unit_test.hpp"

#if defined(BOOST_ASIO_HAS_BOOST_BIND)
namespace bindns = boost;
#else // defined(BOOST_ASIO_HAS_BOOST_BIND)
namespace bindns = std;
using std::placeholders::_1;
using std::placeholders::_2;
#endif // defined(BOOST_ASIO_HAS_BOOST_BIND)

class connection_sink
{
public:
  connection_sink()
    : acceptor_(io_context_,
        boost::asio::ip::tcp::endpoint(
          boost::asio::ip::address_v4::loopback(), 0)),
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

  boost::asio::ip::tcp::endpoint target_endpoint()
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

  boost::asio::io_context io_context_;
  boost::asio::ip::tcp::acceptor acceptor_;
  boost::asio::ip::tcp::endpoint target_endpoint_;
  boost::asio::ip::tcp::socket socket_;
  boost::asio::detail::thread thread_;
};

bool true_cond_1(const boost::system::error_code& /*ec*/,
    const boost::asio::ip::tcp::endpoint& /*endpoint*/)
{
  return true;
}

struct true_cond_2
{
  template <typename Endpoint>
  bool operator()(const boost::system::error_code& /*ec*/,
      const Endpoint& /*endpoint*/)
  {
    return true;
  }
};

std::vector<boost::asio::ip::tcp::endpoint>::const_iterator legacy_true_cond_1(
    const boost::system::error_code& /*ec*/,
    std::vector<boost::asio::ip::tcp::endpoint>::const_iterator next)
{
  return next;
}

struct legacy_true_cond_2
{
  template <typename Iterator>
  Iterator operator()(const boost::system::error_code& /*ec*/, Iterator next)
  {
    return next;
  }
};

bool false_cond(const boost::system::error_code& /*ec*/,
    const boost::asio::ip::tcp::endpoint& /*endpoint*/)
{
  return false;
}

void range_handler(const boost::system::error_code& ec,
    const boost::asio::ip::tcp::endpoint& endpoint,
    boost::system::error_code* out_ec,
    boost::asio::ip::tcp::endpoint* out_endpoint)
{
  *out_ec = ec;
  *out_endpoint = endpoint;
}

void iter_handler(const boost::system::error_code& ec,
    std::vector<boost::asio::ip::tcp::endpoint>::const_iterator iter,
    boost::system::error_code* out_ec,
    std::vector<boost::asio::ip::tcp::endpoint>::const_iterator* out_iter)
{
  *out_ec = ec;
  *out_iter = iter;
}

void test_connect_range()
{
  connection_sink sink;
  boost::asio::io_context io_context;
  boost::asio::ip::tcp::socket socket(io_context);
  std::vector<boost::asio::ip::tcp::endpoint> endpoints;
  boost::asio::ip::tcp::endpoint result;

  try
  {
    result = boost::asio::connect(socket, endpoints);
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }

  endpoints.push_back(sink.target_endpoint());

  result = boost::asio::connect(socket, endpoints);
  BOOST_ASIO_CHECK(result == endpoints[0]);

  endpoints.push_back(sink.target_endpoint());

  result = boost::asio::connect(socket, endpoints);
  BOOST_ASIO_CHECK(result == endpoints[0]);

  endpoints.insert(endpoints.begin(), boost::asio::ip::tcp::endpoint());

  result = boost::asio::connect(socket, endpoints);
  BOOST_ASIO_CHECK(result == endpoints[1]);
}

void test_connect_range_ec()
{
  connection_sink sink;
  boost::asio::io_context io_context;
  boost::asio::ip::tcp::socket socket(io_context);
  std::vector<boost::asio::ip::tcp::endpoint> endpoints;
  boost::asio::ip::tcp::endpoint result;
  boost::system::error_code ec;

  result = boost::asio::connect(socket, endpoints, ec);
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  result = boost::asio::connect(socket, endpoints, ec);
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  endpoints.push_back(sink.target_endpoint());

  result = boost::asio::connect(socket, endpoints, ec);
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  endpoints.insert(endpoints.begin(), boost::asio::ip::tcp::endpoint());

  result = boost::asio::connect(socket, endpoints, ec);
  BOOST_ASIO_CHECK(result == endpoints[1]);
  BOOST_ASIO_CHECK(!ec);
}

void test_connect_range_cond()
{
  connection_sink sink;
  boost::asio::io_context io_context;
  boost::asio::ip::tcp::socket socket(io_context);
  std::vector<boost::asio::ip::tcp::endpoint> endpoints;
  boost::asio::ip::tcp::endpoint result;

  try
  {
    result = boost::asio::connect(socket, endpoints, true_cond_1);
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }

  try
  {
    result = boost::asio::connect(socket, endpoints, true_cond_2());
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }

  try
  {
    result = boost::asio::connect(socket, endpoints, legacy_true_cond_1);
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }

  try
  {
    result = boost::asio::connect(socket, endpoints, legacy_true_cond_2());
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }

  try
  {
    result = boost::asio::connect(socket, endpoints, false_cond);
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }

  endpoints.push_back(sink.target_endpoint());

  result = boost::asio::connect(socket, endpoints, true_cond_1);
  BOOST_ASIO_CHECK(result == endpoints[0]);

  result = boost::asio::connect(socket, endpoints, true_cond_2());
  BOOST_ASIO_CHECK(result == endpoints[0]);

  result = boost::asio::connect(socket, endpoints, legacy_true_cond_1);
  BOOST_ASIO_CHECK(result == endpoints[0]);

  result = boost::asio::connect(socket, endpoints, legacy_true_cond_2());
  BOOST_ASIO_CHECK(result == endpoints[0]);

  try
  {
    result = boost::asio::connect(socket, endpoints, false_cond);
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }

  endpoints.push_back(sink.target_endpoint());

  result = boost::asio::connect(socket, endpoints, true_cond_1);
  BOOST_ASIO_CHECK(result == endpoints[0]);

  result = boost::asio::connect(socket, endpoints, true_cond_2());
  BOOST_ASIO_CHECK(result == endpoints[0]);

  result = boost::asio::connect(socket, endpoints, legacy_true_cond_1);
  BOOST_ASIO_CHECK(result == endpoints[0]);

  result = boost::asio::connect(socket, endpoints, legacy_true_cond_2());
  BOOST_ASIO_CHECK(result == endpoints[0]);

  try
  {
    result = boost::asio::connect(socket, endpoints, false_cond);
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }

  endpoints.insert(endpoints.begin(), boost::asio::ip::tcp::endpoint());

  result = boost::asio::connect(socket, endpoints, true_cond_1);
  BOOST_ASIO_CHECK(result == endpoints[1]);

  result = boost::asio::connect(socket, endpoints, true_cond_2());
  BOOST_ASIO_CHECK(result == endpoints[1]);

  result = boost::asio::connect(socket, endpoints, legacy_true_cond_1);
  BOOST_ASIO_CHECK(result == endpoints[1]);

  result = boost::asio::connect(socket, endpoints, legacy_true_cond_2());
  BOOST_ASIO_CHECK(result == endpoints[1]);

  try
  {
    result = boost::asio::connect(socket, endpoints, false_cond);
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }
}

void test_connect_range_cond_ec()
{
  connection_sink sink;
  boost::asio::io_context io_context;
  boost::asio::ip::tcp::socket socket(io_context);
  std::vector<boost::asio::ip::tcp::endpoint> endpoints;
  boost::asio::ip::tcp::endpoint result;
  boost::system::error_code ec;

  result = boost::asio::connect(socket, endpoints, true_cond_1, ec);
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  result = boost::asio::connect(socket, endpoints, true_cond_2(), ec);
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  result = boost::asio::connect(socket, endpoints, legacy_true_cond_1, ec);
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  result = boost::asio::connect(socket, endpoints, legacy_true_cond_2(), ec);
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  result = boost::asio::connect(socket, endpoints, false_cond, ec);
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  result = boost::asio::connect(socket, endpoints, true_cond_1, ec);
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, endpoints, true_cond_2(), ec);
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, endpoints, legacy_true_cond_1, ec);
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, endpoints, legacy_true_cond_2(), ec);
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, endpoints, false_cond, ec);
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  result = boost::asio::connect(socket, endpoints, true_cond_1, ec);
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, endpoints, true_cond_2(), ec);
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, endpoints, legacy_true_cond_1, ec);
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, endpoints, legacy_true_cond_2(), ec);
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, endpoints, false_cond, ec);
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  endpoints.insert(endpoints.begin(), boost::asio::ip::tcp::endpoint());

  result = boost::asio::connect(socket, endpoints, true_cond_1, ec);
  BOOST_ASIO_CHECK(result == endpoints[1]);
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, endpoints, true_cond_2(), ec);
  BOOST_ASIO_CHECK(result == endpoints[1]);
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, endpoints, legacy_true_cond_1, ec);
  BOOST_ASIO_CHECK(result == endpoints[1]);
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, endpoints, legacy_true_cond_2(), ec);
  BOOST_ASIO_CHECK(result == endpoints[1]);
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, endpoints, false_cond, ec);
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
}

void test_connect_iter()
{
  connection_sink sink;
  boost::asio::io_context io_context;
  boost::asio::ip::tcp::socket socket(io_context);
  std::vector<boost::asio::ip::tcp::endpoint> endpoints;
  const std::vector<boost::asio::ip::tcp::endpoint>& cendpoints = endpoints;
  std::vector<boost::asio::ip::tcp::endpoint>::const_iterator result;

  try
  {
    result = boost::asio::connect(socket, cendpoints.begin(), cendpoints.end());
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }

  endpoints.push_back(sink.target_endpoint());

  result = boost::asio::connect(socket, cendpoints.begin(), cendpoints.end());
  BOOST_ASIO_CHECK(result == cendpoints.begin());

  endpoints.push_back(sink.target_endpoint());

  result = boost::asio::connect(socket, cendpoints.begin(), cendpoints.end());
  BOOST_ASIO_CHECK(result == cendpoints.begin());

  endpoints.insert(endpoints.begin(), boost::asio::ip::tcp::endpoint());

  result = boost::asio::connect(socket, cendpoints.begin(), cendpoints.end());
  BOOST_ASIO_CHECK(result == cendpoints.begin() + 1);
}

void test_connect_iter_ec()
{
  connection_sink sink;
  boost::asio::io_context io_context;
  boost::asio::ip::tcp::socket socket(io_context);
  std::vector<boost::asio::ip::tcp::endpoint> endpoints;
  const std::vector<boost::asio::ip::tcp::endpoint>& cendpoints = endpoints;
  std::vector<boost::asio::ip::tcp::endpoint>::const_iterator result;
  boost::system::error_code ec;

  result = boost::asio::connect(socket,
      cendpoints.begin(), cendpoints.end(), ec);
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  result = boost::asio::connect(socket,
      cendpoints.begin(), cendpoints.end(), ec);
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  endpoints.push_back(sink.target_endpoint());

  result = boost::asio::connect(socket,
      cendpoints.begin(), cendpoints.end(), ec);
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  endpoints.insert(endpoints.begin(), boost::asio::ip::tcp::endpoint());

  result = boost::asio::connect(socket,
      cendpoints.begin(), cendpoints.end(), ec);
  BOOST_ASIO_CHECK(result == cendpoints.begin() + 1);
  BOOST_ASIO_CHECK(!ec);
}

void test_connect_iter_cond()
{
  connection_sink sink;
  boost::asio::io_context io_context;
  boost::asio::ip::tcp::socket socket(io_context);
  std::vector<boost::asio::ip::tcp::endpoint> endpoints;
  const std::vector<boost::asio::ip::tcp::endpoint>& cendpoints = endpoints;
  std::vector<boost::asio::ip::tcp::endpoint>::const_iterator result;

  try
  {
    result = boost::asio::connect(socket, cendpoints.begin(),
        cendpoints.end(), true_cond_1);
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }

  try
  {
    result = boost::asio::connect(socket, cendpoints.begin(),
        cendpoints.end(), true_cond_2());
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }

  try
  {
    result = boost::asio::connect(socket, cendpoints.begin(),
        cendpoints.end(), legacy_true_cond_1);
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }

  try
  {
    result = boost::asio::connect(socket, cendpoints.begin(),
        cendpoints.end(), legacy_true_cond_2());
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }

  try
  {
    result = boost::asio::connect(socket, cendpoints.begin(),
        cendpoints.end(), false_cond);
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }

  endpoints.push_back(sink.target_endpoint());

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_1);
  BOOST_ASIO_CHECK(result == cendpoints.begin());

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_2());
  BOOST_ASIO_CHECK(result == cendpoints.begin());

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_1);
  BOOST_ASIO_CHECK(result == cendpoints.begin());

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_2());
  BOOST_ASIO_CHECK(result == cendpoints.begin());

  try
  {
    result = boost::asio::connect(socket, cendpoints.begin(),
        cendpoints.end(), false_cond);
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }

  endpoints.push_back(sink.target_endpoint());

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_1);
  BOOST_ASIO_CHECK(result == cendpoints.begin());

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_2());
  BOOST_ASIO_CHECK(result == cendpoints.begin());

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_1);
  BOOST_ASIO_CHECK(result == cendpoints.begin());

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_2());
  BOOST_ASIO_CHECK(result == cendpoints.begin());

  try
  {
    result = boost::asio::connect(socket, cendpoints.begin(),
        cendpoints.end(), false_cond);
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }

  endpoints.insert(endpoints.begin(), boost::asio::ip::tcp::endpoint());

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_1);
  BOOST_ASIO_CHECK(result == cendpoints.begin() + 1);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_2());
  BOOST_ASIO_CHECK(result == cendpoints.begin() + 1);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_1);
  BOOST_ASIO_CHECK(result == cendpoints.begin() + 1);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_2());
  BOOST_ASIO_CHECK(result == cendpoints.begin() + 1);

  try
  {
    result = boost::asio::connect(socket, cendpoints.begin(),
        cendpoints.end(), false_cond);
    BOOST_ASIO_CHECK(false);
  }
  catch (boost::system::system_error& e)
  {
    BOOST_ASIO_CHECK(e.code() == boost::asio::error::not_found);
  }
}

void test_connect_iter_cond_ec()
{
  connection_sink sink;
  boost::asio::io_context io_context;
  boost::asio::ip::tcp::socket socket(io_context);
  std::vector<boost::asio::ip::tcp::endpoint> endpoints;
  const std::vector<boost::asio::ip::tcp::endpoint>& cendpoints = endpoints;
  std::vector<boost::asio::ip::tcp::endpoint>::const_iterator result;
  boost::system::error_code ec;

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_1, ec);
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_2(), ec);
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_1, ec);
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_2(), ec);
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), false_cond, ec);
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_1, ec);
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_2(), ec);
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_1, ec);
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_2(), ec);
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), false_cond, ec);
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_1, ec);
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_2(), ec);
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_1, ec);
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_2(), ec);
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), false_cond, ec);
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  endpoints.insert(endpoints.begin(), boost::asio::ip::tcp::endpoint());

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_1, ec);
  BOOST_ASIO_CHECK(result == cendpoints.begin() + 1);
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), true_cond_2(), ec);
  BOOST_ASIO_CHECK(result == cendpoints.begin() + 1);
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_1, ec);
  BOOST_ASIO_CHECK(result == cendpoints.begin() + 1);
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), legacy_true_cond_2(), ec);
  BOOST_ASIO_CHECK(result == cendpoints.begin() + 1);
  BOOST_ASIO_CHECK(!ec);

  result = boost::asio::connect(socket, cendpoints.begin(),
      cendpoints.end(), false_cond, ec);
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
}

void test_async_connect_range()
{
  connection_sink sink;
  boost::asio::io_context io_context;
  boost::asio::ip::tcp::socket socket(io_context);
  std::vector<boost::asio::ip::tcp::endpoint> endpoints;
  boost::asio::ip::tcp::endpoint result;
  boost::system::error_code ec;

  boost::asio::async_connect(socket, endpoints,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  boost::asio::async_connect(socket, endpoints,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  endpoints.push_back(sink.target_endpoint());

  boost::asio::async_connect(socket, endpoints,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  endpoints.insert(endpoints.begin(), boost::asio::ip::tcp::endpoint());

  boost::asio::async_connect(socket, endpoints,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == endpoints[1]);
  BOOST_ASIO_CHECK(!ec);
}

void test_async_connect_range_cond()
{
  connection_sink sink;
  boost::asio::io_context io_context;
  boost::asio::ip::tcp::socket socket(io_context);
  std::vector<boost::asio::ip::tcp::endpoint> endpoints;
  boost::asio::ip::tcp::endpoint result;
  boost::system::error_code ec;

  boost::asio::async_connect(socket, endpoints, true_cond_1,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  boost::asio::async_connect(socket, endpoints, true_cond_2(),
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  boost::asio::async_connect(socket, endpoints, legacy_true_cond_1,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  boost::asio::async_connect(socket, endpoints, legacy_true_cond_2(),
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  boost::asio::async_connect(socket, endpoints, false_cond,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  boost::asio::async_connect(socket, endpoints, true_cond_1,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, endpoints, true_cond_2(),
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, endpoints, legacy_true_cond_1,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, endpoints, legacy_true_cond_2(),
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, endpoints, false_cond,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  boost::asio::async_connect(socket, endpoints, true_cond_1,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, endpoints, true_cond_2(),
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, endpoints, legacy_true_cond_1,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, endpoints, legacy_true_cond_2(),
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == endpoints[0]);
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, endpoints, false_cond,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  endpoints.insert(endpoints.begin(), boost::asio::ip::tcp::endpoint());

  boost::asio::async_connect(socket, endpoints, true_cond_1,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == endpoints[1]);
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, endpoints, true_cond_2(),
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == endpoints[1]);
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, endpoints, legacy_true_cond_1,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == endpoints[1]);
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, endpoints, legacy_true_cond_2(),
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == endpoints[1]);
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, endpoints, false_cond,
      bindns::bind(range_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == boost::asio::ip::tcp::endpoint());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
}

void test_async_connect_iter()
{
  connection_sink sink;
  boost::asio::io_context io_context;
  boost::asio::ip::tcp::socket socket(io_context);
  std::vector<boost::asio::ip::tcp::endpoint> endpoints;
  const std::vector<boost::asio::ip::tcp::endpoint>& cendpoints = endpoints;
  std::vector<boost::asio::ip::tcp::endpoint>::const_iterator result;
  boost::system::error_code ec;

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  endpoints.push_back(sink.target_endpoint());

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  endpoints.insert(endpoints.begin(), boost::asio::ip::tcp::endpoint());

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.begin() + 1);
  BOOST_ASIO_CHECK(!ec);
}

void test_async_connect_iter_cond()
{
  connection_sink sink;
  boost::asio::io_context io_context;
  boost::asio::ip::tcp::socket socket(io_context);
  std::vector<boost::asio::ip::tcp::endpoint> endpoints;
  const std::vector<boost::asio::ip::tcp::endpoint>& cendpoints = endpoints;
  std::vector<boost::asio::ip::tcp::endpoint>::const_iterator result;
  boost::system::error_code ec;

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      true_cond_1, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      true_cond_2(), bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      legacy_true_cond_1, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      legacy_true_cond_2(), bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      false_cond, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      true_cond_1, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      true_cond_2(), bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      legacy_true_cond_1, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      legacy_true_cond_2(), bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      false_cond, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  endpoints.push_back(sink.target_endpoint());

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      true_cond_1, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      true_cond_2(), bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      legacy_true_cond_1, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      legacy_true_cond_2(), bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.begin());
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      false_cond, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);

  endpoints.insert(endpoints.begin(), boost::asio::ip::tcp::endpoint());

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      true_cond_1, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.begin() + 1);
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      true_cond_2(), bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.begin() + 1);
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      legacy_true_cond_1, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.begin() + 1);
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      legacy_true_cond_2(), bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.begin() + 1);
  BOOST_ASIO_CHECK(!ec);

  boost::asio::async_connect(socket, cendpoints.begin(), cendpoints.end(),
      false_cond, bindns::bind(iter_handler, _1, _2, &ec, &result));
  io_context.restart();
  io_context.run();
  BOOST_ASIO_CHECK(result == cendpoints.end());
  BOOST_ASIO_CHECK(ec == boost::asio::error::not_found);
}

BOOST_ASIO_TEST_SUITE
(
  "connect",
  BOOST_ASIO_TEST_CASE(test_connect_range)
  BOOST_ASIO_TEST_CASE(test_connect_range_ec)
  BOOST_ASIO_TEST_CASE(test_connect_range_cond)
  BOOST_ASIO_TEST_CASE(test_connect_range_cond_ec)
  BOOST_ASIO_TEST_CASE(test_connect_iter)
  BOOST_ASIO_TEST_CASE(test_connect_iter_ec)
  BOOST_ASIO_TEST_CASE(test_connect_iter_cond)
  BOOST_ASIO_TEST_CASE(test_connect_iter_cond_ec)
  BOOST_ASIO_TEST_CASE(test_async_connect_range)
  BOOST_ASIO_TEST_CASE(test_async_connect_range_cond)
  BOOST_ASIO_TEST_CASE(test_async_connect_iter)
  BOOST_ASIO_TEST_CASE(test_async_connect_iter_cond)
)
