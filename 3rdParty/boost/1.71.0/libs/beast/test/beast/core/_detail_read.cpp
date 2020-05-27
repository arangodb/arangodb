//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/detail/read.hpp>

#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/completion_condition.hpp>

namespace boost {
namespace beast {
namespace detail {

class read_test : public unit_test::suite
{
public:
#if 0
    void
    testRead()
    {
        {
            net::io_context ioc;
            test::stream ts(ioc);
            ostream(ts.buffer()) << "test";
            error_code ec;
            flat_buffer b;
            read(ts, b, boost::asio::transfer_at_least(4), ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(buffers_to_string(b.data()) == "test");
        }
        {
            net::io_context ioc;
            test::stream ts(ioc);
            auto tc = connect(ts);
            ostream(ts.buffer()) << "test";
            tc.close();
            error_code ec;
            flat_buffer b;
            read(ts, b, boost::asio::transfer_all(), ec);
            BEAST_EXPECTS(ec == boost::asio::error::eof, ec.message());
            BEAST_EXPECT(buffers_to_string(b.data()) == "test");
        }
    }

    void
    testAsyncRead()
    {
        {
            net::io_context ioc;
            test::stream ts(ioc);
            ostream(ts.buffer()) << "test";
            flat_buffer b;
            error_code ec;
            bool invoked = false;
            async_read(ts, b, boost::asio::transfer_at_least(4),
                [&invoked, &ec](error_code ec_, std::size_t)
                {
                    ec = ec_;
                    invoked = true;
                });
            ioc.run();
            BEAST_EXPECT(invoked);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(buffers_to_string(b.data()) == "test");
        }
        {
            net::io_context ioc;
            test::stream ts(ioc);
            auto tc = connect(ts);
            ostream(ts.buffer()) << "test";
            tc.close();
            flat_buffer b;
            error_code ec;
            bool invoked = false;
            async_read(ts, b, boost::asio::transfer_all(),
                [&invoked, &ec](error_code ec_, std::size_t)
                {
                    ec = ec_;
                    invoked = true;
                });
            ioc.run();
            BEAST_EXPECT(invoked);
            BEAST_EXPECTS(ec == boost::asio::error::eof, ec.message());
            BEAST_EXPECT(buffers_to_string(b.data()) == "test");
        }
    }
#endif
    void
    run() override
    {
        pass();
        //testRead();
        //testAsyncRead();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,read);

} // detail
} // beast
} // boost
