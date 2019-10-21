//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// prevent ssl.hpp from actually being included,
// otherwise we would need OpenSSL on AppVeyor
#ifndef BOOST_ASIO_SSL_HPP
#define BOOST_ASIO_SSL_HPP
namespace boost { namespace asio { namespace ssl { } } }
#endif

//[snippet_core_1a

#include <boost/beast/core.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <thread>

//]

using namespace boost::beast;

namespace doc_core_snippets {

void fxx()
{
//[snippet_core_1b
//
using namespace boost::beast;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

net::io_context ioc;
auto work = net::make_work_guard(ioc);
std::thread t{[&](){ ioc.run(); }};

error_code ec;
tcp::socket sock{ioc};

//]
    boost::ignore_unused(ec);

    {
//[snippet_core_2

// The resolver is used to look up IP addresses and port numbers from a domain and service name pair
tcp::resolver r{ioc};

// A socket represents the local end of a connection between two peers
tcp::socket stream{ioc};

// Establish a connection before sending and receiving data
net::connect(stream, r.resolve("www.example.com", "http"));

// At this point `stream` is a connected to a remote
// host and may be used to perform stream operations.

//]
    }

} // fxx()

//------------------------------------------------------------------------------

//[snippet_core_3

template<class SyncWriteStream>
void write_string(SyncWriteStream& stream, string_view s)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream type requirements not met");
    net::write(stream, net::const_buffer(s.data(), s.size()));
}

//]

} // doc_core_snippets
