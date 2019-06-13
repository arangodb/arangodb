//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_STREAM_IPP
#define BOOST_BEAST_WEBSOCKET_IMPL_STREAM_IPP

#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/teardown.hpp>
#include <boost/beast/websocket/detail/hybi13.hpp>
#include <boost/beast/websocket/detail/pmd_extension.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/http/rfc7230.hpp>
#include <boost/beast/core/buffers_cat.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/core/detail/clamp.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/assert.hpp>
#include <boost/endian/buffers.hpp>
#include <boost/make_unique.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

#include <iostream>

namespace boost {
namespace beast {
namespace websocket {

template<class NextLayer, bool deflateSupported>
template<class... Args>
stream<NextLayer, deflateSupported>::
stream(Args&&... args)
    : stream_(std::forward<Args>(args)...)
{
    BOOST_ASSERT(rd_buf_.max_size() >=
        max_control_frame_size);
}

template<class NextLayer, bool deflateSupported>
template<class DynamicBuffer, class>
std::size_t
stream<NextLayer, deflateSupported>::
read_size_hint(DynamicBuffer& buffer) const
{
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    auto const initial_size = (std::min)(
        +tcp_frame_size,
        buffer.max_size() - buffer.size());
    if(initial_size == 0)
        return 1; // buffer is full
    return read_size_hint(initial_size);
}

//------------------------------------------------------------------------------

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
set_option(permessage_deflate const& o, std::true_type)
{
    if( o.server_max_window_bits > 15 ||
        o.server_max_window_bits < 9)
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "invalid server_max_window_bits"});
    if( o.client_max_window_bits > 15 ||
        o.client_max_window_bits < 9)
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "invalid client_max_window_bits"});
    if( o.compLevel < 0 ||
        o.compLevel > 9)
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "invalid compLevel"});
    if( o.memLevel < 1 ||
        o.memLevel > 9)
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "invalid memLevel"});
    this->pmd_opts_ = o;
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
set_option(permessage_deflate const& o, std::false_type)
{
    if(o.client_enable || o.server_enable)
    {
        // Can't enable permessage-deflate
        // when deflateSupported == false.
        //
        BOOST_THROW_EXCEPTION(std::invalid_argument{
            "deflateSupported == false"});
        }
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
open(role_type role)
{
    // VFALCO TODO analyze and remove dupe code in reset()
    role_ = role;
    status_ = status::open;
    rd_remain_ = 0;
    rd_cont_ = false;
    rd_done_ = true;
    // Can't clear this because accept uses it
    //rd_buf_.reset();
    rd_fh_.fin = false;
    rd_close_ = false;
    wr_close_ = false;
    // These should not be necessary, because all completion
    // handlers must be allowed to execute otherwise the
    // stream exhibits undefined behavior.
    wr_block_.reset();
    rd_block_.reset();
    cr_.code = close_code::none;

    wr_cont_ = false;
    wr_buf_size_ = 0;

    open_pmd(is_deflate_supported{});
}

template<class NextLayer, bool deflateSupported>
inline
void
stream<NextLayer, deflateSupported>::
open_pmd(std::true_type)
{
    if(((role_ == role_type::client &&
            this->pmd_opts_.client_enable) ||
        (role_ == role_type::server &&
            this->pmd_opts_.server_enable)) &&
        this->pmd_config_.accept)
    {
        pmd_normalize(this->pmd_config_);
        this->pmd_.reset(new typename
            detail::stream_base<deflateSupported>::pmd_type);
        if(role_ == role_type::client)
        {
            this->pmd_->zi.reset(
                this->pmd_config_.server_max_window_bits);
            this->pmd_->zo.reset(
                this->pmd_opts_.compLevel,
                this->pmd_config_.client_max_window_bits,
                this->pmd_opts_.memLevel,
                zlib::Strategy::normal);
        }
        else
        {
            this->pmd_->zi.reset(
                this->pmd_config_.client_max_window_bits);
            this->pmd_->zo.reset(
                this->pmd_opts_.compLevel,
                this->pmd_config_.server_max_window_bits,
                this->pmd_opts_.memLevel,
                zlib::Strategy::normal);
        }
    }
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
close()
{
    wr_buf_.reset();
    close_pmd(is_deflate_supported{});
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
reset()
{
    BOOST_ASSERT(status_ != status::open);
    rd_remain_ = 0;
    rd_cont_ = false;
    rd_done_ = true;
    rd_buf_.consume(rd_buf_.size());
    rd_fh_.fin = false;
    rd_close_ = false;
    wr_close_ = false;
    wr_cont_ = false;
    // These should not be necessary, because all completion
    // handlers must be allowed to execute otherwise the
    // stream exhibits undefined behavior.
    wr_block_.reset();
    rd_block_.reset();
    cr_.code = close_code::none;
}

// Called before each write frame
template<class NextLayer, bool deflateSupported>
inline
void
stream<NextLayer, deflateSupported>::
begin_msg(std::true_type)
{
    wr_frag_ = wr_frag_opt_;
    wr_compress_ = static_cast<bool>(this->pmd_);

    // Maintain the write buffer
    if( wr_compress_ ||
        role_ == role_type::client)
    {
        if(! wr_buf_ || wr_buf_size_ != wr_buf_opt_)
        {
            wr_buf_size_ = wr_buf_opt_;
            wr_buf_ = boost::make_unique_noinit<
                std::uint8_t[]>(wr_buf_size_);
        }
    }
    else
    {
        wr_buf_size_ = wr_buf_opt_;
        wr_buf_.reset();
    }
}

// Called before each write frame
template<class NextLayer, bool deflateSupported>
inline
void
stream<NextLayer, deflateSupported>::
begin_msg(std::false_type)
{
    wr_frag_ = wr_frag_opt_;

    // Maintain the write buffer
    if(role_ == role_type::client)
    {
        if(! wr_buf_ || wr_buf_size_ != wr_buf_opt_)
        {
            wr_buf_size_ = wr_buf_opt_;
            wr_buf_ = boost::make_unique_noinit<
                std::uint8_t[]>(wr_buf_size_);
        }
    }
    else
    {
        wr_buf_size_ = wr_buf_opt_;
        wr_buf_.reset();
    }
}

template<class NextLayer, bool deflateSupported>
std::size_t
stream<NextLayer, deflateSupported>::
read_size_hint(
    std::size_t initial_size,
    std::true_type) const
{
    using beast::detail::clamp;
    std::size_t result;
    BOOST_ASSERT(initial_size > 0);
    if(! this->pmd_ || (! rd_done_ && ! this->pmd_->rd_set))
    {
        // current message is uncompressed

        if(rd_done_)
        {
            // first message frame
            result = initial_size;
            goto done;
        }
        else if(rd_fh_.fin)
        {
            // last message frame
            BOOST_ASSERT(rd_remain_ > 0);
            result = clamp(rd_remain_);
            goto done;
        }
    }
    result = (std::max)(
        initial_size, clamp(rd_remain_));
done:
    BOOST_ASSERT(result != 0);
    return result;
}

template<class NextLayer, bool deflateSupported>
std::size_t
stream<NextLayer, deflateSupported>::
read_size_hint(
    std::size_t initial_size,
    std::false_type) const
{
    using beast::detail::clamp;
    std::size_t result;
    BOOST_ASSERT(initial_size > 0);
    // compression is not supported
    if(rd_done_)
    {
        // first message frame
        result = initial_size;
    }
    else if(rd_fh_.fin)
    {
        // last message frame
        BOOST_ASSERT(rd_remain_ > 0);
        result = clamp(rd_remain_);
    }
    else
    {
        result = (std::max)(
            initial_size, clamp(rd_remain_));
    }
    BOOST_ASSERT(result != 0);
    return result;
}

//------------------------------------------------------------------------------

// Attempt to read a complete frame header.
// Returns `false` if more bytes are needed
template<class NextLayer, bool deflateSupported>
template<class DynamicBuffer>
bool
stream<NextLayer, deflateSupported>::
parse_fh(
    detail::frame_header& fh,
    DynamicBuffer& b,
    error_code& ec)
{
    using boost::asio::buffer;
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    if(buffer_size(b.data()) < 2)
    {
        // need more bytes
        ec.assign(0, ec.category());
        return false;
    }
    buffers_suffix<typename
        DynamicBuffer::const_buffers_type> cb{
            b.data()};
    std::size_t need;
    {
        std::uint8_t tmp[2];
        cb.consume(buffer_copy(buffer(tmp), cb));
        fh.len = tmp[1] & 0x7f;
        switch(fh.len)
        {
            case 126: need = 2; break;
            case 127: need = 8; break;
            default:
                need = 0;
        }
        fh.mask = (tmp[1] & 0x80) != 0;
        if(fh.mask)
            need += 4;
        if(buffer_size(cb) < need)
        {
            // need more bytes
            ec.assign(0, ec.category());
            return false;
        }
        fh.op   = static_cast<
            detail::opcode>(tmp[0] & 0x0f);
        fh.fin  = (tmp[0] & 0x80) != 0;
        fh.rsv1 = (tmp[0] & 0x40) != 0;
        fh.rsv2 = (tmp[0] & 0x20) != 0;
        fh.rsv3 = (tmp[0] & 0x10) != 0;
    }
    switch(fh.op)
    {
    case detail::opcode::binary:
    case detail::opcode::text:
        if(rd_cont_)
        {
            // new data frame when continuation expected
            ec = error::bad_data_frame;
            return false;
        }
        if(fh.rsv2 || fh.rsv3 ||
            ! this->rd_deflated(fh.rsv1))
        {
            // reserved bits not cleared
            ec = error::bad_reserved_bits;
            return false;
        }
        break;

    case detail::opcode::cont:
        if(! rd_cont_)
        {
            // continuation without an active message
            ec = error::bad_continuation;
            return false;
        }
        if(fh.rsv1 || fh.rsv2 || fh.rsv3)
        {
            // reserved bits not cleared
            ec = error::bad_reserved_bits;
            return false;
        }
        break;

    default:
        if(detail::is_reserved(fh.op))
        {
            // reserved opcode
            ec = error::bad_opcode;
            return false;
        }
        if(! fh.fin)
        {
            // fragmented control message
            ec = error::bad_control_fragment;
            return false;
        }
        if(fh.len > 125)
        {
            // invalid length for control message
            ec = error::bad_control_size;
            return false;
        }
        if(fh.rsv1 || fh.rsv2 || fh.rsv3)
        {
            // reserved bits not cleared
            ec = error::bad_reserved_bits;
            return false;
        }
        break;
    }
    if(role_ == role_type::server && ! fh.mask)
    {
        // unmasked frame from client
        ec = error::bad_unmasked_frame;
        return false;
    }
    if(role_ == role_type::client && fh.mask)
    {
        // masked frame from server
        ec = error::bad_masked_frame;
        return false;
    }
    if(detail::is_control(fh.op) &&
        buffer_size(cb) < need + fh.len)
    {
        // Make the entire control frame payload
        // get read in before we return `true`
        return false;
    }
    switch(fh.len)
    {
    case 126:
    {
        std::uint8_t tmp[2];
        BOOST_ASSERT(buffer_size(cb) >= sizeof(tmp));
        cb.consume(buffer_copy(buffer(tmp), cb));
        fh.len = detail::big_uint16_to_native(&tmp[0]);
        if(fh.len < 126)
        {
            // length not canonical
            ec = error::bad_size;
            return false;
        }
        break;
    }
    case 127:
    {
        std::uint8_t tmp[8];
        BOOST_ASSERT(buffer_size(cb) >= sizeof(tmp));
        cb.consume(buffer_copy(buffer(tmp), cb));
        fh.len = detail::big_uint64_to_native(&tmp[0]);
        if(fh.len < 65536)
        {
            // length not canonical
            ec = error::bad_size;
            return false;
        }
        break;
    }
    }
    if(fh.mask)
    {
        std::uint8_t tmp[4];
        BOOST_ASSERT(buffer_size(cb) >= sizeof(tmp));
        cb.consume(buffer_copy(buffer(tmp), cb));
        fh.key = detail::little_uint32_to_native(&tmp[0]);
        detail::prepare_key(rd_key_, fh.key);
    }
    else
    {
        // initialize this otherwise operator== breaks
        fh.key = 0;
    }
    if(! detail::is_control(fh.op))
    {
        if(fh.op != detail::opcode::cont)
        {
            rd_size_ = 0;
            rd_op_ = fh.op;
        }
        else
        {
            if(rd_size_ > (std::numeric_limits<
                    std::uint64_t>::max)() - fh.len)
            {
                // message size exceeds configured limit
                ec = error::message_too_big;
                return false;
            }
        }
        if(! this->rd_deflated())
        {
            if(rd_msg_max_ && beast::detail::sum_exceeds(
                rd_size_, fh.len, rd_msg_max_))
            {
                // message size exceeds configured limit
                ec = error::message_too_big;
                return false;
            }
        }
        rd_cont_ = ! fh.fin;
        rd_remain_ = fh.len;
    }
    b.consume(b.size() - buffer_size(cb));
    ec.assign(0, ec.category());
    return true;
}

template<class NextLayer, bool deflateSupported>
template<class DynamicBuffer>
void
stream<NextLayer, deflateSupported>::
write_close(DynamicBuffer& db, close_reason const& cr)
{
    using namespace boost::endian;
    detail::frame_header fh;
    fh.op = detail::opcode::close;
    fh.fin = true;
    fh.rsv1 = false;
    fh.rsv2 = false;
    fh.rsv3 = false;
    fh.len = cr.code == close_code::none ?
        0 : 2 + cr.reason.size();
    if(role_ == role_type::client)
    {
        fh.mask = true;
        fh.key = this->create_mask();
    }
    else
    {
        fh.mask = false;
    }
    detail::write(db, fh);
    if(cr.code != close_code::none)
    {
        detail::prepared_key key;
        if(fh.mask)
            detail::prepare_key(key, fh.key);
        {
            std::uint8_t tmp[2];
            ::new(&tmp[0]) big_uint16_buf_t{
                (std::uint16_t)cr.code};
            auto mb = db.prepare(2);
            boost::asio::buffer_copy(mb,
                boost::asio::buffer(tmp));
            if(fh.mask)
                detail::mask_inplace(mb, key);
            db.commit(2);
        }
        if(! cr.reason.empty())
        {
            auto mb = db.prepare(cr.reason.size());
            boost::asio::buffer_copy(mb,
                boost::asio::const_buffer(
                    cr.reason.data(), cr.reason.size()));
            if(fh.mask)
                detail::mask_inplace(mb, key);
            db.commit(cr.reason.size());
        }
    }
}

template<class NextLayer, bool deflateSupported>
template<class DynamicBuffer>
void
stream<NextLayer, deflateSupported>::
write_ping(DynamicBuffer& db,
    detail::opcode code, ping_data const& data)
{
    detail::frame_header fh;
    fh.op = code;
    fh.fin = true;
    fh.rsv1 = false;
    fh.rsv2 = false;
    fh.rsv3 = false;
    fh.len = data.size();
    fh.mask = role_ == role_type::client;
    if(fh.mask)
        fh.key = this->create_mask();
    detail::write(db, fh);
    if(data.empty())
        return;
    detail::prepared_key key;
    if(fh.mask)
        detail::prepare_key(key, fh.key);
    auto mb = db.prepare(data.size());
    boost::asio::buffer_copy(mb,
        boost::asio::const_buffer(
            data.data(), data.size()));
    if(fh.mask)
        detail::mask_inplace(mb, key);
    db.commit(data.size());
}

//------------------------------------------------------------------------------

template<class NextLayer, bool deflateSupported>
template<class Decorator>
request_type
stream<NextLayer, deflateSupported>::
build_request(detail::sec_ws_key_type& key,
    string_view host, string_view target,
        Decorator const& decorator)
{
    request_type req;
    req.target(target);
    req.version(11);
    req.method(http::verb::get);
    req.set(http::field::host, host);
    req.set(http::field::upgrade, "websocket");
    req.set(http::field::connection, "upgrade");
    detail::make_sec_ws_key(key);
    req.set(http::field::sec_websocket_key, key);
    req.set(http::field::sec_websocket_version, "13");
    build_request_pmd(req, is_deflate_supported{});
    decorator(req);
    if(! req.count(http::field::user_agent))
        req.set(http::field::user_agent,
            BOOST_BEAST_VERSION_STRING);
    return req;
}

template<class NextLayer, bool deflateSupported>
inline
void
stream<NextLayer, deflateSupported>::
build_request_pmd(request_type& req, std::true_type)
{
    if(this->pmd_opts_.client_enable)
    {
        detail::pmd_offer config;
        config.accept = true;
        config.server_max_window_bits =
            this->pmd_opts_.server_max_window_bits;
        config.client_max_window_bits =
            this->pmd_opts_.client_max_window_bits;
        config.server_no_context_takeover =
            this->pmd_opts_.server_no_context_takeover;
        config.client_no_context_takeover =
            this->pmd_opts_.client_no_context_takeover;
        detail::pmd_write(req, config);
    }
}

template<class NextLayer, bool deflateSupported>
template<class Body, class Allocator, class Decorator>
response_type
stream<NextLayer, deflateSupported>::
build_response(
    http::request<Body,
        http::basic_fields<Allocator>> const& req,
    Decorator const& decorator,
    error_code& result)
{
    auto const decorate =
        [&decorator](response_type& res)
        {
            decorator(res);
            if(! res.count(http::field::server))
            {
                BOOST_STATIC_ASSERT(sizeof(BOOST_BEAST_VERSION_STRING) < 20);
                static_string<20> s(BOOST_BEAST_VERSION_STRING);
                res.set(http::field::server, s);
            }
        };
    auto err =
        [&](error e)
        {
            result = e;
            response_type res;
            res.version(req.version());
            res.result(http::status::bad_request);
            res.body() = result.message();
            res.prepare_payload();
            decorate(res);
            return res;
        };
    if(req.version() != 11)
        return err(error::bad_http_version);
    if(req.method() != http::verb::get)
        return err(error::bad_method);
    if(! req.count(http::field::host))
        return err(error::no_host);
    {
        auto const it = req.find(http::field::connection);
        if(it == req.end())
            return err(error::no_connection);
        if(! http::token_list{it->value()}.exists("upgrade"))
            return err(error::no_connection_upgrade);
    }
    {
        auto const it = req.find(http::field::upgrade);
        if(it == req.end())
            return err(error::no_upgrade);
        if(! http::token_list{it->value()}.exists("websocket"))
            return err(error::no_upgrade_websocket);
    }
    string_view key;
    {
        auto const it = req.find(http::field::sec_websocket_key);
        if(it == req.end())
            return err(error::no_sec_key);
        key = it->value();
        if(key.size() > detail::sec_ws_key_type::max_size_n)
            return err(error::bad_sec_key);
    }
    {
        auto const it = req.find(http::field::sec_websocket_version);
        if(it == req.end())
            return err(error::no_sec_version);
        if(it->value() != "13")
        {
            response_type res;
            res.result(http::status::upgrade_required);
            res.version(req.version());
            res.set(http::field::sec_websocket_version, "13");
            result = error::bad_sec_version;
            res.body() = result.message();
            res.prepare_payload();
            decorate(res);
            return res;
        }
    }

    response_type res;
    res.result(http::status::switching_protocols);
    res.version(req.version());
    res.set(http::field::upgrade, "websocket");
    res.set(http::field::connection, "upgrade");
    {
        detail::sec_ws_accept_type acc;
        detail::make_sec_ws_accept(acc, key);
        res.set(http::field::sec_websocket_accept, acc);
    }
    build_response_pmd(res, req, is_deflate_supported{});
    decorate(res);
    result = {};
    return res;
}

template<class NextLayer, bool deflateSupported>
template<class Body, class Allocator>
inline
void
stream<NextLayer, deflateSupported>::
build_response_pmd(
    response_type& res,
    http::request<Body,
        http::basic_fields<Allocator>> const& req,
    std::true_type)
{
    detail::pmd_offer offer;
    detail::pmd_offer unused;
    pmd_read(offer, req);
    pmd_negotiate(res, unused, offer, this->pmd_opts_);
}

// Called when the WebSocket Upgrade response is received
template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
on_response(
    response_type const& res,
    detail::sec_ws_key_type const& key,
    error_code& ec)
{
    auto const err =
        [&](error e)
        {
            ec = e;
        };
    if(res.result() != http::status::switching_protocols)
        return err(error::upgrade_declined);
    if(res.version() != 11)
        return err(error::bad_http_version);
    {
        auto const it = res.find(http::field::connection);
        if(it == res.end())
            return err(error::no_connection);
        if(! http::token_list{it->value()}.exists("upgrade"))
            return err(error::no_connection_upgrade);
    }
    {
        auto const it = res.find(http::field::upgrade);
        if(it == res.end())
            return err(error::no_upgrade);
        if(! http::token_list{it->value()}.exists("websocket"))
            return err(error::no_upgrade_websocket);
    }
    {
        auto const it = res.find(http::field::sec_websocket_accept);
        if(it == res.end())
            return err(error::no_sec_accept);
        detail::sec_ws_accept_type acc;
        detail::make_sec_ws_accept(acc, key);
        if(acc.compare(it->value()) != 0)
            return err(error::bad_sec_accept);
    }

    ec.assign(0, ec.category());
    on_response_pmd(res, is_deflate_supported{});
    open(role_type::client);
}

template<class NextLayer, bool deflateSupported>
inline
void
stream<NextLayer, deflateSupported>::
on_response_pmd(
    response_type const& res,
    std::true_type)
{
    detail::pmd_offer offer;
    pmd_read(offer, res);
    // VFALCO see if offer satisfies pmd_config_,
    //        return an error if not.
    this->pmd_config_ = offer; // overwrite for now
}

// _Fail the WebSocket Connection_
template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
do_fail(
    std::uint16_t code,         // if set, send a close frame first
    error_code ev,              // error code to use upon success
    error_code& ec)             // set to the error, else set to ev
{
    BOOST_ASSERT(ev);
    status_ = status::closing;
    if(code != close_code::none && ! wr_close_)
    {
        wr_close_ = true;
        detail::frame_buffer fb;
        write_close<
            flat_static_buffer_base>(fb, code);
        boost::asio::write(stream_, fb.data(), ec);
        if(! check_ok(ec))
            return;
    }
    using beast::websocket::teardown;
    teardown(role_, stream_, ec);
    if(ec == boost::asio::error::eof)
    {
        // Rationale:
        // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
        ec.assign(0, ec.category());
    }
    if(! ec)
        ec = ev;
    if(ec && ec != error::closed)
        status_ = status::failed;
    else
        status_ = status::closed;
    close();
}

} // websocket
} // beast
} // boost

#endif
