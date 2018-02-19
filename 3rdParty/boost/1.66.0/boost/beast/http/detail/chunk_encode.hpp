//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_DETAIL_CHUNK_ENCODE_HPP
#define BOOST_BEAST_HTTP_DETAIL_CHUNK_ENCODE_HPP

#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/http/type_traits.hpp>
#include <boost/asio/buffer.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>

namespace boost {
namespace beast {
namespace http {
namespace detail {

struct chunk_extensions
{
    virtual ~chunk_extensions() = default;
    virtual boost::asio::const_buffer str() = 0;
};
    
template<class ChunkExtensions>
struct chunk_extensions_impl : chunk_extensions
{
    ChunkExtensions ext_;

    chunk_extensions_impl(ChunkExtensions&& ext)
        : ext_(std::move(ext))
    {
    }

    chunk_extensions_impl(ChunkExtensions const& ext)
        : ext_(ext)
    {
    }

    boost::asio::const_buffer
    str() override
    {
        auto const s = ext_.str();
        return {s.data(), s.size()};
    }
};

template<class T, class = void>
struct is_chunk_extensions : std::false_type {};

template<class T>
struct is_chunk_extensions<T, beast::detail::void_t<decltype(
    std::declval<string_view&>() = std::declval<T&>().str(),
        (void)0)>> : std::true_type
{
};

//------------------------------------------------------------------------------

/** A buffer sequence containing a chunk-encoding header
*/
class chunk_size
{
    template<class OutIter>
    static
    OutIter
    to_hex(OutIter last, std::size_t n)
    {
        if(n == 0)
        {
            *--last = '0';
            return last;
        }
        while(n)
        {
            *--last = "0123456789abcdef"[n&0xf];
            n>>=4;
        }
        return last;
    }

    struct sequence
    {
        boost::asio::const_buffer b;
        char data[1 + 2 * sizeof(std::size_t)];

        explicit
        sequence(std::size_t n)
        {
            char* it0 = data + sizeof(data);
            auto it = to_hex(it0, n);
            b = {it,
                static_cast<std::size_t>(it0 - it)};
        }
    };

    std::shared_ptr<sequence> sp_;

public:
    using value_type = boost::asio::const_buffer;

    using const_iterator = value_type const*;

    chunk_size(chunk_size const& other) = default;

    /** Construct a chunk header

        @param n The number of octets in this chunk.
    */
    chunk_size(std::size_t n)
        : sp_(std::make_shared<sequence>(n))
    {
    }

    const_iterator
    begin() const
    {
        return &sp_->b;
    }

    const_iterator
    end() const
    {
        return begin() + 1;
    }
};

//------------------------------------------------------------------------------

/// Returns a buffer sequence holding a CRLF for chunk encoding
inline
boost::asio::const_buffer
chunk_crlf()
{
    return {"\r\n", 2};
}

/// Returns a buffer sequence holding a final chunk header
inline
boost::asio::const_buffer
chunk_last()
{
    return {"0\r\n", 3};
}

//------------------------------------------------------------------------------

template<class = void>
struct chunk_crlf_iter_type
{
    class value_type
    {
        char const s[2] = {'\r', '\n'};
        
    public:
        value_type() = default;

        operator
        boost::asio::const_buffer() const
        {
            return {s, sizeof(s)};
        }
    };
    static value_type value;
};

template<class T>
typename chunk_crlf_iter_type<T>::value_type
chunk_crlf_iter_type<T>::value;

using chunk_crlf_iter = chunk_crlf_iter_type<void>;

//------------------------------------------------------------------------------

template<class = void>
struct chunk_size0_iter_type
{
    class value_type
    {
        char const s[3] = {'0', '\r', '\n'};
        
    public:
        value_type() = default;

        operator
        boost::asio::const_buffer() const
        {
            return {s, sizeof(s)};
        }
    };
    static value_type value;
};

template<class T>
typename chunk_size0_iter_type<T>::value_type
chunk_size0_iter_type<T>::value;

using chunk_size0_iter = chunk_size0_iter_type<void>;

struct chunk_size0
{
    using value_type = chunk_size0_iter::value_type;

    using const_iterator = value_type const*;

    const_iterator
    begin() const
    {
        return &chunk_size0_iter::value;
    }

    const_iterator
    end() const
    {
        return begin() + 1;
    }
};

//------------------------------------------------------------------------------

template<class T,
    bool = is_fields<T>::value>
struct buffers_or_fields
{
    using type = typename
        T::writer::const_buffers_type;
};

template<class T>
struct buffers_or_fields<T, false>
{
    using type = T;
};

} // detail
} // http
} // beast
} // boost

#endif
