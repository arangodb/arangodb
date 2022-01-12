//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/config.hpp>

#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable: 4459) // declaration hides global declaration
#endif

#include <boost/beast/_experimental/unit_test/suite.hpp>

#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace {

#include "websocket_common.ipp"

//[code_websocket_7_1

struct custom_stream;

void
teardown(
    role_type role,
    custom_stream& stream,
    error_code& ec);

template<class TeardownHandler>
void
async_teardown(
    role_type role,
    custom_stream& stream,
    TeardownHandler&& handler);

//]

void
teardown(
    role_type,
    custom_stream&,
    error_code&)
{
}

//[code_websocket_7_2

template <class NextLayer>
struct custom_wrapper
{
    NextLayer next_layer;

    template<class... Args>
    explicit
    custom_wrapper(Args&&... args)
        : next_layer(std::forward<Args>(args)...)
    {
    }

    friend
    void
    teardown(
        role_type role,
        custom_wrapper& stream,
        error_code& ec)
    {
        using boost::beast::websocket::teardown;
        teardown(role, stream.next_layer, ec);
    }

    template<class TeardownHandler>
    friend
    void
    async_teardown(
        role_type role,
        custom_wrapper& stream,
        TeardownHandler&& handler)
    {
        using boost::beast::websocket::async_teardown;
        async_teardown(role, stream.next_layer, std::forward<TeardownHandler>(handler));
    }
};

//]

void
snippets()
{
    //stream<tcp_stream> ws(ioc);

    {
    //[code_websocket_7_3

    //]
    }
}

struct websocket_7_test
    : public boost::beast::unit_test::suite
{
    void
    run() override
    {
        BEAST_EXPECT(&snippets);
        BEAST_EXPECT(static_cast<void(*)(
            role_type, custom_stream&, error_code&)>(
                &teardown));
    }
};

BEAST_DEFINE_TESTSUITE(beast,doc,websocket_7);

} // (anon)

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
