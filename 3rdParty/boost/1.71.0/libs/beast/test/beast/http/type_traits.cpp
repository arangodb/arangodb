//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/type_traits.hpp>

#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/core/buffer_traits.hpp>
#include <boost/beast/core/buffers_cat.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <string>

namespace boost {
namespace beast {
namespace http {

// Buffers

BOOST_STATIC_ASSERT(net::is_const_buffer_sequence<chunk_crlf>::value);

BOOST_STATIC_ASSERT(std::is_same<
    buffers_type<chunk_crlf>,
    net::const_buffer>::value);

BOOST_STATIC_ASSERT(! std::is_same<
    buffers_type<chunk_crlf>,
    net::mutable_buffer>::value);

BOOST_STATIC_ASSERT(std::is_convertible<
    decltype(*net::buffer_sequence_begin(std::declval<chunk_crlf const&>())),
    net::const_buffer
        >::value);

BOOST_STATIC_ASSERT(! std::is_convertible<
    decltype(*net::buffer_sequence_begin(std::declval<chunk_crlf const&>())),
    net::mutable_buffer
        >::value);

BOOST_STATIC_ASSERT(net::is_const_buffer_sequence<
    buffers_cat_view<net::const_buffer, chunk_crlf>
        >::value);

BOOST_STATIC_ASSERT(std::is_same<
    buffers_type<buffers_cat_view<net::const_buffer, chunk_crlf>>,
    net::const_buffer>::value);

BOOST_STATIC_ASSERT(! std::is_same<
    buffers_type<buffers_cat_view<net::const_buffer, chunk_crlf>>,
    net::mutable_buffer>::value);

using bs_t = buffers_suffix<buffers_cat_view<net::const_buffer, chunk_crlf>>;

BOOST_STATIC_ASSERT(std::is_same<
    buffers_type<bs_t>,
    net::const_buffer>::value);

BOOST_STATIC_ASSERT(! std::is_same<
    buffers_type<bs_t>,
    net::mutable_buffer>::value);

BOOST_STATIC_ASSERT(std::is_convertible<
    decltype(*std::declval<bs_t const&>().begin()),
    net::const_buffer>::value);

BOOST_STATIC_ASSERT(! std::is_convertible<
    decltype(*std::declval<bs_t const&>().begin()),
    net::mutable_buffer>::value);

BOOST_STATIC_ASSERT(std::is_same<
    buffers_iterator_type<bs_t>,
    decltype(std::declval<bs_t const&>().begin())>::value);

using iter_t = bs_t::const_iterator;

BOOST_STATIC_ASSERT(std::is_same<
    net::const_buffer,
    decltype(std::declval<iter_t const&>().operator*())
        >::value);

BOOST_STATIC_ASSERT(! std::is_same<
    net::mutable_buffer,
    decltype(std::declval<iter_t const&>().operator*())
        >::value);

BOOST_STATIC_ASSERT(std::is_constructible<
    net::const_buffer,
    decltype(std::declval<iter_t const&>().operator*())>::value);

BOOST_STATIC_ASSERT(std::is_same<
    net::const_buffer, buffers_suffix<
        buffers_cat_view<net::const_buffer, net::const_buffer, chunk_crlf>
            >::const_iterator::value_type>::value);

BOOST_STATIC_ASSERT(std::is_same<
    net::const_buffer, buffers_type<
    std::decay<buffers_cat_view<net::const_buffer, net::const_buffer, chunk_crlf>>::type
        >>::value);

BOOST_STATIC_ASSERT(std::is_same<net::const_buffer, decltype(
    std::declval<buffers_suffix<buffers_cat_view<
        net::const_buffer, net::const_buffer, chunk_crlf>
            >::const_iterator const&>().operator*())>::value);

BOOST_STATIC_ASSERT(std::is_same<net::const_buffer, decltype(
    std::declval<buffers_suffix<buffers_cat_view<
        net::const_buffer, net::const_buffer, chunk_crlf>
            >::const_iterator const&>().operator*())>::value);

BOOST_STATIC_ASSERT(std::is_same<net::const_buffer, decltype(
    std::declval<buffers_suffix<
        buffers_cat_view<
            beast::detail::buffers_ref<buffers_cat_view<
                net::const_buffer,
                net::const_buffer,
                net::const_buffer,
                http::basic_fields<
                    std::allocator<char>>::writer::field_range,
                http::chunk_crlf> >,
            http::detail::chunk_size,
            net::const_buffer,
            http::chunk_crlf,
            http::basic_string_body<
                char, std::char_traits<char>,
                std::allocator<char>>::writer::const_buffers_type,
            http::chunk_crlf,
            net::const_buffer,
            net::const_buffer,
            http::chunk_crlf>>::const_iterator const&>(
                ).operator*())>::value);

// Body

BOOST_STATIC_ASSERT(! is_body_writer<int>::value);

BOOST_STATIC_ASSERT(is_body_writer<empty_body>::value);

BOOST_STATIC_ASSERT(! is_body_reader<std::string>::value);

namespace {

struct not_fields {};

} // (anonymous)

BOOST_STATIC_ASSERT(! is_fields<not_fields>::value);

} // http
} // beast
} // boost
