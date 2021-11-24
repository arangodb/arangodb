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

//[code_websocket_3_1

void set_user_agent(request_type& req)
{
    // Set the User-Agent on the request
    req.set(http::field::user_agent, "My User Agent");
}

//]

void
snippets()
{
    {
    //[code_websocket_3_2

        stream<tcp_stream> ws(ioc);

        // The function `set_user_agent` will be invoked with
        // every upgrade request before it is sent by the stream.

        ws.set_option(stream_base::decorator(&set_user_agent));

    //]
    }

    stream<tcp_stream> ws(ioc);
    
    {
    //[code_websocket_3_3

        struct set_server
        {
            void operator()(response_type& res)
            {
                // Set the Server field on the response
                res.set(http::field::user_agent, "My Server");
            }
        };

        ws.set_option(stream_base::decorator(set_server{}));

    //]
    }

    {
    //[code_websocket_3_4

        ws.set_option(stream_base::decorator(
            [](response_type& res)
            {
                // Set the Server field on the response
                res.set(http::field::user_agent, "My Server");
            }));

    //]
    }

    {
    //[code_websocket_3_5

        struct set_message_fields
        {
            void operator()(request_type& req)
            {
                // Set the User-Agent on the request
                req.set(http::field::user_agent, "My User Agent");
            }

            void operator()(response_type& res)
            {
                // Set the Server field on the response
                res.set(http::field::user_agent, "My Server");
            }
        };

        ws.set_option(stream_base::decorator(set_message_fields{}));

    //]
    }

    {
    //[code_websocket_3_6
    
        struct set_auth
        {
            std::unique_ptr<std::string> key;

            void operator()(request_type& req)
            {
                // Set the authorization field
                req.set(http::field::authorization, *key);
            }
        };

        // The stream takes ownership of the decorator object
        ws.set_option(stream_base::decorator(
            set_auth{boost::make_unique<std::string>("Basic QWxhZGRpbjpPcGVuU2VzYW1l")}));
    
    //]
    }
}

struct websocket_3_test
    : public boost::beast::unit_test::suite
{
    void
    run() override
    {
        BEAST_EXPECT(&snippets);
    }
};

BEAST_DEFINE_TESTSUITE(beast,doc,websocket_3);

} // (anon)

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
