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
#include <boost/beast/core/detail/type_traits.hpp>
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

using prepared_key = std::array<unsigned char, 4>;

inline
void
prepare_key(prepared_key& prepared, std::uint32_t key)
{
    prepared[0] = (key >>  0) & 0xff;
    prepared[1] = (key >>  8) & 0xff;
    prepared[2] = (key >> 16) & 0xff;
    prepared[3] = (key >> 24) & 0xff;
}

template<std::size_t N>
void
rol(std::array<unsigned char, N>& v, std::size_t n)
{
    auto v0 = v;
    for(std::size_t i = 0; i < v.size(); ++i )
        v[i] = v0[(i + n) % v.size()];
}

// Apply mask in place
//
inline
void
mask_inplace(boost::asio::mutable_buffer& b, prepared_key& key)
{
    auto n = b.size();
    auto mask = key; // avoid aliasing
    auto p = static_cast<unsigned char*>(b.data());
    while(n >= 4)
    {
        for(int i = 0; i < 4; ++i)
            p[i] ^= mask[i];
        p += 4;
        n -= 4;
    }
    if(n > 0)
    {
        for(std::size_t i = 0; i < n; ++i)
            p[i] ^= mask[i];
        rol(key, n);
    }
}

// Apply mask in place
//
template<class MutableBuffers, class KeyType>
void
mask_inplace(MutableBuffers const& bs, KeyType& key)
{
    for(boost::asio::mutable_buffer b :
            beast::detail::buffers_range(bs))
        mask_inplace(b, key);
}

} // detail
} // websocket
} // beast
} // boost

#endif
