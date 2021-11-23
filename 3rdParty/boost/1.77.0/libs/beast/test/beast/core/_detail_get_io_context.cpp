//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/detail/get_io_context.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/strand.hpp>

namespace boost {
namespace beast {
namespace detail {

class get_io_context_test : public beast::unit_test::suite
{
public:
    void
    testFunction()
    {
        struct none
        {
        };

        net::io_context ioc;
        BEAST_EXPECT(get_io_context(5) == nullptr);
        BEAST_EXPECT(get_io_context(none{}) == nullptr);
        BEAST_EXPECT(get_io_context(ioc) == &ioc);
        BEAST_EXPECT(get_io_context(ioc.get_executor()) == &ioc);
        BEAST_EXPECT(get_io_context(net::make_strand(ioc)) == &ioc);
        BEAST_EXPECT(get_io_context(net::any_io_executor(ioc.get_executor())) == &ioc);
    }

    void
    run() override
    {
        testFunction();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,get_io_context);

} // detail
} // beast
} // boost
