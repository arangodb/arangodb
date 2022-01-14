//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_NODEJS_PARSER_HPP
#define BOOST_BEAST_HTTP_NODEJS_PARSER_HPP

#include "nodejs-parser/http_parser.h"

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/rfc7230.hpp>
#include <boost/beast/core/buffers_range.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/system/error_code.hpp>
#include <boost/throw_exception.hpp>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast {
namespace http {

namespace detail {

class nodejs_message_category final
    : public boost::system::error_category
{
public:
    const char*
    name() const noexcept override
    {
        return "nodejs-http-error";
    }

    std::string
    message(int ev) const override
    {
        return http_errno_description(
            static_cast<http_errno>(ev));
    }

    boost::system::error_condition
    default_error_condition(int ev) const noexcept override
    {
        return boost::system::error_condition{ev, *this};
    }

    bool
    equivalent(int ev,
        boost::system::error_condition const& condition
            ) const noexcept override
    {
        return condition.value() == ev &&
            &condition.category() == this;
    }

    bool
    equivalent(boost::system::error_code const& error,
        int ev) const noexcept override
    {
        return error.value() == ev &&
            &error.category() == this;
    }
};

template<class = void>
boost::system::error_code
make_nodejs_error(int http_errno)
{
    static nodejs_message_category const mc{};
    return boost::system::error_code{http_errno, mc};
}

inline
char const*
method_to_string(unsigned method)
{
    using namespace beast;
    switch(static_cast<http_method>(method))
    {
    case HTTP_DELETE:      return "DELETE";
    case HTTP_GET:         return "GET";
    case HTTP_HEAD:        return "HEAD";
    case HTTP_POST:        return "POST";
    case HTTP_PUT:         return "PUT";

    // pathological
    case HTTP_CONNECT:     return "CONNECT";
    case HTTP_OPTIONS:     return "OPTIONS";
    case HTTP_TRACE:       return "TRACE";

    // webdav
    case HTTP_COPY:        return "COPY";
    case HTTP_LOCK:        return "LOCK";
    case HTTP_MKCOL:       return "MKCOL";
    case HTTP_MOVE:        return "MOVE";
    case HTTP_PROPFIND:    return "PROPFIND";
    case HTTP_PROPPATCH:   return "PROPPATCH";
    case HTTP_SEARCH:      return "SEARCH";
    case HTTP_UNLOCK:      return "UNLOCK";
    case HTTP_BIND:        return "BIND";
    case HTTP_REBIND:      return "REBIND";
    case HTTP_UNBIND:      return "UNBIND";
    case HTTP_ACL:         return "ACL";

    // subversion
    case HTTP_REPORT:      return "REPORT";
    case HTTP_MKACTIVITY:  return "MKACTIVITY";
    case HTTP_CHECKOUT:    return "CHECKOUT";
    case HTTP_MERGE:       return "MERGE";

    // upnp
    case HTTP_MSEARCH:     return "MSEARCH";
    case HTTP_NOTIFY:      return "NOTIFY";
    case HTTP_SUBSCRIBE:   return "SUBSCRIBE";
    case HTTP_UNSUBSCRIBE: return "UNSUBSCRIBE";

    // RFC-5789
    case HTTP_PATCH:       return "PATCH";
    case HTTP_PURGE:       return "PURGE";

    // CalDav
    case HTTP_MKCALENDAR:  return "MKCALENDAR";

    // RFC-2068, section 19.6.1.2
    case HTTP_LINK:        return "LINK";
    case HTTP_UNLINK:      return "UNLINK";
    };

    return "<unknown>";
}

} // detail

template<class Derived>
class nodejs_basic_parser
{
    http_parser state_;
    boost::system::error_code* ec_;
    bool complete_ = false;
    std::string url_;
    std::string status_;
    std::string field_;
    std::string value_;

public:
    using error_code = boost::system::error_code;

    nodejs_basic_parser(nodejs_basic_parser&& other);

    nodejs_basic_parser&
    operator=(nodejs_basic_parser&& other);

    nodejs_basic_parser(nodejs_basic_parser const& other);

    nodejs_basic_parser& operator=(nodejs_basic_parser const& other);

    explicit
    nodejs_basic_parser(bool request) noexcept;

    bool
    complete() const noexcept
    {
        return complete_;
    }

    std::size_t
    write(void const* data, std::size_t size)
    {
        error_code ec;
        auto const used = write(data, size, ec);
        if(ec)
            BOOST_THROW_EXCEPTION(system_error{ec});
        return used;
    }

    std::size_t
    write(void const* data, std::size_t size,
        error_code& ec);

    template<class ConstBufferSequence>
    std::size_t
    write(ConstBufferSequence const& buffers)
    {
        error_code ec;
        auto const used = write(buffers, ec);
        if(ec)
            BOOST_THROW_EXCEPTION(system_error{ec});
        return used;
    }

    template<class ConstBufferSequence>
    std::size_t
    write(ConstBufferSequence const& buffers,
        error_code& ec);

    void
    write_eof()
    {
        error_code ec;
        write_eof(ec);
        if(ec)
            BOOST_THROW_EXCEPTION(system_error{ec});
    }

    void
    write_eof(error_code& ec);

    void
    check_header();

private:
    Derived&
    impl()
    {
        return *static_cast<Derived*>(this);
    }

    static int cb_message_start(http_parser*);
    static int cb_url(http_parser*, char const*, std::size_t);
    static int cb_status(http_parser*, char const*, std::size_t);
    static int cb_header_field(http_parser*, char const*, std::size_t);
    static int cb_header_value(http_parser*, char const*, std::size_t);
    static int cb_headers_complete(http_parser*);
    static int cb_body(http_parser*, char const*, std::size_t);
    static int cb_message_complete(http_parser*);
    static int cb_chunk_header(http_parser*);
    static int cb_chunk_complete(http_parser*);

    struct hooks_t : http_parser_settings
    {
        hooks_t()
        {
            http_parser_settings_init(this);
            on_message_begin    = &nodejs_basic_parser::cb_message_start;
            on_url              = &nodejs_basic_parser::cb_url;
            on_status           = &nodejs_basic_parser::cb_status;
            on_header_field     = &nodejs_basic_parser::cb_header_field;
            on_header_value     = &nodejs_basic_parser::cb_header_value;
            on_headers_complete = &nodejs_basic_parser::cb_headers_complete;
            on_body             = &nodejs_basic_parser::cb_body;
            on_message_complete = &nodejs_basic_parser::cb_message_complete;
            on_chunk_header     = &nodejs_basic_parser::cb_chunk_header;
            on_chunk_complete   = &nodejs_basic_parser::cb_chunk_complete;
        }
    };

    static
    http_parser_settings const*
    hooks();
};

template<class Derived>
template<class ConstBufferSequence>
std::size_t
nodejs_basic_parser<Derived>::write(
    ConstBufferSequence const& buffers, error_code& ec)
{
    static_assert(net::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence type requirements not met");
    std::size_t bytes_used = 0;
    for(auto buffer : beast::buffers_range_ref(buffers))
    {
        auto const n = write(
            static_cast<void const*>(buffer.data()),
                buffer.size(), ec);
        if(ec)
            return 0;
        bytes_used += n;
        if(complete())
            break;
    }
    return bytes_used;
}

template<class Derived>
http_parser_settings const*
nodejs_basic_parser<Derived>::hooks()
{
    static hooks_t const h;
    return &h;
}

template<class Derived>
nodejs_basic_parser<Derived>::
nodejs_basic_parser(nodejs_basic_parser&& other)
{
    state_ = other.state_;
    state_.data = this;
    complete_ = other.complete_;
    url_ = std::move(other.url_);
    status_ = std::move(other.status_);
    field_ = std::move(other.field_);
    value_ = std::move(other.value_);
}

template<class Derived>
auto
nodejs_basic_parser<Derived>::
operator=(nodejs_basic_parser&& other) ->
    nodejs_basic_parser&
{
    state_ = other.state_;
    state_.data = this;
    complete_ = other.complete_;
    url_ = std::move(other.url_);
    status_ = std::move(other.status_);
    field_ = std::move(other.field_);
    value_ = std::move(other.value_);
    return *this;
}

template<class Derived>
nodejs_basic_parser<Derived>::
nodejs_basic_parser(nodejs_basic_parser const& other)
{
    state_ = other.state_;
    state_.data = this;
    complete_ = other.complete_;
    url_ = other.url_;
    status_ = other.status_;
    field_ = other.field_;
    value_ = other.value_;
}

template<class Derived>
auto
nodejs_basic_parser<Derived>::
operator=(nodejs_basic_parser const& other) ->
    nodejs_basic_parser&
{
    state_ = other.state_;
    state_.data = this;
    complete_ = other.complete_;
    url_ = other.url_;
    status_ = other.status_;
    field_ = other.field_;
    value_ = other.value_;
    return *this;
}

template<class Derived>
nodejs_basic_parser<Derived>::
nodejs_basic_parser(bool request) noexcept
{
    state_.data = this;
    http_parser_init(&state_, request
        ? http_parser_type::HTTP_REQUEST
        : http_parser_type::HTTP_RESPONSE);
}

template<class Derived>
std::size_t
nodejs_basic_parser<Derived>::
write(void const* data,
    std::size_t size, error_code& ec)
{
    ec_ = &ec;
    auto const n = http_parser_execute(
        &state_, hooks(),
            static_cast<const char*>(data), size);
    if(! ec)
        ec = detail::make_nodejs_error(
            static_cast<int>(state_.http_errno));
    if(ec)
        return 0;
    return n;
}

template<class Derived>
void
nodejs_basic_parser<Derived>::
write_eof(error_code& ec)
{
    ec_ = &ec;
    http_parser_execute(&state_, hooks(), nullptr, 0);
    if(! ec)
        ec = detail::make_nodejs_error(
            static_cast<int>(state_.http_errno));
}

template<class Derived>
void
nodejs_basic_parser<Derived>::
check_header()
{
    if(! value_.empty())
    {
        impl().on_field(field_, value_);
        field_.clear();
        value_.clear();
    }
}

template<class Derived>
int
nodejs_basic_parser<Derived>::
cb_message_start(http_parser* p)
{
    auto& t = *reinterpret_cast<nodejs_basic_parser*>(p->data);
    t.complete_ = false;
    t.url_.clear();
    t.status_.clear();
    t.field_.clear();
    t.value_.clear();
    t.impl().on_start();
    return 0;
}

template<class Derived>
int
nodejs_basic_parser<Derived>::
cb_url(http_parser* p,
    char const* in, std::size_t bytes)
{
    auto& t = *reinterpret_cast<nodejs_basic_parser*>(p->data);
    t.url_.append(in, bytes);
    return 0;
}

template<class Derived>
int
nodejs_basic_parser<Derived>::
cb_status(http_parser* p,
    char const* in, std::size_t bytes)
{
    auto& t = *reinterpret_cast<nodejs_basic_parser*>(p->data);
    t.status_.append(in, bytes);
    return 0;
}

template<class Derived>
int
nodejs_basic_parser<Derived>::
cb_header_field(http_parser* p,
    char const* in, std::size_t bytes)
{
    auto& t = *reinterpret_cast<nodejs_basic_parser*>(p->data);
    t.check_header();
    t.field_.append(in, bytes);
    return 0;
}

template<class Derived>
int
nodejs_basic_parser<Derived>::
cb_header_value(http_parser* p,
    char const* in, std::size_t bytes)
{
    auto& t = *reinterpret_cast<nodejs_basic_parser*>(p->data);
    t.value_.append(in, bytes);
    return 0;
}

template<class Derived>
int
nodejs_basic_parser<Derived>::
cb_headers_complete(http_parser* p)
{
    auto& t = *reinterpret_cast<nodejs_basic_parser*>(p->data);
    t.check_header();
    t.impl().on_headers_complete(*t.ec_);
    if(*t.ec_)
        return 1;
    bool const keep_alive =
        http_should_keep_alive(p) != 0;
    if(p->type == http_parser_type::HTTP_REQUEST)
    {
        t.impl().on_request(p->method, t.url_,
            p->http_major, p->http_minor, keep_alive,
                p->upgrade);
        return 0;
    }
    return t.impl().on_response(p->status_code, t.status_,
        p->http_major, p->http_minor, keep_alive,
            p->upgrade) ? 0 : 1;
}

template<class Derived>
int
nodejs_basic_parser<Derived>::
cb_body(http_parser* p,
    char const* in, std::size_t bytes)
{
    auto& t = *reinterpret_cast<nodejs_basic_parser*>(p->data);
    t.impl().on_body(in, bytes, *t.ec_);
    return *t.ec_ ? 1 : 0;
}

template<class Derived>
int
nodejs_basic_parser<Derived>::
cb_message_complete(http_parser* p)
{
    auto& t = *reinterpret_cast<nodejs_basic_parser*>(p->data);
    t.complete_ = true;
    t.impl().on_complete();
    return 0;
}

template<class Derived>
int
nodejs_basic_parser<Derived>::
cb_chunk_header(http_parser*)
{
    return 0;
}

template<class Derived>
int
nodejs_basic_parser<Derived>::
cb_chunk_complete(http_parser*)
{
    return 0;
}

//------------------------------------------------------------------------------

/** A HTTP parser.

    The parser may only be used once.
*/
template<bool isRequest, class Body, class Fields>
class nodejs_parser
    : public nodejs_basic_parser<nodejs_parser<isRequest, Body, Fields>>
{
    bool started_ = false;

public:
    nodejs_parser(nodejs_parser&&) = default;

    nodejs_parser()
        : http::nodejs_basic_parser<nodejs_parser>(isRequest)
    {
    }

    /// Returns `true` if at least one byte has been processed
    bool
    started()
    {
        return started_;
    }

private:
    friend class http::nodejs_basic_parser<nodejs_parser>;

    void
    on_start()
    {
        started_ = true;
    }

    void
    on_field(std::string const&, std::string const&)
    {
    }

    void
    on_headers_complete(error_code& ec)
    {
        // vFALCO TODO Decode the Content-Length and
        // Transfer-Encoding, see if we can reserve the buffer.
        //
        // r_.reserve(content_length)
        ec = {};
    }

    bool
    on_request(unsigned, std::string const&,
        int, int, bool, bool, std::true_type)
    {
        return true;
    }

    bool
    on_request(unsigned, std::string const&,
        int, int, bool, bool, std::false_type)
    {
        return true;
    }

    bool
    on_request(unsigned method, std::string const& url,
        int major, int minor, bool keep_alive, bool upgrade)
    {
        return on_request(method, url,
            major, minor, keep_alive, upgrade,
                std::integral_constant<
                    bool, isRequest>{});
    }

    bool
    on_response(int, std::string const&,
        int, int, bool, bool, std::true_type)
    {
        return true;
    }

    bool
    on_response(int, std::string const&, int, int, bool, bool,
        std::false_type)
    {
        return true;
    }

    bool
    on_response(int status, std::string const& reason,
        int major, int minor, bool keep_alive, bool upgrade)
    {
        return on_response(
            status, reason, major, minor, keep_alive, upgrade,
                std::integral_constant<bool, ! isRequest>{});
    }

    void
    on_body(void const*, std::size_t, error_code& ec)
    {
        ec = {};
    }

    void
    on_complete()
    {
    }
};

} // http
} // beast
} // boost

#endif
