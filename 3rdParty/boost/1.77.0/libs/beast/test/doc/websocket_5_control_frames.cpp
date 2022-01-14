//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/config.hpp>

#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable: 4459) // declaration hides global declaration
#endif

#include <boost/beast/_experimental/unit_test/suite.hpp>

#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace {

#include "websocket_common.ipp"

void
snippets()
{
    stream<tcp_stream> ws(ioc);

    {
    //[code_websocket_5_1

        ws.control_callback(
            [](frame_type kind, string_view payload)
            {
                // Do something with the payload
                boost::ignore_unused(kind, payload);
            });

    //]
    }

    {
    //[code_websocket_5_2
        
        ws.close(close_code::normal);

    //]
    }
    
    {
    //[code_websocket_5_3

        ws.auto_fragment(true);
        ws.write_buffer_bytes(16384);

    //]
    }
}

struct websocket_5_test
    : public boost::beast::unit_test::suite
{
    void
    run() override
    {
        BEAST_EXPECT(&snippets);
    }
};

BEAST_DEFINE_TESTSUITE(beast,doc,websocket_5);

} // (anon)

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
