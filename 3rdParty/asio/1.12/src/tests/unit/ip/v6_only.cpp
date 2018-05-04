//
// v6_only.cpp
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
#include "asio/ip/v6_only.hpp"

#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/ip/udp.hpp"
#include "../unit_test.hpp"

//------------------------------------------------------------------------------

// ip_v6_only_compile test
// ~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks that the ip::v6_only socket option compiles and
// link correctly. Runtime failures are ignored.

namespace ip_v6_only_compile {

void test()
{
  using namespace asio;
  namespace ip = asio::ip;

  try
  {
    io_context ioc;
    ip::udp::socket sock(ioc);

    // v6_only class.

    ip::v6_only v6_only1(true);
    sock.set_option(v6_only1);
    ip::v6_only v6_only2;
    sock.get_option(v6_only2);
    v6_only1 = true;
    (void)static_cast<bool>(v6_only1);
    (void)static_cast<bool>(!v6_only1);
    (void)static_cast<bool>(v6_only1.value());
  }
  catch (std::exception&)
  {
  }
}

} // namespace ip_v6_only_compile

//------------------------------------------------------------------------------

// ip_v6_only_runtime test
// ~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks the runtime operation of the ip::v6_only socket
// option.

namespace ip_v6_only_runtime {

void test()
{
  using namespace asio;
  namespace ip = asio::ip;

  io_context ioc;
  asio::error_code ec;

  ip::tcp::endpoint ep_v6(ip::address_v6::loopback(), 0);
  ip::tcp::acceptor acceptor_v6(ioc);
  acceptor_v6.open(ep_v6.protocol(), ec);
  acceptor_v6.bind(ep_v6, ec);
  bool have_v6 = !ec;
  acceptor_v6.close(ec);
  acceptor_v6.open(ep_v6.protocol(), ec);

  if (have_v6)
  {
    ip::v6_only v6_only1;
    acceptor_v6.get_option(v6_only1, ec);
    ASIO_CHECK(!ec);
    bool have_dual_stack = !v6_only1.value();

    if (have_dual_stack)
    {
      ip::v6_only v6_only2(false);
      ASIO_CHECK(!v6_only2.value());
      ASIO_CHECK(!static_cast<bool>(v6_only2));
      ASIO_CHECK(!v6_only2);
      acceptor_v6.set_option(v6_only2, ec);
      ASIO_CHECK(!ec);

      ip::v6_only v6_only3;
      acceptor_v6.get_option(v6_only3, ec);
      ASIO_CHECK(!ec);
      ASIO_CHECK(!v6_only3.value());
      ASIO_CHECK(!static_cast<bool>(v6_only3));
      ASIO_CHECK(!v6_only3);

      ip::v6_only v6_only4(true);
      ASIO_CHECK(v6_only4.value());
      ASIO_CHECK(static_cast<bool>(v6_only4));
      ASIO_CHECK(!!v6_only4);
      acceptor_v6.set_option(v6_only4, ec);
      ASIO_CHECK(!ec);

      ip::v6_only v6_only5;
      acceptor_v6.get_option(v6_only5, ec);
      ASIO_CHECK(!ec);
      ASIO_CHECK(v6_only5.value());
      ASIO_CHECK(static_cast<bool>(v6_only5));
      ASIO_CHECK(!!v6_only5);
    }
  }
}

} // namespace ip_v6_only_runtime

//------------------------------------------------------------------------------

ASIO_TEST_SUITE
(
  "ip/v6_only",
  ASIO_TEST_CASE(ip_v6_only_compile::test)
  ASIO_TEST_CASE(ip_v6_only_runtime::test)
)
