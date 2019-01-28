//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_DETAIL_FRAME_HPP
#define BOOST_BEAST_WEBSOCKET_DETAIL_FRAME_HPP

#include <boost/beast/websocket/error.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/detail/utf8_checker.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/static_string.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/assert.hpp>
#include <boost/endian/buffers.hpp>
#include <cstdint>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

inline
std::uint16_t
big_uint16_to_native(void const* buf)
{
    auto const p = static_cast<
        std::uint8_t const*>(buf);
    return (p[0]<<8) + p[1];
}

inline
std::uint64_t
big_uint64_to_native(void const* buf)
{
    auto const p = static_cast<
        std::uint8_t const*>(buf);
    return
        (static_cast<std::uint64_t>(p[0])<<56) +
        (static_cast<std::uint64_t>(p[1])<<48) +
        (static_cast<std::uint64_t>(p[2])<<40) +
        (static_cast<std::uint64_t>(p[3])<<32) +
        (static_cast<std::uint64_t>(p[4])<<24) +
        (static_cast<std::uint64_t>(p[5])<<16) +
        (static_cast<std::uint64_t>(p[6])<< 8) +
                                    p[7];
}

inline
std::uint32_t
little_uint32_to_native(void const* buf)
{
    auto const p = static_cast<
        std::uint8_t const*>(buf);
    return
                                    p[0] +
        (static_cast<std::uint32_t>(p[1])<< 8) +
        (static_cast<std::uint32_t>(p[2])<<16) +
        (static_cast<std::uint32_t>(p[3])<<24);
}

inline
void
native_to_little_uint32(std::uint32_t v, void* buf)
{
    auto p = static_cast<std::uint8_t*>(buf);
    p[0] =  v        & 0xff;
    p[1] = (v >>  8) & 0xff;
    p[2] = (v >> 16) & 0xff;
    p[3] = (v >> 24) & 0xff;
}

// frame header opcodes
enum class opcode : std::uint8_t
{
    cont    = 0,
    text    = 1,
    binary  = 2,
    rsv3    = 3,
    rsv4    = 4,
    rsv5    = 5,
    rsv6    = 6,
    rsv7    = 7,
    close   = 8,
    ping    = 9,
    pong    = 10,
    crsvb   = 11,
    crsvc   = 12,
    crsvd   = 13,
    crsve   = 14,
    crsvf   = 15
};

// Contents of a WebSocket frame header
struct frame_header
{
    std::uint64_t len;
    std::uint32_t key;
    opcode op;
    bool fin  : 1;
    bool mask : 1;
    bool rsv1 : 1;
    bool rsv2 : 1;
    bool rsv3 : 1;
};

// holds the largest possible frame header
using fh_buffer = flat_static_buffer<14>;

// holds the largest possible control frame
using frame_buffer =
    flat_static_buffer< 2 + 8 + 4 + 125 >;

inline
bool constexpr
is_reserved(opcode op)
{
    return
        (op >= opcode::rsv3  && op <= opcode::rsv7) ||
        (op >= opcode::crsvb && op <= opcode::crsvf);
}

inline
bool constexpr
is_valid(opcode op)
{
    return op <= opcode::crsvf;
}

inline
bool constexpr
is_control(opcode op)
{
    return op >= opcode::close;
}

inline
bool
is_valid_close_code(std::uint16_t v)
{
    switch(v)
    {
    case close_code::normal:            // 1000
    case close_code::going_away:        // 1001
    case close_code::protocol_error:    // 1002
    case close_code::unknown_data:      // 1003
    case close_code::bad_payload:       // 1007
    case close_code::policy_error:      // 1008
    case close_code::too_big:           // 1009
    case close_code::needs_extension:   // 1010
    case close_code::internal_error:    // 1011
    case close_code::service_restart:   // 1012
    case close_code::try_again_later:   // 1013
        return true;

    // explicitly reserved
    case close_code::reserved1:         // 1004
    case close_code::no_status:         // 1005
    case close_code::abnormal:          // 1006
    case close_code::reserved2:         // 1014
    case close_code::reserved3:         // 1015
        return false;
    }
    // reserved
    if(v >= 1016 && v <= 2999)
        return false;
    // not used
    if(v <= 999)
        return false;
    return true;
}

//------------------------------------------------------------------------------

// Write frame header to dynamic buffer
//
template<class DynamicBuffer>
void
write(DynamicBuffer& db, frame_header const& fh)
{
    using boost::asio::buffer;
    using boost::asio::buffer_copy;
    using namespace boost::endian;
    std::size_t n;
    std::uint8_t b[14];
    b[0] = (fh.fin ? 0x80 : 0x00) | static_cast<std::uint8_t>(fh.op);
    if(fh.rsv1)
        b[0] |= 0x40;
    if(fh.rsv2)
        b[0] |= 0x20;
    if(fh.rsv3)
        b[0] |= 0x10;
    b[1] = fh.mask ? 0x80 : 0x00;
    if(fh.len <= 125)
    {
        b[1] |= fh.len;
        n = 2;
    }
    else if(fh.len <= 65535)
    {
        b[1] |= 126;
        ::new(&b[2]) big_uint16_buf_t{
            (std::uint16_t)fh.len};
        n = 4;
    }
    else
    {
        b[1] |= 127;
        ::new(&b[2]) big_uint64_buf_t{fh.len};
        n = 10;
    }
    if(fh.mask)
    {
        native_to_little_uint32(fh.key, &b[n]);
        n += 4;
    }
    db.commit(buffer_copy(
        db.prepare(n), buffer(b)));
}

// Read data from buffers
// This is for ping and pong payloads
//
template<class Buffers>
void
read_ping(ping_data& data, Buffers const& bs)
{
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    using boost::asio::mutable_buffer;
    BOOST_ASSERT(buffer_size(bs) <= data.max_size());
    data.resize(buffer_size(bs));
    buffer_copy(mutable_buffer{
        data.data(), data.size()}, bs);
}

// Read close_reason, return true on success
// This is for the close payload
//
template<class Buffers>
void
read_close(
    close_reason& cr,
    Buffers const& bs,
    error_code& ec)
{
    using boost::asio::buffer;
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    using namespace boost::endian;
    auto n = buffer_size(bs);
    BOOST_ASSERT(n <= 125);
    if(n == 0)
    {
        cr = close_reason{};
        ec.assign(0, ec.category());
        return;
    }
    if(n == 1)
    {
        // invalid payload size == 1
        ec = error::bad_close_size;
        return;
    }
    buffers_suffix<Buffers> cb(bs);
    {
        std::uint8_t b[2];
        buffer_copy(buffer(b), cb);
        cr.code = big_uint16_to_native(&b[0]);
        cb.consume(2);
        n -= 2;
        if(! is_valid_close_code(cr.code))
        {
            // invalid close code
            ec = error::bad_close_code;
            return;
        }
    }
    if(n > 0)
    {
        cr.reason.resize(n);
        buffer_copy(buffer(&cr.reason[0], n), cb);
        if(! check_utf8(
            cr.reason.data(), cr.reason.size()))
        {
            // not valid utf-8
            ec = error::bad_close_payload;
            return;
        }
    }
    else
    {
        cr.reason = "";
    }
    ec.assign(0, ec.category());
}

} // detail
} // websocket
} // beast
} // boost

#endif
