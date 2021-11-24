//
// network_v4.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (c) 2014 Oliver Kowalke (oliver dot kowalke at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Disable autolinking for unit tests.
#if !defined(BOOST_ALL_NO_LIB)
#define BOOST_ALL_NO_LIB 1
#endif // !defined(BOOST_ALL_NO_LIB)

// Test that header file is self-contained.
#include <boost/asio/ip/network_v4.hpp>

#include "../unit_test.hpp"
#include <sstream>

//------------------------------------------------------------------------------

// ip_network_v4_compile test
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks that all public member functions on the class
// ip::network_v4 compile and link correctly. Runtime failures are ignored.

namespace ip_network_v4_compile {

void test()
{
  using namespace boost::asio;
  namespace ip = boost::asio::ip;

  try
  {
    boost::system::error_code ec;

    // network_v4 constructors.

    ip::network_v4 net1(ip::make_address_v4("192.168.1.0"), 32);
    ip::network_v4 net2(ip::make_address_v4("192.168.1.0"),
        ip::make_address_v4("255.255.255.0"));

    // network_v4 functions.

    ip::address_v4 addr1 = net1.address();
    (void)addr1;

    unsigned short prefix_len = net1.prefix_length();
    (void)prefix_len;

    ip::address_v4 addr2 = net1.netmask();
    (void)addr2;

    ip::address_v4 addr3 = net1.network();
    (void)addr3;

    ip::address_v4 addr4 = net1.broadcast();
    (void)addr4;

    ip::address_v4_range hosts = net1.hosts();
    (void)hosts;

    ip::network_v4 net3 = net1.canonical();
    (void)net3;

    bool b1 = net1.is_host();
    (void)b1;

    bool b2 = net1.is_subnet_of(net2);
    (void)b2;

    std::string s1 = net1.to_string();
    (void)s1;

    std::string s2 = net1.to_string(ec);
    (void)s2;

    // network_v4 comparisons.

    bool b3 = (net1 == net2);
    (void)b3;

    bool b4 = (net1 != net2);
    (void)b4;

    // network_v4 creation functions.

    net1 = ip::make_network_v4(ip::address_v4(), 24);
    net1 = ip::make_network_v4(ip::address_v4(), ip::address_v4());
    net1 = ip::make_network_v4("10.0.0.0/8");
    net1 = ip::make_network_v4("10.0.0.0/8", ec);
    net1 = ip::make_network_v4(s1);
    net1 = ip::make_network_v4(s1, ec);
#if defined(BOOST_ASIO_HAS_STRING_VIEW)
# if defined(BOOST_ASIO_HAS_STD_STRING_VIEW)
    std::string_view string_view_value("10.0.0.0/8");
# elif defined(BOOST_ASIO_HAS_STD_EXPERIMENTAL_STRING_VIEW)
    std::experimental::string_view string_view_value("10.0.0.0/8");
# endif // defined(BOOST_ASIO_HAS_STD_EXPERIMENTAL_STRING_VIEW)
    net1 = ip::make_network_v4(string_view_value);
    net1 = ip::make_network_v4(string_view_value, ec);
#endif // defined(BOOST_ASIO_HAS_STRING_VIEW)

    // network_v4 I/O.

    std::ostringstream os;
    os << net1;

#if !defined(BOOST_NO_STD_WSTREAMBUF)
    std::wostringstream wos;
    wos << net1;
#endif // !defined(BOOST_NO_STD_WSTREAMBUF)
  }
  catch (std::exception&)
  {
  }
}

} // namespace ip_network_v4_compile

//------------------------------------------------------------------------------

// ip_network_v4_runtime test
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks that the various public member functions meet the
// necessary postconditions.

namespace ip_network_v4_runtime {

void test()
{
  using boost::asio::ip::address_v4;
  using boost::asio::ip::make_address_v4;
  using boost::asio::ip::network_v4;
  using boost::asio::ip::make_network_v4;

  address_v4 addr = make_address_v4("1.2.3.4");

  // calculate prefix length

  network_v4 net1(addr, make_address_v4("255.255.255.0"));
  BOOST_ASIO_CHECK(net1.prefix_length() == 24);

  network_v4 net2(addr, make_address_v4("255.255.255.192"));
  BOOST_ASIO_CHECK(net2.prefix_length() == 26);

  network_v4 net3(addr, make_address_v4("128.0.0.0"));
  BOOST_ASIO_CHECK(net3.prefix_length() == 1);

  std::string msg;
  try
  {
    make_network_v4(addr, make_address_v4("255.255.255.1"));
  }
  catch(std::exception& ex)
  {
    msg = ex.what();
  }
  BOOST_ASIO_CHECK(msg == std::string("non-contiguous netmask"));

  msg.clear();
  try
  {
    make_network_v4(addr, make_address_v4("0.255.255.0"));
  }
  catch(std::exception& ex)
  {
    msg = ex.what();
  }
  BOOST_ASIO_CHECK(msg == std::string("non-contiguous netmask"));

  // calculate netmask

  network_v4 net4(addr, 23);
  BOOST_ASIO_CHECK(net4.netmask() == make_address_v4("255.255.254.0"));

  network_v4 net5(addr, 12);
  BOOST_ASIO_CHECK(net5.netmask() == make_address_v4("255.240.0.0"));

  network_v4 net6(addr, 24);
  BOOST_ASIO_CHECK(net6.netmask() == make_address_v4("255.255.255.0"));

  network_v4 net7(addr, 16);
  BOOST_ASIO_CHECK(net7.netmask() == make_address_v4("255.255.0.0"));

  network_v4 net8(addr, 8);
  BOOST_ASIO_CHECK(net8.netmask() == make_address_v4("255.0.0.0"));

  network_v4 net9(addr, 32);
  BOOST_ASIO_CHECK(net9.netmask() == make_address_v4("255.255.255.255"));

  network_v4 net10(addr, 1);
  BOOST_ASIO_CHECK(net10.netmask() == make_address_v4("128.0.0.0"));

  network_v4 net11(addr, 0);
  BOOST_ASIO_CHECK(net11.netmask() == make_address_v4("0.0.0.0"));

  msg.clear();
  try
  {
    make_network_v4(addr, 33);
  }
  catch(std::out_of_range& ex)
  {
    msg = ex.what();
  }
  BOOST_ASIO_CHECK(msg == std::string("prefix length too large"));

  // construct address range from address and prefix length
  BOOST_ASIO_CHECK(network_v4(make_address_v4("192.168.77.100"), 32).network() == make_address_v4("192.168.77.100"));
  BOOST_ASIO_CHECK(network_v4(make_address_v4("192.168.77.100"), 24).network() == make_address_v4("192.168.77.0"));
  BOOST_ASIO_CHECK(network_v4(make_address_v4("192.168.77.128"), 25).network() == make_address_v4("192.168.77.128"));

  // construct address range from string in CIDR notation
  BOOST_ASIO_CHECK(make_network_v4("192.168.77.100/32").network() == make_address_v4("192.168.77.100"));
  BOOST_ASIO_CHECK(make_network_v4("192.168.77.100/24").network() == make_address_v4("192.168.77.0"));
  BOOST_ASIO_CHECK(make_network_v4("192.168.77.128/25").network() == make_address_v4("192.168.77.128"));

  // construct network from invalid string
  boost::system::error_code ec;
  make_network_v4("10.0.0.256/24", ec);
  BOOST_ASIO_CHECK(!!ec);
  make_network_v4("10.0.0.0/33", ec);
  BOOST_ASIO_CHECK(!!ec);
  make_network_v4("10.0.0.0/-1", ec);
  BOOST_ASIO_CHECK(!!ec);
  make_network_v4("10.0.0.0/", ec);
  BOOST_ASIO_CHECK(!!ec);
  make_network_v4("10.0.0.0", ec);
  BOOST_ASIO_CHECK(!!ec);

  // prefix length
  BOOST_ASIO_CHECK(make_network_v4("193.99.144.80/24").prefix_length() == 24);
  BOOST_ASIO_CHECK(network_v4(make_address_v4("193.99.144.80"), 24).prefix_length() == 24);
  BOOST_ASIO_CHECK(network_v4(make_address_v4("192.168.77.0"), make_address_v4("255.255.255.0")).prefix_length() == 24);

  // to string
  std::string a("192.168.77.0/32");
  BOOST_ASIO_CHECK(make_network_v4(a.c_str()).to_string() == a);
  BOOST_ASIO_CHECK(network_v4(make_address_v4("192.168.77.10"), 24).to_string() == std::string("192.168.77.10/24"));

  // return host part
  BOOST_ASIO_CHECK(make_network_v4("192.168.77.11/24").address() == make_address_v4("192.168.77.11"));

  // return host in CIDR notation
  BOOST_ASIO_CHECK(make_network_v4("192.168.78.30/20").address().to_string() == "192.168.78.30");

  // return network in CIDR notation
  BOOST_ASIO_CHECK(make_network_v4("192.168.78.30/20").canonical().to_string() == "192.168.64.0/20");

  // is host
  BOOST_ASIO_CHECK(make_network_v4("192.168.77.0/32").is_host());
  BOOST_ASIO_CHECK(!make_network_v4("192.168.77.0/31").is_host());

  // is real subnet of
  BOOST_ASIO_CHECK(make_network_v4("192.168.0.192/24").is_subnet_of(make_network_v4("192.168.0.0/16")));
  BOOST_ASIO_CHECK(make_network_v4("192.168.0.0/24").is_subnet_of(make_network_v4("192.168.192.168/16")));
  BOOST_ASIO_CHECK(make_network_v4("192.168.0.192/24").is_subnet_of(make_network_v4("192.168.192.168/16")));
  BOOST_ASIO_CHECK(make_network_v4("192.168.0.0/24").is_subnet_of(make_network_v4("192.168.0.0/16")));
  BOOST_ASIO_CHECK(make_network_v4("192.168.0.0/24").is_subnet_of(make_network_v4("192.168.0.0/23")));
  BOOST_ASIO_CHECK(make_network_v4("192.168.0.0/24").is_subnet_of(make_network_v4("192.168.0.0/0")));
  BOOST_ASIO_CHECK(make_network_v4("192.168.0.0/32").is_subnet_of(make_network_v4("192.168.0.0/24")));

  BOOST_ASIO_CHECK(!make_network_v4("192.168.0.0/32").is_subnet_of(make_network_v4("192.168.0.0/32")));
  BOOST_ASIO_CHECK(!make_network_v4("192.168.0.0/24").is_subnet_of(make_network_v4("192.168.1.0/24")));
  BOOST_ASIO_CHECK(!make_network_v4("192.168.0.0/16").is_subnet_of(make_network_v4("192.168.1.0/24")));

  network_v4 r(make_network_v4("192.168.0.0/24"));
  BOOST_ASIO_CHECK(!r.is_subnet_of(r));

  network_v4 net12(make_network_v4("192.168.0.2/24"));
  network_v4 net13(make_network_v4("192.168.1.1/28"));
  network_v4 net14(make_network_v4("192.168.1.21/28"));
  // network
  BOOST_ASIO_CHECK(net12.network() == make_address_v4("192.168.0.0"));
  BOOST_ASIO_CHECK(net13.network() == make_address_v4("192.168.1.0"));
  BOOST_ASIO_CHECK(net14.network() == make_address_v4("192.168.1.16"));
  // netmask
  BOOST_ASIO_CHECK(net12.netmask() == make_address_v4("255.255.255.0"));
  BOOST_ASIO_CHECK(net13.netmask() == make_address_v4("255.255.255.240"));
  BOOST_ASIO_CHECK(net14.netmask() == make_address_v4("255.255.255.240"));
  // broadcast
  BOOST_ASIO_CHECK(net12.broadcast() == make_address_v4("192.168.0.255"));
  BOOST_ASIO_CHECK(net13.broadcast() == make_address_v4("192.168.1.15"));
  BOOST_ASIO_CHECK(net14.broadcast() == make_address_v4("192.168.1.31"));
  // iterator
  BOOST_ASIO_CHECK(std::distance(net12.hosts().begin(),net12.hosts().end()) == 254);
  BOOST_ASIO_CHECK(*net12.hosts().begin() == make_address_v4("192.168.0.1"));
  BOOST_ASIO_CHECK(net12.hosts().end() != net12.hosts().find(make_address_v4("192.168.0.10")));
  BOOST_ASIO_CHECK(net12.hosts().end() == net12.hosts().find(make_address_v4("192.168.1.10")));
  BOOST_ASIO_CHECK(std::distance(net13.hosts().begin(),net13.hosts().end()) == 14);
  BOOST_ASIO_CHECK(*net13.hosts().begin() == make_address_v4("192.168.1.1"));
  BOOST_ASIO_CHECK(net13.hosts().end() != net13.hosts().find(make_address_v4("192.168.1.14")));
  BOOST_ASIO_CHECK(net13.hosts().end() == net13.hosts().find(make_address_v4("192.168.1.15")));
  BOOST_ASIO_CHECK(std::distance(net14.hosts().begin(),net14.hosts().end()) == 14);
  BOOST_ASIO_CHECK(*net14.hosts().begin() == make_address_v4("192.168.1.17"));
  BOOST_ASIO_CHECK(net14.hosts().end() != net14.hosts().find(make_address_v4("192.168.1.30")));
  BOOST_ASIO_CHECK(net14.hosts().end() == net14.hosts().find(make_address_v4("192.168.1.31")));
}

} // namespace ip_network_v4_runtime

//------------------------------------------------------------------------------

BOOST_ASIO_TEST_SUITE
(
  "ip/network_v4",
  BOOST_ASIO_TEST_CASE(ip_network_v4_compile::test)
  BOOST_ASIO_TEST_CASE(ip_network_v4_runtime::test)
)
