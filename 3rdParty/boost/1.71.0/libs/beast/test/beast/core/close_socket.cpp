//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/close_socket.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/string.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <utility>

namespace boost {
namespace beast {

class close_socket_test : public beast::unit_test::suite
{
public:
    void
    run() override
    {
        testClose();
        testJavadocs();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,close_socket);

} // beast
} // boost
