//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_DETAIL_UTF8_CHECKER_HPP
#define BOOST_BEAST_WEBSOCKET_DETAIL_UTF8_CHECKER_HPP

#include <boost/beast/core/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/assert.hpp>
#include <algorithm>
#include <cstdint>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

/** A UTF8 validator.

    This validator can be used to check if a buffer containing UTF8 text is
    valid. The write function may be called incrementally with segmented UTF8
    sequences. The finish function determines if all processed text is valid.
*/
template<class = void>
class utf8_checker_t
{
    std::size_t need_ = 0;  // chars we need to finish the code point
    std::uint8_t* p_ = cp_; // current position in temp buffer
    std::uint8_t cp_[4];    // a temp buffer for the code point

public:
    /** Prepare to process text as valid utf8
    */
    void
    reset();

    /** Check that all processed text is valid utf8
    */
    bool
    finish();

    /** Check if text is valid UTF8

        @return `true` if the text is valid utf8 or false otherwise.
    */
    bool
    write(std::uint8_t const* in, std::size_t size);

    /** Check if text is valid UTF8

        @return `true` if the text is valid utf8 or false otherwise.
    */
    template<class ConstBufferSequence>
    bool
    write(ConstBufferSequence const& bs);
};

template<class _>
void
utf8_checker_t<_>::
reset()
{
    need_ = 0;
    p_ = cp_;
}

template<class _>
bool
utf8_checker_t<_>::
finish()
{
    auto const success = need_ == 0;
    reset();
    return success;
}

template<class _>
template<class ConstBufferSequence>
bool
utf8_checker_t<_>::
write(ConstBufferSequence const& bs)
{
    static_assert(boost::asio::is_const_buffer_sequence<ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
    for(auto b : beast::detail::buffers_range(bs))
        if(! write(static_cast<
            std::uint8_t const*>(b.data()),
                b.size()))
            return false;
    return true;
}

template<class _>
bool
utf8_checker_t<_>::
write(std::uint8_t const* in, std::size_t size)
{
    auto const valid =
        [](std::uint8_t const*& p)
        {
            if(p[0] < 128)
            {
                ++p;
                return true;
            }
            if((p[0] & 0xe0) == 0xc0)
            {
                if( (p[1] & 0xc0) != 0x80 ||
                    (p[0] & 0xfe) == 0xc0)  // overlong
                    return false;
                p += 2;
                return true;
            }
            if((p[0] & 0xf0) == 0xe0)
            {
                if(    (p[1] & 0xc0) != 0x80
                    || (p[2] & 0xc0) != 0x80
                    || (p[0] == 0xe0 && (p[1] & 0xe0) == 0x80) // overlong
                    || (p[0] == 0xed && (p[1] & 0xe0) == 0xa0) // surrogate
                    //|| (p[0] == 0xef && p[1] == 0xbf && (p[2] & 0xfe) == 0xbe) // U+FFFE or U+FFFF
                    )
                    return false;
                p += 3;
                return true;
            }
            if((p[0] & 0xf8) == 0xf0)
            {
                if(    (p[1] & 0xc0) != 0x80
                    || (p[2] & 0xc0) != 0x80
                    || (p[3] & 0xc0) != 0x80
                    || (p[0] == 0xf0 && (p[1] & 0xf0) == 0x80) // overlong
                    || (p[0] == 0xf4 && p[1] > 0x8f) || p[0] > 0xf4 // > U+10FFFF
                    )
                    return false;
                p += 4;
                return true;
            }
            return false;
        };
    auto const fail_fast =
        [&]()
        {
            auto const n = p_ - cp_;
            switch(n)
            {
            default:
                BOOST_ASSERT(false);
                BOOST_FALLTHROUGH;
            case 1:
                cp_[1] = 0x81;
                BOOST_FALLTHROUGH;
            case 2:
                cp_[2] = 0x81;
                BOOST_FALLTHROUGH;
            case 3:
                cp_[3] = 0x81;
                break;
            }
            std::uint8_t const* p = cp_;
            return ! valid(p);
        };
    auto const needed =
        [](std::uint8_t const v)
        {
            if(v < 128)
                return 1;
            if(v < 192)
                return 0;
            if(v < 224)
                return 2;
            if(v < 240)
                return 3;
            if(v < 248)
                return 4;
            return 0;
        };

    auto const end = in + size;

    // Finish up any incomplete code point
    if(need_ > 0)
    {
        // Calculate what we have
        auto n = (std::min)(size, need_);
        size -= n;
        need_ -= n;

        // Add characters to the code point
        while(n--)
            *p_++ = *in++;
        BOOST_ASSERT(p_ <= cp_ + 4);

        // Still incomplete?
        if(need_ > 0)
        {
            // Incomplete code point
            BOOST_ASSERT(in == end);

            // Do partial validation on the incomplete
            // code point, this is called "Fail fast"
            // in Autobahn|Testsuite parlance.
            return ! fail_fast();
        }

        // Complete code point, validate it
        std::uint8_t const* p = &cp_[0];
        if(! valid(p))
            return false;
        p_ = cp_;
    }

    if(size <= sizeof(std::size_t))
        goto slow;

    // Align `in` to sizeof(std::size_t) boundary
    {
        auto const in0 = in;
        auto last = reinterpret_cast<std::uint8_t const*>(
            ((reinterpret_cast<std::uintptr_t>(in) + sizeof(std::size_t) - 1) /
                sizeof(std::size_t)) * sizeof(std::size_t));

        // Check one character at a time for low-ASCII
        while(in < last)
        {
            if(*in & 0x80)
            {
                // Not low-ASCII so switch to slow loop
                size = size - (in - in0);
                goto slow;
            }
            ++in;
        }
        size = size - (in - in0);
    }

    // Fast loop: Process 4 or 8 low-ASCII characters at a time
    {
        auto const in0 = in;
        auto last = in + size - 7;
        auto constexpr mask = static_cast<
            std::size_t>(0x8080808080808080 & ~std::size_t{0});
        while(in < last)
        {
#if 0
            std::size_t temp;
            std::memcpy(&temp, in, sizeof(temp));
            if((temp & mask) != 0)
#else
            // Technically UB but works on all known platforms
            if((*reinterpret_cast<std::size_t const*>(in) & mask) != 0)
#endif
            {
                size = size - (in - in0);
                goto slow;
            }
            in += sizeof(std::size_t);
        }
        // There's at least one more full code point left
        last += 4;
        while(in < last)
            if(! valid(in))
                return false;
        goto tail;
    }

slow:
    // Slow loop: Full validation on one code point at a time
    {
        auto last = in + size - 3;
        while(in < last)
            if(! valid(in))
                return false;
    }

tail:
    // Handle the remaining bytes. The last
    // characters could split a code point so
    // we save the partial code point for later.
    //
    // On entry to the loop, `in` points to the
    // beginning of a code point.
    //
    for(;;)
    {
        // Number of chars left
        auto n = end - in;
        if(! n)
            break;

        // Chars we need to finish this code point
        auto const need = needed(*in);
        if(need == 0)
            return false;
        if(need <= n)
        {
            // Check a whole code point
            if(! valid(in))
                return false;
        }
        else
        {
            // Calculate how many chars we need
            // to finish this partial code point
            need_ = need - n;

            // Save the partial code point
            while(n--)
                *p_++ = *in++;
            BOOST_ASSERT(in == end);
            BOOST_ASSERT(p_ <= cp_ + 4);

            // Do partial validation on the incomplete
            // code point, this is called "Fail fast"
            // in Autobahn|Testsuite parlance.
            return ! fail_fast();
        }
    }
    return true;
}

using utf8_checker = utf8_checker_t<>;

template<class = void>
bool
check_utf8(char const* p, std::size_t n)
{
    utf8_checker c;
    if(! c.write(reinterpret_cast<const uint8_t*>(p), n))
        return false;
    return c.finish();
}

} // detail
} // websocket
} // beast
} // boost

#endif
