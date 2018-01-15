//
// Copyright (w) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/stream.hpp>

#include "test.hpp"

namespace boost {
namespace beast {
namespace websocket {

class stream_test : public websocket_test_suite
{
public:
    void
    testOptions()
    {
        stream<test::stream> ws{ioc_};
        ws.auto_fragment(true);
        ws.write_buffer_size(2048);
        ws.binary(false);
        ws.read_message_max(1 * 1024 * 1024);
        try
        {
            ws.write_buffer_size(7);
            fail();
        }
        catch(std::exception const&)
        {
            pass();
        }

        auto const bad =
        [&](permessage_deflate const& pmd)
        {
            stream<test::stream> ws{ioc_};
            try
            {
                ws.set_option(pmd);
                fail("", __FILE__, __LINE__);
            }
            catch(std::exception const&)
            {
                pass();
            }
        };

        {
            permessage_deflate pmd;
            pmd.server_max_window_bits = 16;
            bad(pmd);
        }

        {
            permessage_deflate pmd;
            pmd.server_max_window_bits = 8;
            bad(pmd);
        }

        {
            permessage_deflate pmd;
            pmd.client_max_window_bits = 16;
            bad(pmd);
        }

        {
            permessage_deflate pmd;
            pmd.client_max_window_bits = 8;
            bad(pmd);
        }

        {
            permessage_deflate pmd;
            pmd.compLevel = -1;
            bad(pmd);
        }

        {
            permessage_deflate pmd;
            pmd.compLevel = 10;
            bad(pmd);
        }

        {
            permessage_deflate pmd;
            pmd.memLevel = 0;
            bad(pmd);
        }

        {
            permessage_deflate pmd;
            pmd.memLevel = 10;
            bad(pmd);
        }
    }

    void
    run() override
    {
        BOOST_STATIC_ASSERT(std::is_constructible<
            stream<test::stream>, boost::asio::io_context&>::value);

        BOOST_STATIC_ASSERT(std::is_move_constructible<
            stream<test::stream>>::value);

        BOOST_STATIC_ASSERT(std::is_move_assignable<
            stream<test::stream>>::value);

        BOOST_STATIC_ASSERT(std::is_constructible<
            stream<test::stream&>, test::stream&>::value);

        BOOST_STATIC_ASSERT(std::is_move_constructible<
            stream<test::stream&>>::value);

        BOOST_STATIC_ASSERT(! std::is_move_assignable<
            stream<test::stream&>>::value);

        log << "sizeof(websocket::stream) == " <<
            sizeof(websocket::stream<test::stream&>) << std::endl;

        testOptions();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,stream);

} // websocket
} // beast
} // boost
