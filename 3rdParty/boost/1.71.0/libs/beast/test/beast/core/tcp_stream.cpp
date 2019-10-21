//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/tcp_stream.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {

class tcp_stream_test
    : public beast::unit_test::suite
{
public:
    using tcp = net::ip::tcp;

    void
    testStream()
    {
        net::io_context ioc;
        {
            tcp::socket s(ioc);
            tcp_stream s1(std::move(s));
        }
    }

    void
    run()
    {
        testStream();
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,tcp_stream);

} // beast
} // boost
