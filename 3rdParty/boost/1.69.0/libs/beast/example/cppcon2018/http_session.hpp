//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#ifndef CPPCON2018_HTTP_SESSION_HPP
#define CPPCON2018_HTTP_SESSION_HPP

#include "net.hpp"
#include "beast.hpp"
#include "shared_state.hpp"
#include <cstdlib>
#include <memory>

/** Represents an established HTTP connection
*/
class http_session : public std::enable_shared_from_this<http_session>
{
    tcp::socket socket_;
    beast::flat_buffer buffer_;
    std::shared_ptr<shared_state> state_;
    http::request<http::string_body> req_;

    struct send_lambda
    {
        http_session& self_;

        explicit
        send_lambda(http_session& self)
            : self_(self)
        {
        }

        template<bool isRequest, class Body, class Fields>
        void
        operator()(http::message<isRequest, Body, Fields>&& msg) const;
    };

    void fail(error_code ec, char const* what);
    void on_read(error_code ec, std::size_t);
    void on_write(
        error_code ec, std::size_t, bool close);

public:
    http_session(
        tcp::socket socket,
        std::shared_ptr<shared_state> const& state);

    void run();
};

#endif
