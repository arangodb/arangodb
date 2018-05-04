//
// connect_pair.cpp
// ~~~~~~~~~~~~~~~~
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
#include "asio/local/connect_pair.hpp"

#include "asio/io_context.hpp"
#include "asio/local/datagram_protocol.hpp"
#include "asio/local/stream_protocol.hpp"
#include "../unit_test.hpp"

//------------------------------------------------------------------------------

// local_connect_pair_compile test
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// The following test checks that all host_name functions compile and link
// correctly. Runtime failures are ignored.

namespace local_connect_pair_compile {

void test()
{
#if defined(ASIO_HAS_LOCAL_SOCKETS)
  using namespace asio;
  namespace local = asio::local;
  typedef local::datagram_protocol dp;
  typedef local::stream_protocol sp;

  try
  {
    asio::io_context io_context;
    asio::error_code ec1;

    dp::socket s1(io_context);
    dp::socket s2(io_context);
    local::connect_pair(s1, s2);

    dp::socket s3(io_context);
    dp::socket s4(io_context);
    local::connect_pair(s3, s4, ec1);

    sp::socket s5(io_context);
    sp::socket s6(io_context);
    local::connect_pair(s5, s6);

    sp::socket s7(io_context);
    sp::socket s8(io_context);
    local::connect_pair(s7, s8, ec1);
  }
  catch (std::exception&)
  {
  }
#endif // defined(ASIO_HAS_LOCAL_SOCKETS)
}

} // namespace local_connect_pair_compile

//------------------------------------------------------------------------------

ASIO_TEST_SUITE
(
  "local/connect_pair",
  ASIO_TEST_CASE(local_connect_pair_compile::test)
)
