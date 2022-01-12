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
    //[code_websocket_4_1

        net::const_buffer b("Hello, world!", 13);

        // This sets all outgoing messages to be sent as text.
        // Text messages must contain valid utf8, this is checked
        // when reading but not when writing.

        ws.text(true);

        // Write the buffer as text
        ws.write(b);
    //]
    }

    {
    //[code_websocket_4_2

        // This DynamicBuffer will hold the received message
        flat_buffer buffer;

        // Read a complete message into the buffer's input area
        ws.read(buffer);

        // Set text mode if the received message was also text,
        // otherwise binary mode will be set.
        ws.text(ws.got_text());

        // Echo the received message back to the peer. If the received
        // message was in text mode, the echoed message will also be
        // in text mode, otherwise it will be in binary mode.
        ws.write(buffer.data());

        // Discard all of the bytes stored in the dynamic buffer,
        // otherwise the next call to read will append to the existing
        // data instead of building a fresh message.
        buffer.consume(buffer.size());

    //]
    }

    {
    //[code_websocket_4_3

        // This DynamicBuffer will hold the received message
        multi_buffer buffer;

        // Read the next message in pieces
        do
        {
            // Append up to 512 bytes of the message into the buffer
            ws.read_some(buffer, 512);
        }
        while(! ws.is_message_done());

        // At this point we have a complete message in the buffer, now echo it

        // The echoed message will be sent in binary mode if the received
        // message was in binary mode, otherwise we will send in text mode.
        ws.binary(ws.got_binary());

        // This buffer adaptor allows us to iterate through buffer in pieces
        buffers_suffix<multi_buffer::const_buffers_type> cb{buffer.data()};

        // Echo the received message in pieces.
        // This will cause the message to be broken up into multiple frames.
        for(;;)
        {
            if(buffer_bytes(cb) > 512)
            {
                // There are more than 512 bytes left to send, just
                // send the next 512 bytes. The value `false` informs
                // the stream that the message is not complete.
                ws.write_some(false, buffers_prefix(512, cb));

                // This efficiently discards data from the adaptor by
                // simply ignoring it, but does not actually affect the
                // underlying dynamic buffer.
                cb.consume(512);
            }
            else
            {
                // Only 512 bytes or less remain, so write the whole
                // thing and inform the stream that this piece represents
                // the end of the message by passing `true`.
                ws.write_some(true, cb);
                break;
            }
        }

        // Discard all of the bytes stored in the dynamic buffer,
        // otherwise the next call to read will append to the existing
        // data instead of building a fresh message.
        buffer.consume(buffer.size());

    //]
    }
}

struct websocket_4_test
    : public boost::beast::unit_test::suite
{
    void
    run() override
    {
        BEAST_EXPECT(&snippets);
    }
};

BEAST_DEFINE_TESTSUITE(beast,doc,websocket_4);

} // (anon)

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
