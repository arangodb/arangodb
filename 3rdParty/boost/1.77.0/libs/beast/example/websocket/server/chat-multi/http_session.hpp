//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#ifndef BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_HTTP_SESSION_HPP
#define BOOST_BEAST_EXAMPLE_WEBSOCKET_CHAT_MULTI_HTTP_SESSION_HPP

#include "net.hpp"
#include "beast.hpp"
#include "shared_state.hpp"
#include <boost/optional.hpp>
#include <boost/smart_ptr.hpp>
#include <cstdlib>
#include <memory>

/** Represents an established HTTP connection
*/
class http_session : public boost::enable_shared_from_this<http_session>
{
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    boost::shared_ptr<shared_state> state_;

    // The parser is stored in an optional container so we can
    // construct it from scratch it at the beginning of each new message.
    boost::optional<http::request_parser<http::string_body>> parser_;

    struct send_lambda;

    void fail(beast::error_code ec, char const* what);
    void do_read();
    void on_read(beast::error_code ec, std::size_t);
    void on_write(beast::error_code ec, std::size_t, bool close);

public:
    http_session(
        tcp::socket&& socket,
        boost::shared_ptr<shared_state> const& state);

    void run();
};

#endif
