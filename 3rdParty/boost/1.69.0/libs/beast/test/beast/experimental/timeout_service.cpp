//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/experimental/core/timeout_service.hpp>

#include <boost/beast/unit_test/suite.hpp>

namespace boost {
namespace beast {

class timeout_service_test
    : public beast::unit_test::suite
{
public:
    void
    run() override
    {
        boost::asio::io_context ctx;
        set_timeout_service_options(ctx,
            std::chrono::seconds(1));
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,timeout_service);

} // beast
} // boost
