//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_TEST_PARSER_HPP
#define BOOST_BEAST_HTTP_TEST_PARSER_HPP

#include <boost/beast/http/basic_parser.hpp>
#include <boost/beast/_experimental/test/fail_count.hpp>
#include <string>
#include <unordered_map>

namespace boost {
namespace beast {
namespace http {

template<bool isRequest>
class test_parser
    : public basic_parser<isRequest>
{
    test::fail_count* fc_ = nullptr;

public:
    using mutable_buffers_type =
        net::mutable_buffer;

    int status = 0;
    int version = 0;
    std::string method;
    std::string path;
    std::string reason;
    std::string body;
    int got_on_begin       = 0;
    int got_on_field       = 0;
    int got_on_header      = 0;
    int got_on_body        = 0;
    int got_content_length = 0;
    int got_on_chunk       = 0;
    int got_on_complete    = 0;
    std::unordered_map<
        std::string, std::string> fields;

    test_parser() = default;

    explicit
    test_parser(test::fail_count& fc)
        : fc_(&fc)
    {
    }

    void
    on_request_impl(verb, string_view method_str_,
        string_view path_, int version_, error_code& ec)
    {
        method = std::string(
            method_str_.data(), method_str_.size());
        path = std::string(
            path_.data(), path_.size());
        version = version_;
        ++got_on_begin;
        if(fc_)
            fc_->fail(ec);
    }

    void
    on_response_impl(int code,
        string_view reason_,
            int version_, error_code& ec)
    {
        status = code;
        reason = std::string(
            reason_.data(), reason_.size());
        version = version_;
        ++got_on_begin;
        if(fc_)
            fc_->fail(ec);
    }

    void
    on_field_impl(field, string_view name,
        string_view value, error_code& ec)
    {
        ++got_on_field;
        if(fc_)
            fc_->fail(ec);
        fields[std::string(name)] = std::string(value);
    }

    void
    on_header_impl(error_code& ec)
    {
        ++got_on_header;
        if(fc_)
            fc_->fail(ec);
    }

    void
    on_body_init_impl(
        boost::optional<std::uint64_t> const& content_length_,
        error_code& ec)
    {
        // The real implementation clears out the error code in basic_string_body::reader::init
        ec = {};
        ++got_on_body;
        got_content_length =
            static_cast<bool>(content_length_);
        if(fc_)
            fc_->fail(ec);
    }

    std::size_t
    on_body_impl(string_view s,
        error_code& ec)
    {
        body.append(s.data(), s.size());
        if(fc_)
            fc_->fail(ec);
        return s.size();
    }

    void
    on_chunk_header_impl(
        std::uint64_t,
        string_view,
        error_code& ec)
    {
        ++got_on_chunk;
        if(fc_)
            fc_->fail(ec);
    }

    std::size_t
    on_chunk_body_impl(
        std::uint64_t,
        string_view s,
        error_code& ec)
    {
        body.append(s.data(), s.size());
        if(fc_)
            fc_->fail(ec);
        return s.size();
    }


    void
    on_finish_impl(error_code& ec)
    {
        ++got_on_complete;
        if(fc_)
            fc_->fail(ec);
    }
};

} // http
} // beast
} // boost

#endif
