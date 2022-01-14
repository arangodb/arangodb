//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/stream.hpp>

#include <boost/beast/core/tcp_stream.hpp>
#include <boost/asio/strand.hpp>

#include "test.hpp"

namespace boost {
namespace beast {
namespace websocket {

class stream_test : public websocket_test_suite
{
public:
    void
    testGetSetOption()
    {
        net::io_context ioc;
        stream<test::stream> ws(ioc);

        {
            ws.set_option(
                stream_base::decorator(
                    [](request_type&)
                    {
                    }));

            ws.set_option(
                stream_base::decorator(
                    [](response_type&)
                    {
                    }));
        }

        {
            ws.set_option(
                stream_base::timeout::suggested(
                    role_type::client));

            ws.set_option(
                stream_base::timeout::suggested(
                    role_type::server));

            ws.set_option({
                std::chrono::seconds(30),
                std::chrono::seconds(300),
                true});

            stream_base::timeout opt;
            ws.get_option(opt);
            ws.set_option(opt);
        }
    }

    void
    testOptions()
    {
        {
            std::seed_seq ss{42};
            seed_prng(ss);
        }

        stream<test::stream> ws{ioc_};
        ws.auto_fragment(true);
        ws.write_buffer_bytes(2048);
        ws.binary(false);
        ws.read_message_max(1 * 1024 * 1024);
        try
        {
            ws.write_buffer_bytes(7);
            fail();
        }
        catch(std::exception const&)
        {
            pass();
        }

        ws.secure_prng(true);
        ws.secure_prng(false);

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
    testJavadoc()
    {
        net::io_context ioc;
        {
            websocket::stream<tcp_stream> ws{net::make_strand(ioc)};
        }
        {
            websocket::stream<tcp_stream> ws(ioc);
        }
    }

    void
    run() override
    {
        BOOST_STATIC_ASSERT(std::is_constructible<
            stream<test::stream>, net::io_context&>::value);

        BOOST_STATIC_ASSERT(std::is_move_constructible<
            stream<test::stream>>::value);

    #if 0
        BOOST_STATIC_ASSERT(std::is_move_assignable<
            stream<test::stream>>::value);
    #endif

        BOOST_STATIC_ASSERT(std::is_constructible<
            stream<test::stream&>, test::stream&>::value);

        // VFALCO Should these be allowed for NextLayer references?
        BOOST_STATIC_ASSERT(std::is_move_constructible<
            stream<test::stream&>>::value);
    #if 0
        BOOST_STATIC_ASSERT(std::is_move_assignable<
            stream<test::stream&>>::value);
    #endif

        log << "sizeof(websocket::stream) == " <<
            sizeof(websocket::stream<test::stream&>) << std::endl;
        log << "sizeof(websocket::stream::impl_type) == " <<
            sizeof(websocket::stream<test::stream&>::impl_type) << std::endl;

        testOptions();
        testJavadoc();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,stream);

} // websocket
} // beast
} // boost
