//
// unicast.cpp
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
#include "asio/ip/unicast.hpp"

#include "asio/io_context.hpp"
#include "asio/ip/udp.hpp"
#include "../unit_test.hpp"

//------------------------------------------------------------------------------

// ip_unicast_compile test
// ~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks that all nested classes, enums and constants in
// ip::unicast compile and link correctly. Runtime failures are ignored.

namespace ip_unicast_compile {

void test()
{
  using namespace asio;
  namespace ip = asio::ip;

  try
  {
    io_context ioc;
    ip::udp::socket sock(ioc);

    // hops class.

    ip::unicast::hops hops1(1024);
    sock.set_option(hops1);
    ip::unicast::hops hops2;
    sock.get_option(hops2);
    hops1 = 1;
    (void)static_cast<int>(hops1.value());
  }
  catch (std::exception&)
  {
  }
}

} // namespace ip_unicast_compile

//------------------------------------------------------------------------------

// ip_unicast_runtime test
// ~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks the runtime operation of the socket options defined
// in the ip::unicast namespace.

namespace ip_unicast_runtime {

void test()
{
  using namespace asio;
  namespace ip = asio::ip;

  io_context ioc;
  asio::error_code ec;

  ip::udp::endpoint ep_v4(ip::address_v4::loopback(), 0);
  ip::udp::socket sock_v4(ioc);
  sock_v4.open(ep_v4.protocol(), ec);
  sock_v4.bind(ep_v4, ec);
  bool have_v4 = !ec;

  ip::udp::endpoint ep_v6(ip::address_v6::loopback(), 0);
  ip::udp::socket sock_v6(ioc);
  sock_v6.open(ep_v6.protocol(), ec);
  sock_v6.bind(ep_v6, ec);
  bool have_v6 = !ec;

  ASIO_CHECK(have_v4 || have_v6);

  // hops class.

  if (have_v4)
  {
    ip::unicast::hops hops1(1);
    ASIO_CHECK(hops1.value() == 1);
    sock_v4.set_option(hops1, ec);
#if defined(ASIO_WINDOWS) && defined(UNDER_CE)
    // Option is not supported under Windows CE.
    ASIO_CHECK_MESSAGE(ec == asio::error::no_protocol_option,
        ec.value() << ", " << ec.message());
#else // defined(ASIO_WINDOWS) && defined(UNDER_CE)
    ASIO_CHECK(!ec);
#endif // defined(ASIO_WINDOWS) && defined(UNDER_CE)

    ip::unicast::hops hops2;
    sock_v4.get_option(hops2, ec);
#if defined(ASIO_WINDOWS) && defined(UNDER_CE)
    // Option is not supported under Windows CE.
    ASIO_CHECK_MESSAGE(ec == asio::error::no_protocol_option,
        ec.value() << ", " << ec.message());
#else // defined(ASIO_WINDOWS) && defined(UNDER_CE)
    ASIO_CHECK(!ec);
    ASIO_CHECK(hops2.value() == 1);
#endif // defined(ASIO_WINDOWS) && defined(UNDER_CE)

    ip::unicast::hops hops3(255);
    ASIO_CHECK(hops3.value() == 255);
    sock_v4.set_option(hops3, ec);
#if defined(ASIO_WINDOWS) && defined(UNDER_CE)
    // Option is not supported under Windows CE.
    ASIO_CHECK_MESSAGE(ec == asio::error::no_protocol_option,
        ec.value() << ", " << ec.message());
#else // defined(ASIO_WINDOWS) && defined(UNDER_CE)
    ASIO_CHECK(!ec);
#endif // defined(ASIO_WINDOWS) && defined(UNDER_CE)

    ip::unicast::hops hops4;
    sock_v4.get_option(hops4, ec);
#if defined(ASIO_WINDOWS) && defined(UNDER_CE)
    // Option is not supported under Windows CE.
    ASIO_CHECK_MESSAGE(ec == asio::error::no_protocol_option,
        ec.value() << ", " << ec.message());
#else // defined(ASIO_WINDOWS) && defined(UNDER_CE)
    ASIO_CHECK(!ec);
    ASIO_CHECK(hops4.value() == 255);
#endif // defined(ASIO_WINDOWS) && defined(UNDER_CE)
  }

  if (have_v6)
  {
    ip::unicast::hops hops1(1);
    ASIO_CHECK(hops1.value() == 1);
    sock_v6.set_option(hops1, ec);
    ASIO_CHECK(!ec);

    ip::unicast::hops hops2;
    sock_v6.get_option(hops2, ec);
    ASIO_CHECK(!ec);
    ASIO_CHECK(hops2.value() == 1);

    ip::unicast::hops hops3(255);
    ASIO_CHECK(hops3.value() == 255);
    sock_v6.set_option(hops3, ec);
    ASIO_CHECK(!ec);

    ip::unicast::hops hops4;
    sock_v6.get_option(hops4, ec);
    ASIO_CHECK(!ec);
    ASIO_CHECK(hops4.value() == 255);
  }
}

} // namespace ip_unicast_runtime

//------------------------------------------------------------------------------

ASIO_TEST_SUITE
(
  "ip/unicast",
  ASIO_TEST_CASE(ip_unicast_compile::test)
  ASIO_TEST_CASE(ip_unicast_runtime::test)
)
