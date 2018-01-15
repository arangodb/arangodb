//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_TEARDOWN_IPP
#define BOOST_BEAST_WEBSOCKET_IMPL_TEARDOWN_IPP

#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/post.hpp>
#include <memory>

namespace boost {
namespace beast {
namespace websocket {

namespace detail {

template<class Handler>
class teardown_tcp_op
{
    using socket_type =
        boost::asio::ip::tcp::socket;

    Handler h_;
    socket_type& s_;
    role_type role_;
    int step_ = 0;

public:
    teardown_tcp_op(teardown_tcp_op&& other) = default;
    teardown_tcp_op(teardown_tcp_op const& other) = default;

    template<class DeducedHandler>
    teardown_tcp_op(
        DeducedHandler&& h,
        socket_type& s,
        role_type role)
        : h_(std::forward<DeducedHandler>(h))
        , s_(s)
        , role_(role)
    {
    }

    using allocator_type =
        boost::asio::associated_allocator_t<Handler>;

    allocator_type
    get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(h_);
    }

    using executor_type = boost::asio::associated_executor_t<
        Handler, decltype(std::declval<socket_type&>().get_executor())>;

    executor_type
    get_executor() const noexcept
    {
        return boost::asio::get_associated_executor(
            h_, s_.get_executor());
    }

    void
    operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0);

    friend
    bool asio_handler_is_continuation(teardown_tcp_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return op->step_ >= 3 ||
            asio_handler_is_continuation(std::addressof(op->h_));
    }
};

template<class Handler>
void
teardown_tcp_op<Handler>::
operator()(error_code ec, std::size_t)
{
    using boost::asio::buffer;
    using tcp = boost::asio::ip::tcp;
    switch(step_)
    {
    case 0:
        s_.non_blocking(true, ec);
        if(ec)
        {
            step_ = 1;
            return boost::asio::post(
                s_.get_executor(),
                bind_handler(std::move(*this), ec, 0));
        }
        step_ = 2;
        if(role_ == role_type::server)
            s_.shutdown(tcp::socket::shutdown_send, ec);
        goto do_read;

    case 1:
        break;

    case 2:
        step_ = 3;

    case 3:
        if(ec != boost::asio::error::would_block)
            break;
        {
            char buf[2048];
            s_.read_some(
                boost::asio::buffer(buf), ec);
            if(ec)
                break;
        }

    do_read:
        return s_.async_read_some(
            boost::asio::null_buffers{},
                std::move(*this));
    }
    if(role_ == role_type::client)
        s_.shutdown(tcp::socket::shutdown_send, ec);
    s_.close(ec);
    h_(ec);
}

} // detail

//------------------------------------------------------------------------------

inline
void
teardown(
    role_type role,
    boost::asio::ip::tcp::socket& socket,
    error_code& ec)
{
    using boost::asio::buffer;
    if(role == role_type::server)
        socket.shutdown(
            boost::asio::ip::tcp::socket::shutdown_send, ec);
    while(! ec)
    {
        char buf[8192];
        auto const n = socket.read_some(
            buffer(buf), ec);
        if(! n)
            break;
    }
    if(role == role_type::client)
        socket.shutdown(
            boost::asio::ip::tcp::socket::shutdown_send, ec);
    socket.close(ec);
}

template<class TeardownHandler>
inline
void
async_teardown(
    role_type role,
    boost::asio::ip::tcp::socket& socket,
    TeardownHandler&& handler)
{
    static_assert(beast::is_completion_handler<
        TeardownHandler, void(error_code)>::value,
            "TeardownHandler requirements not met");
    detail::teardown_tcp_op<typename std::decay<
        TeardownHandler>::type>{std::forward<
            TeardownHandler>(handler), socket,
                role}();
}

} // websocket
} // beast
} // boost

#endif
