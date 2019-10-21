//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// This header file is designed to be included multiple times
// inside of function bodies holding documentation snippets.

using namespace boost::beast;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp     = net::ip::tcp;

error_code ec;
net::io_context ioc;
auto work = net::make_work_guard(ioc);
std::thread t{[&](){ ioc.run(); }};

tcp::socket sock(ioc);

ssl::context ctx(ssl::context::tlsv12);

