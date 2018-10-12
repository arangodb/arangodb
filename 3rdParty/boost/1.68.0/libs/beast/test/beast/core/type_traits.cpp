//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/type_traits.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/detail/consuming_buffers.hpp>
#include <memory>

namespace boost {
namespace beast {

namespace detail {

namespace {

//
// is_invocable
//

struct is_invocable_udt1
{
    void operator()(int) const;
};

struct is_invocable_udt2
{
    int operator()(int) const;
};

struct is_invocable_udt3
{
    int operator()(int);
};

struct is_invocable_udt4
{
    void operator()(std::unique_ptr<int>);
};

#ifndef __INTELLISENSE__
// VFALCO Fails to compile with Intellisense
BOOST_STATIC_ASSERT(is_invocable<is_invocable_udt1, void(int)>::value);
BOOST_STATIC_ASSERT(is_invocable<is_invocable_udt2, int(int)>::value);
BOOST_STATIC_ASSERT(is_invocable<is_invocable_udt3, int(int)>::value);
BOOST_STATIC_ASSERT(! is_invocable<is_invocable_udt1, void(void)>::value);
BOOST_STATIC_ASSERT(! is_invocable<is_invocable_udt2, int(void)>::value);
BOOST_STATIC_ASSERT(! is_invocable<is_invocable_udt2, void(void)>::value);
BOOST_STATIC_ASSERT(! is_invocable<is_invocable_udt3 const, int(int)>::value);
BOOST_STATIC_ASSERT(is_invocable<is_invocable_udt4, void(std::unique_ptr<int>)>::value);
#endif

//
// get_lowest_layer
//

struct F1 {};
struct F2 {};

template<class F>
struct F3
{
    using next_layer_type =
        typename std::remove_reference<F>::type;

    using lowest_layer_type = get_lowest_layer<next_layer_type>;
};

template<class F>
struct F4
{
    using next_layer_type =
        typename std::remove_reference<F>::type;

    using lowest_layer_type =
        get_lowest_layer<next_layer_type>;
};

BOOST_STATIC_ASSERT(std::is_same<get_lowest_layer<F1>, F1>::value);
BOOST_STATIC_ASSERT(std::is_same<get_lowest_layer<F2>, F2>::value);
BOOST_STATIC_ASSERT(std::is_same<get_lowest_layer<F3<F1>>, F1>::value);
BOOST_STATIC_ASSERT(std::is_same<get_lowest_layer<F3<F2>>, F2>::value);
BOOST_STATIC_ASSERT(std::is_same<get_lowest_layer<F4<F1>>, F1>::value);
BOOST_STATIC_ASSERT(std::is_same<get_lowest_layer<F4<F2>>, F2>::value);
BOOST_STATIC_ASSERT(std::is_same<get_lowest_layer<F4<F3<F1>>>, F1>::value);
BOOST_STATIC_ASSERT(std::is_same<get_lowest_layer<F4<F3<F2>>>, F2>::value);

//
// min_all, max_all
//

BOOST_STATIC_ASSERT(std::is_same<std::size_t,
    widest_unsigned<unsigned short, std::size_t>::type>::value);
BOOST_STATIC_ASSERT(std::is_same<std::uint64_t,
    widest_unsigned<unsigned short, std::uint32_t, std::uint64_t>::type>::value);
BOOST_STATIC_ASSERT(! std::is_same<std::uint64_t,
    widest_unsigned<unsigned short, std::uint32_t>::type>::value);
BOOST_STATIC_ASSERT(std::is_same<unsigned short,
    unwidest_unsigned<unsigned short, std::size_t>::type>::value);
BOOST_STATIC_ASSERT(std::is_same<unsigned short,
    unwidest_unsigned<unsigned short, std::uint32_t, std::uint64_t>::type>::value);
BOOST_STATIC_ASSERT(! std::is_same<std::uint32_t,
    unwidest_unsigned<unsigned short, std::uint32_t>::type>::value);
BOOST_STATIC_ASSERT(max_all(1u) == 1);
BOOST_STATIC_ASSERT(max_all(1u, 2u) == 2);
BOOST_STATIC_ASSERT(max_all(1u, 2u, static_cast<std::uint64_t>(3)) == 3);
BOOST_STATIC_ASSERT(min_all(1u) == 1);
BOOST_STATIC_ASSERT(min_all(1u, 2u) == 1);
BOOST_STATIC_ASSERT(min_all(1u, 2u, static_cast<std::uint64_t>(3)) == 1);

} // (anonymous)

} // detail

//
// handler concepts
//

namespace {

struct H
{
    void operator()(int);
};

} // anonymous

BOOST_STATIC_ASSERT(is_completion_handler<H, void(int)>::value);
BOOST_STATIC_ASSERT(! is_completion_handler<H, void(void)>::value);

//
// stream concepts
//

namespace {

using stream_type = boost::asio::ip::tcp::socket;

struct not_a_stream
{
    void
    get_io_service();
};

BOOST_STATIC_ASSERT(has_get_executor<stream_type>::value);
BOOST_STATIC_ASSERT(is_async_read_stream<stream_type>::value);
BOOST_STATIC_ASSERT(is_async_write_stream<stream_type>::value);
BOOST_STATIC_ASSERT(is_async_stream<stream_type>::value);
BOOST_STATIC_ASSERT(is_sync_read_stream<stream_type>::value);
BOOST_STATIC_ASSERT(is_sync_write_stream<stream_type>::value);
BOOST_STATIC_ASSERT(is_sync_stream<stream_type>::value);

BOOST_STATIC_ASSERT(! has_get_executor<not_a_stream>::value);
BOOST_STATIC_ASSERT(! is_async_read_stream<not_a_stream>::value);
BOOST_STATIC_ASSERT(! is_async_write_stream<not_a_stream>::value);
BOOST_STATIC_ASSERT(! is_sync_read_stream<not_a_stream>::value);
BOOST_STATIC_ASSERT(! is_sync_write_stream<not_a_stream>::value);

} // (anonymous)

} // beast
} // boost
