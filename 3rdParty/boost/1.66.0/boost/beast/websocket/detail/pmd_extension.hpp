//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_DETAIL_PMD_EXTENSION_HPP
#define BOOST_BEAST_WEBSOCKET_DETAIL_PMD_EXTENSION_HPP

#include <boost/beast/core/error.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/read_size.hpp>
#include <boost/beast/zlib/deflate_stream.hpp>
#include <boost/beast/zlib/inflate_stream.hpp>
#include <boost/beast/websocket/option.hpp>
#include <boost/beast/http/rfc7230.hpp>
#include <boost/asio/buffer.hpp>
#include <utility>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

// permessage-deflate offer parameters
//
// "context takeover" means:
// preserve sliding window across messages
//
struct pmd_offer
{
    bool accept;

    // 0 = absent, or 8..15
    int server_max_window_bits;

    // -1 = present, 0 = absent, or 8..15
    int client_max_window_bits;

    // `true` if server_no_context_takeover offered
    bool server_no_context_takeover;

    // `true` if client_no_context_takeover offered
    bool client_no_context_takeover;
};

template<class = void>
int
parse_bits(string_view s)
{
    if(s.size() == 0)
        return -1;
    if(s.size() > 2)
        return -1;
    if(s[0] < '1' || s[0] > '9')
        return -1;
    unsigned i = 0;
    for(auto c : s)
    {
        if(c < '0' || c > '9')
            return -1;
        auto const i0 = i;
        i = 10 * i + (c - '0');
        if(i < i0)
            return -1;
    }
    return static_cast<int>(i);
}

// Parse permessage-deflate request fields
//
template<class Allocator>
void
pmd_read(pmd_offer& offer,
    http::basic_fields<Allocator> const& fields)
{
    offer.accept = false;
    offer.server_max_window_bits= 0;
    offer.client_max_window_bits = 0;
    offer.server_no_context_takeover = false;
    offer.client_no_context_takeover = false;

    http::ext_list list{
        fields["Sec-WebSocket-Extensions"]};
    for(auto const& ext : list)
    {
        if(iequals(ext.first, "permessage-deflate"))
        {
            for(auto const& param : ext.second)
            {
                if(iequals(param.first,
                    "server_max_window_bits"))
                {
                    if(offer.server_max_window_bits != 0)
                    {
                        // The negotiation offer contains multiple
                        // extension parameters with the same name.
                        //
                        return; // MUST decline
                    }
                    if(param.second.empty())
                    {
                        // The negotiation offer extension
                        // parameter is missing the value.
                        //
                        return; // MUST decline
                    }
                    offer.server_max_window_bits =
                        parse_bits(param.second);
                    if( offer.server_max_window_bits < 8 ||
                        offer.server_max_window_bits > 15)
                    {
                        // The negotiation offer contains an
                        // extension parameter with an invalid value.
                        //
                        return; // MUST decline
                    }
                }
                else if(iequals(param.first,
                    "client_max_window_bits"))
                {
                    if(offer.client_max_window_bits != 0)
                    {
                        // The negotiation offer contains multiple
                        // extension parameters with the same name.
                        //
                        return; // MUST decline
                    }
                    if(! param.second.empty())
                    {
                        offer.client_max_window_bits =
                            parse_bits(param.second);
                        if( offer.client_max_window_bits < 8 ||
                            offer.client_max_window_bits > 15)
                        {
                            // The negotiation offer contains an
                            // extension parameter with an invalid value.
                            //
                            return; // MUST decline
                        }
                    }
                    else
                    {
                        offer.client_max_window_bits = -1;
                    }
                }
                else if(iequals(param.first,
                    "server_no_context_takeover"))
                {
                    if(offer.server_no_context_takeover)
                    {
                        // The negotiation offer contains multiple
                        // extension parameters with the same name.
                        //
                        return; // MUST decline
                    }
                    if(! param.second.empty())
                    {
                        // The negotiation offer contains an
                        // extension parameter with an invalid value.
                        //
                        return; // MUST decline
                    }
                    offer.server_no_context_takeover = true;
                }
                else if(iequals(param.first,
                    "client_no_context_takeover"))
                {
                    if(offer.client_no_context_takeover)
                    {
                        // The negotiation offer contains multiple
                        // extension parameters with the same name.
                        //
                        return; // MUST decline
                    }
                    if(! param.second.empty())
                    {
                        // The negotiation offer contains an
                        // extension parameter with an invalid value.
                        //
                        return; // MUST decline
                    }
                    offer.client_no_context_takeover = true;
                }
                else
                {
                    // The negotiation offer contains an extension
                    // parameter not defined for use in an offer.
                    //
                    return; // MUST decline
                }
            }
            offer.accept = true;
            return;
        }
    }
}

// Set permessage-deflate fields for a client offer
//
template<class Allocator>
void
pmd_write(http::basic_fields<Allocator>& fields,
    pmd_offer const& offer)
{
    static_string<512> s;
    s = "permessage-deflate";
    if(offer.server_max_window_bits != 0)
    {
        if(offer.server_max_window_bits != -1)
        {
            s += "; server_max_window_bits=";
            s += to_static_string(
                offer.server_max_window_bits);
        }
        else
        {
            s += "; server_max_window_bits";
        }
    }
    if(offer.client_max_window_bits != 0)
    {
        if(offer.client_max_window_bits != -1)
        {
            s += "; client_max_window_bits=";
            s += to_static_string(
                offer.client_max_window_bits);
        }
        else
        {
            s += "; client_max_window_bits";
        }
    }
    if(offer.server_no_context_takeover)
    {
        s += "; server_no_context_takeover";
    }
    if(offer.client_no_context_takeover)
    {
        s += "; client_no_context_takeover";
    }
    fields.set(http::field::sec_websocket_extensions, s);
}

// Negotiate a permessage-deflate client offer
//
template<class Allocator>
void
pmd_negotiate(
    http::basic_fields<Allocator>& fields,
    pmd_offer& config,
    pmd_offer const& offer,
    permessage_deflate const& o)
{
    if(! (offer.accept && o.server_enable))
    {
        config.accept = false;
        return;
    }
    config.accept = true;

    static_string<512> s = "permessage-deflate";

    config.server_no_context_takeover =
        offer.server_no_context_takeover ||
            o.server_no_context_takeover;
    if(config.server_no_context_takeover)
        s += "; server_no_context_takeover";

    config.client_no_context_takeover =
        o.client_no_context_takeover ||
            offer.client_no_context_takeover;
    if(config.client_no_context_takeover)
        s += "; client_no_context_takeover";

    if(offer.server_max_window_bits != 0)
        config.server_max_window_bits = (std::min)(
            offer.server_max_window_bits,
                o.server_max_window_bits);
    else
        config.server_max_window_bits =
            o.server_max_window_bits;
    if(config.server_max_window_bits < 15)
    {
        // ZLib's deflateInit silently treats 8 as
        // 9 due to a bug, so prevent 8 from being used.
        //
        if(config.server_max_window_bits < 9)
            config.server_max_window_bits = 9;

        s += "; server_max_window_bits=";
        s += to_static_string(
            config.server_max_window_bits);
    }

    switch(offer.client_max_window_bits)
    {
    case -1:
        // extension parameter is present with no value
        config.client_max_window_bits =
            o.client_max_window_bits;
        if(config.client_max_window_bits < 15)
        {
            s += "; client_max_window_bits=";
            s += to_static_string(
                config.client_max_window_bits);
        }
        break;

    case 0:
        /*  extension parameter is absent.

            If a received extension negotiation offer doesn't have the
            "client_max_window_bits" extension parameter, the corresponding
            extension negotiation response to the offer MUST NOT include the
            "client_max_window_bits" extension parameter.
        */
        if(o.client_max_window_bits == 15)
            config.client_max_window_bits = 15;
        else
            config.accept = false;
        break;

    default:
        // extension parameter has value in [8..15]
        config.client_max_window_bits = (std::min)(
            o.client_max_window_bits,
                offer.client_max_window_bits);
        s += "; client_max_window_bits=";
        s += to_static_string(
            config.client_max_window_bits);
        break;
    }
    if(config.accept)
        fields.set(http::field::sec_websocket_extensions, s);
}

// Normalize the server's response
//
inline
void
pmd_normalize(pmd_offer& offer)
{
    if(offer.accept)
    {
        if( offer.server_max_window_bits == 0)
            offer.server_max_window_bits = 15;

        if( offer.client_max_window_bits ==  0 ||
            offer.client_max_window_bits == -1)
            offer.client_max_window_bits = 15;
    }
}

//--------------------------------------------------------------------

// Compress a buffer sequence
// Returns: `true` if more calls are needed
//
template<class DeflateStream, class ConstBufferSequence>
bool
deflate(
    DeflateStream& zo,
    boost::asio::mutable_buffer& out,
    buffers_suffix<ConstBufferSequence>& cb,
    bool fin,
    std::size_t& total_in,
    error_code& ec)
{
    using boost::asio::buffer;
    BOOST_ASSERT(out.size() >= 6);
    zlib::z_params zs;
    zs.avail_in = 0;
    zs.next_in = nullptr;
    zs.avail_out = out.size();
    zs.next_out = out.data();
    for(auto in : beast::detail::buffers_range(cb))
    {
        zs.avail_in = in.size();
        if(zs.avail_in == 0)
            continue;
        zs.next_in = in.data();
        zo.write(zs, zlib::Flush::none, ec);
        if(ec)
        {
            if(ec != zlib::error::need_buffers)
                return false;
            BOOST_ASSERT(zs.avail_out == 0);
            BOOST_ASSERT(zs.total_out == out.size());
            ec.assign(0, ec.category());
            break;
        }
        if(zs.avail_out == 0)
        {
            BOOST_ASSERT(zs.total_out == out.size());
            break;
        }
        BOOST_ASSERT(zs.avail_in == 0);
    }
    total_in = zs.total_in;
    cb.consume(zs.total_in);
    if(zs.avail_out > 0 && fin)
    {
        auto const remain = boost::asio::buffer_size(cb);
        if(remain == 0)
        {
            // Inspired by Mark Adler
            // https://github.com/madler/zlib/issues/149
            //
            // VFALCO We could do this flush twice depending
            //        on how much space is in the output.
            zo.write(zs, zlib::Flush::block, ec);
            BOOST_ASSERT(! ec || ec == zlib::error::need_buffers);
            if(ec == zlib::error::need_buffers)
                ec.assign(0, ec.category());
            if(ec)
                return false;
            if(zs.avail_out >= 6)
            {
                zo.write(zs, zlib::Flush::full, ec);
                BOOST_ASSERT(! ec);
                // remove flush marker
                zs.total_out -= 4;
                out = buffer(out.data(), zs.total_out);
                return false;
            }
        }
    }
    ec.assign(0, ec.category());
    out = buffer(out.data(), zs.total_out);
    return true;
}

} // detail
} // websocket
} // beast
} // boost

#endif
