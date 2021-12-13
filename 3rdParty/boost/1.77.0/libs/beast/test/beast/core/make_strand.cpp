//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/net::make_strand.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/io_context.hpp>

namespace boost {
namespace beast {

class make_strand_test : public beast::unit_test::suite
{
public:
    void
    testFunction()
    {
        net::io_context ioc;
        net::make_strand(ioc);
        net::make_strand(ioc.get_executor());
        net::make_strand(net::make_strand(ioc));

        net::any_io_executor ex(ioc.get_executor());
        net::make_strand(ex);

        // this *should-not* compile
        //net::make_strand(ex.context());
    }

    void
    run() override
    {
        testFunction();
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,net::make_strand);

} // beast
} // boost
