//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/error.hpp>

#include <boost/beast/unit_test/suite.hpp>
#include <memory>

namespace boost {
namespace beast {
namespace websocket {

class error_test : public unit_test::suite
{
public:
    void check(error e)
    {
        auto const ec = make_error_code(e);
        ec.category().name();
        BEAST_EXPECT(! ec.message().empty());
        BEAST_EXPECT(ec != condition::handshake_failed);
        BEAST_EXPECT(ec != condition::protocol_violation);
    }

    void check(condition c, error e)
    {
        auto const ec = make_error_code(e);
        BEAST_EXPECT(ec.category().name() != nullptr);
        BEAST_EXPECT(! ec.message().empty());
        BEAST_EXPECT(ec == c);
    }

    void run() override
    {
        check(error::closed);
        check(error::buffer_overflow);
        check(error::partial_deflate_block);
        check(error::message_too_big);

        check(condition::protocol_violation, error::bad_opcode);
        check(condition::protocol_violation, error::bad_data_frame);
        check(condition::protocol_violation, error::bad_continuation);
        check(condition::protocol_violation, error::bad_reserved_bits);
        check(condition::protocol_violation, error::bad_control_fragment);
        check(condition::protocol_violation, error::bad_control_size);
        check(condition::protocol_violation, error::bad_unmasked_frame);
        check(condition::protocol_violation, error::bad_masked_frame);
        check(condition::protocol_violation, error::bad_size);
        check(condition::protocol_violation, error::bad_frame_payload);
        check(condition::protocol_violation, error::bad_close_code);
        check(condition::protocol_violation, error::bad_close_size);
        check(condition::protocol_violation, error::bad_close_payload);

        check(condition::handshake_failed, error::bad_http_version);
        check(condition::handshake_failed, error::bad_method);
        check(condition::handshake_failed, error::no_host);
        check(condition::handshake_failed, error::no_connection);
        check(condition::handshake_failed, error::no_connection_upgrade);
        check(condition::handshake_failed, error::no_upgrade);
        check(condition::handshake_failed, error::no_upgrade_websocket);
        check(condition::handshake_failed, error::no_sec_key);
        check(condition::handshake_failed, error::bad_sec_key);
        check(condition::handshake_failed, error::no_sec_version);
        check(condition::handshake_failed, error::bad_sec_version);
        check(condition::handshake_failed, error::no_sec_accept);
        check(condition::handshake_failed, error::bad_sec_accept);
        check(condition::handshake_failed, error::upgrade_declined);
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,error);

} // websocket
} // beast
} // boost
