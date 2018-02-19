//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_DETAIL_MASK_HPP
#define BOOST_BEAST_WEBSOCKET_DETAIL_MASK_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/buffer.hpp>
#include <array>
#include <climits>
#include <cstdint>
#include <random>
#include <type_traits>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

// Pseudo-random source of mask keys
//
template<class Generator>
class maskgen_t
{
    Generator g_;

public:
    using result_type =
        typename Generator::result_type;

    maskgen_t();

    result_type
    operator()() noexcept;

    void
    rekey();
};

template<class Generator>
maskgen_t<Generator>::maskgen_t()
{
    rekey();
}

template<class Generator>
auto
maskgen_t<Generator>::operator()() noexcept ->
    result_type
{
    for(;;)
        if(auto key = g_())
            return key;
}

template<class _>
void
maskgen_t<_>::rekey()
{
    std::random_device rng;
#if 0
    std::array<std::uint32_t, 32> e;
    for(auto& i : e)
        i = rng();
    // VFALCO This constructor causes
    //        address sanitizer to fail, no idea why.
    std::seed_seq ss(e.begin(), e.end());
    g_.seed(ss);
#else
    g_.seed(rng());
#endif
}

// VFALCO NOTE This generator has 5KB of state!
//using maskgen = maskgen_t<std::mt19937>;
using maskgen = maskgen_t<std::minstd_rand>;

//------------------------------------------------------------------------------

using prepared_key =
    std::conditional<sizeof(void*) == 8,
        std::uint64_t, std::uint32_t>::type;

inline
void
prepare_key(std::uint32_t& prepared, std::uint32_t key)
{
    prepared = key;
}

inline
void
prepare_key(std::uint64_t& prepared, std::uint32_t key)
{
    prepared =
        (static_cast<std::uint64_t>(key) << 32) | key;
}

template<class T>
inline
typename std::enable_if<std::is_integral<T>::value, T>::type
ror(T t, unsigned n = 1)
{
    auto constexpr bits =
        static_cast<unsigned>(
            sizeof(T) * CHAR_BIT);
    n &= bits-1;
    return static_cast<T>((t << (bits - n)) | (
        static_cast<typename std::make_unsigned<T>::type>(t) >> n));
}

// 32-bit optimized
//
template<class = void>
void
mask_inplace_fast(
    boost::asio::mutable_buffer const& b,
        std::uint32_t& key)
{
    auto n = b.size();
    auto p = reinterpret_cast<std::uint8_t*>(b.data());
    if(n >= sizeof(key))
    {
        // Bring p to 4-byte alignment
        auto const i = reinterpret_cast<
            std::uintptr_t>(p) & (sizeof(key)-1);
        switch(i)
        {
        case 1: p[2] ^= static_cast<std::uint8_t>(key >> 16); BOOST_BEAST_FALLTHROUGH;
        case 2: p[1] ^= static_cast<std::uint8_t>(key >> 8);  BOOST_BEAST_FALLTHROUGH;
        case 3: p[0] ^= static_cast<std::uint8_t>(key);
        {
            auto const d = static_cast<unsigned>(sizeof(key) - i);
            key = ror(key, 8*d);
            n -= d;
            p += d;
            BOOST_BEAST_FALLTHROUGH;
        }
        default:
            break;
        }
    }

    // Mask 4 bytes at a time
    for(auto i = n / sizeof(key); i; --i)
    {
        *reinterpret_cast<
            std::uint32_t*>(p) ^= key;
        p += sizeof(key);
    }

    // Leftovers
    n &= sizeof(key)-1;
    switch(n)
    {
    case 3: p[2] ^= static_cast<std::uint8_t>(key >> 16); BOOST_BEAST_FALLTHROUGH;
    case 2: p[1] ^= static_cast<std::uint8_t>(key >> 8);  BOOST_BEAST_FALLTHROUGH;
    case 1: p[0] ^= static_cast<std::uint8_t>(key);
        key = ror(key, static_cast<unsigned>(8*n));
        BOOST_BEAST_FALLTHROUGH;
    default:
        break;
    }
}

// 64-bit optimized
//
template<class = void>
void
mask_inplace_fast(
    boost::asio::mutable_buffer const& b,
        std::uint64_t& key)
{
    auto n = b.size();
    auto p = reinterpret_cast<std::uint8_t*>(b.data());
    if(n >= sizeof(key))
    {
        // Bring p to 8-byte alignment
        auto const i = reinterpret_cast<
            std::uintptr_t>(p) & (sizeof(key)-1);
        switch(i)
        {
        case 1: p[6] ^= static_cast<std::uint8_t>(key >> 48);
        case 2: p[5] ^= static_cast<std::uint8_t>(key >> 40);
        case 3: p[4] ^= static_cast<std::uint8_t>(key >> 32);
        case 4: p[3] ^= static_cast<std::uint8_t>(key >> 24);
        case 5: p[2] ^= static_cast<std::uint8_t>(key >> 16);
        case 6: p[1] ^= static_cast<std::uint8_t>(key >> 8);
        case 7: p[0] ^= static_cast<std::uint8_t>(key);
        {
            auto const d = static_cast<
                unsigned>(sizeof(key) - i);
            key = ror(key, 8*d);
            n -= d;
            p += d;
        }
        default:
            break;
        }
    }

    // Mask 8 bytes at a time
    for(auto i = n / sizeof(key); i; --i)
    {
        *reinterpret_cast<
            std::uint64_t*>(p) ^= key;
        p += sizeof(key);
    }

    // Leftovers
    n &= sizeof(key)-1;
    switch(n)
    {
    case 7: p[6] ^= static_cast<std::uint8_t>(key >> 48);
    case 6: p[5] ^= static_cast<std::uint8_t>(key >> 40);
    case 5: p[4] ^= static_cast<std::uint8_t>(key >> 32);
    case 4: p[3] ^= static_cast<std::uint8_t>(key >> 24);
    case 3: p[2] ^= static_cast<std::uint8_t>(key >> 16);
    case 2: p[1] ^= static_cast<std::uint8_t>(key >> 8);
    case 1: p[0] ^= static_cast<std::uint8_t>(key);
        key = ror(key, static_cast<unsigned>(8*n));
    default:
        break;
    }
}

inline
void
mask_inplace(
    boost::asio::mutable_buffer const& b,
        std::uint32_t& key)
{
    mask_inplace_fast(b, key);
}

inline
void
mask_inplace(
    boost::asio::mutable_buffer const& b,
        std::uint64_t& key)
{
    mask_inplace_fast(b, key);
}

// Apply mask in place
//
template<class MutableBuffers, class KeyType>
void
mask_inplace(
    MutableBuffers const& bs, KeyType& key)
{
    for(boost::asio::mutable_buffer b : bs)
        mask_inplace(b, key);
}

} // detail
} // websocket
} // beast
} // boost

#endif
