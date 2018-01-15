//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_HANDSHAKE_IPP
#define BOOST_BEAST_WEBSOCKET_IMPL_HANDSHAKE_IPP

#include <boost/beast/websocket/detail/type_traits.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/assert.hpp>
#include <boost/throw_exception.hpp>
#include <memory>

namespace boost {
namespace beast {
namespace websocket {

//------------------------------------------------------------------------------

// send the upgrade request and process the response
//
template<class NextLayer>
template<class Handler>
class stream<NextLayer>::handshake_op
    : public boost::asio::coroutine
{
    struct data
    {
        stream<NextLayer>& ws;
        response_type* res_p;
        detail::sec_ws_key_type key;
        http::request<http::empty_body> req;
        response_type res;

        template<class Decorator>
        data(Handler&, stream<NextLayer>& ws_,
            response_type* res_p_,
                string_view host,
                    string_view target,
                        Decorator const& decorator)
            : ws(ws_)
            , res_p(res_p_)
            , req(ws.build_request(key,
                host, target, decorator))
        {
            ws.reset();
        }
    };

    handler_ptr<data, Handler> d_;

public:
    handshake_op(handshake_op&&) = default;
    handshake_op(handshake_op const&) = default;

    template<class DeducedHandler, class... Args>
    handshake_op(DeducedHandler&& h,
            stream<NextLayer>& ws, Args&&... args)
        : d_(std::forward<DeducedHandler>(h),
            ws, std::forward<Args>(args)...)
    {
    }

    using allocator_type =
        boost::asio::associated_allocator_t<Handler>;

    allocator_type
    get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(d_.handler());
    }

    using executor_type = boost::asio::associated_executor_t<
        Handler, decltype(std::declval<stream<NextLayer>&>().get_executor())>;

    executor_type
    get_executor() const noexcept
    {
        return boost::asio::get_associated_executor(
            d_.handler(), d_->ws.get_executor());
    }

    void
    operator()(
        error_code ec = {},
        std::size_t bytes_used = 0);

    friend
    bool asio_handler_is_continuation(handshake_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->d_.handler()));
    }
};

template<class NextLayer>
template<class Handler>
void
stream<NextLayer>::handshake_op<Handler>::
operator()(error_code ec, std::size_t)
{
    auto& d = *d_;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        // Send HTTP Upgrade
        pmd_read(d.ws.pmd_config_, d.req);
        BOOST_ASIO_CORO_YIELD
        http::async_write(d.ws.stream_,
            d.req, std::move(*this));
        if(ec)
            goto upcall;

        // VFALCO We could pre-serialize the request to
        //        a single buffer, send that instead,
        //        and delete the buffer here. The buffer
        //        could be a variable block at the end
        //        of handler_ptr's allocation.

        // Read HTTP response
        BOOST_ASIO_CORO_YIELD
        http::async_read(d.ws.next_layer(),
            d.ws.rd_buf_, d.res,
                std::move(*this));
        if(ec)
            goto upcall;
        d.ws.on_response(d.res, d.key, ec);
        if(d.res_p)
            swap(d.res, *d.res_p);
    upcall:
        d_.invoke(ec);
    }
}

template<class NextLayer>
template<class HandshakeHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    HandshakeHandler, void(error_code))
stream<NextLayer>::
async_handshake(string_view host,
    string_view target,
        HandshakeHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    boost::asio::async_completion<HandshakeHandler,
        void(error_code)> init{handler};
    handshake_op<BOOST_ASIO_HANDLER_TYPE(
        HandshakeHandler, void(error_code))>{
            init.completion_handler, *this, nullptr, host,
                target, &default_decorate_req}();
    return init.result.get();
}

template<class NextLayer>
template<class HandshakeHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    HandshakeHandler, void(error_code))
stream<NextLayer>::
async_handshake(response_type& res,
    string_view host,
        string_view target,
            HandshakeHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    boost::asio::async_completion<HandshakeHandler,
        void(error_code)> init{handler};
    handshake_op<BOOST_ASIO_HANDLER_TYPE(
        HandshakeHandler, void(error_code))>{
            init.completion_handler, *this, &res, host,
                target, &default_decorate_req}();
    return init.result.get();
}

template<class NextLayer>
template<class RequestDecorator, class HandshakeHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    HandshakeHandler, void(error_code))
stream<NextLayer>::
async_handshake_ex(string_view host,
    string_view target,
        RequestDecorator const& decorator,
            HandshakeHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    static_assert(detail::is_RequestDecorator<
            RequestDecorator>::value,
        "RequestDecorator requirements not met");
    boost::asio::async_completion<HandshakeHandler,
        void(error_code)> init{handler};
    handshake_op<BOOST_ASIO_HANDLER_TYPE(
        HandshakeHandler, void(error_code))>{
            init.completion_handler, *this, nullptr, host,
                target, decorator}();
    return init.result.get();
}

template<class NextLayer>
template<class RequestDecorator, class HandshakeHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    HandshakeHandler, void(error_code))
stream<NextLayer>::
async_handshake_ex(response_type& res,
    string_view host,
        string_view target,
            RequestDecorator const& decorator,
                HandshakeHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    static_assert(detail::is_RequestDecorator<
            RequestDecorator>::value,
        "RequestDecorator requirements not met");
    boost::asio::async_completion<HandshakeHandler,
        void(error_code)> init{handler};
    handshake_op<BOOST_ASIO_HANDLER_TYPE(
        HandshakeHandler, void(error_code))>{
            init.completion_handler, *this, &res, host,
                target, decorator}();
    return init.result.get();
}

template<class NextLayer>
void
stream<NextLayer>::
handshake(string_view host,
    string_view target)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    error_code ec;
    handshake(
        host, target, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer>
void
stream<NextLayer>::
handshake(response_type& res,
    string_view host,
        string_view target)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    error_code ec;
    handshake(res, host, target, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer>
template<class RequestDecorator>
void
stream<NextLayer>::
handshake_ex(string_view host,
    string_view target,
        RequestDecorator const& decorator)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(detail::is_RequestDecorator<
            RequestDecorator>::value,
        "RequestDecorator requirements not met");
    error_code ec;
    handshake_ex(host, target, decorator, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer>
template<class RequestDecorator>
void
stream<NextLayer>::
handshake_ex(response_type& res,
    string_view host,
        string_view target,
            RequestDecorator const& decorator)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(detail::is_RequestDecorator<
            RequestDecorator>::value,
        "RequestDecorator requirements not met");
    error_code ec;
    handshake_ex(res, host, target, decorator, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer>
void
stream<NextLayer>::
handshake(string_view host,
    string_view target, error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    do_handshake(nullptr,
        host, target, &default_decorate_req, ec);
}

template<class NextLayer>
void
stream<NextLayer>::
handshake(response_type& res,
    string_view host,
        string_view target,
            error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    do_handshake(&res,
        host, target, &default_decorate_req, ec);
}

template<class NextLayer>
template<class RequestDecorator>
void
stream<NextLayer>::
handshake_ex(string_view host,
    string_view target,
        RequestDecorator const& decorator,
            error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(detail::is_RequestDecorator<
            RequestDecorator>::value,
        "RequestDecorator requirements not met");
    do_handshake(nullptr,
        host, target, decorator, ec);
}

template<class NextLayer>
template<class RequestDecorator>
void
stream<NextLayer>::
handshake_ex(response_type& res,
    string_view host,
        string_view target,
            RequestDecorator const& decorator,
                error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(detail::is_RequestDecorator<
            RequestDecorator>::value,
        "RequestDecorator requirements not met");
    do_handshake(&res,
        host, target, decorator, ec);
}

//------------------------------------------------------------------------------

template<class NextLayer>
template<class RequestDecorator>
void
stream<NextLayer>::
do_handshake(
    response_type* res_p,
    string_view host,
    string_view target,
    RequestDecorator const& decorator,
    error_code& ec)
{
    response_type res;
    reset();
    detail::sec_ws_key_type key;
    {
        auto const req = build_request(
            key, host, target, decorator);
        pmd_read(pmd_config_, req);
        http::write(stream_, req, ec);
    }
    if(ec)
        return;
    http::read(next_layer(), rd_buf_, res, ec);
    if(ec)
        return;
    on_response(res, key, ec);
    if(res_p)
        *res_p = std::move(res);
}

} // websocket
} // beast
} // boost

#endif
