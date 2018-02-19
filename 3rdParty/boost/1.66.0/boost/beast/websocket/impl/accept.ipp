//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_ACCEPT_IPP
#define BOOST_BEAST_WEBSOCKET_IMPL_ACCEPT_IPP

#include <boost/beast/websocket/detail/type_traits.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/post.hpp>
#include <boost/assert.hpp>
#include <boost/throw_exception.hpp>
#include <memory>
#include <type_traits>

namespace boost {
namespace beast {
namespace websocket {

// Respond to an upgrade HTTP request
template<class NextLayer>
template<class Handler>
class stream<NextLayer>::response_op
    : public boost::asio::coroutine
{
    struct data
    {
        stream<NextLayer>& ws;
        response_type res;

        template<class Body, class Allocator, class Decorator>
        data(Handler&, stream<NextLayer>& ws_, http::request<
            Body, http::basic_fields<Allocator>> const& req,
                Decorator const& decorator)
            : ws(ws_)
            , res(ws_.build_response(req, decorator))
        {
        }
    };

    handler_ptr<data, Handler> d_;

public:
    response_op(response_op&&) = default;
    response_op(response_op const&) = default;

    template<class DeducedHandler, class... Args>
    response_op(DeducedHandler&& h,
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

    void operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0);

    friend
    bool asio_handler_is_continuation(response_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->d_.handler()));
    }
};

template<class NextLayer>
template<class Handler>
void
stream<NextLayer>::
response_op<Handler>::
operator()(
    error_code ec,
    std::size_t)
{
    auto& d = *d_;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        // Send response
        BOOST_ASIO_CORO_YIELD
        http::async_write(d.ws.next_layer(),
            d.res, std::move(*this));
        if(! ec && d.res.result() !=
                http::status::switching_protocols)
            ec = error::handshake_failed;
        if(! ec)
        {
            pmd_read(d.ws.pmd_config_, d.res);
            d.ws.open(role_type::server);
        }
        d_.invoke(ec);
    }
}

//------------------------------------------------------------------------------

// read and respond to an upgrade request
//
template<class NextLayer>
template<class Decorator, class Handler>
class stream<NextLayer>::accept_op
    : public boost::asio::coroutine
{
    struct data
    {
        stream<NextLayer>& ws;
        Decorator decorator;
        http::request_parser<http::empty_body> p;
        data(Handler&, stream<NextLayer>& ws_,
                Decorator const& decorator_)
            : ws(ws_)
            , decorator(decorator_)
        {
        }
    };

    handler_ptr<data, Handler> d_;

public:
    accept_op(accept_op&&) = default;
    accept_op(accept_op const&) = default;

    template<class DeducedHandler, class... Args>
    accept_op(DeducedHandler&& h,
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

    template<class Buffers>
    void run(Buffers const& buffers);

    void operator()(
        error_code ec = {},
        std::size_t bytes_used = 0);

    friend
    bool asio_handler_is_continuation(accept_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->d_.handler()));
    }
};

template<class NextLayer>
template<class Decorator, class Handler>
template<class Buffers>
void
stream<NextLayer>::
accept_op<Decorator, Handler>::
run(Buffers const& buffers)
{
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    auto& d = *d_;
    error_code ec;
    boost::optional<typename
        static_buffer_base::mutable_buffers_type> mb;
    auto const len = buffer_size(buffers);
    try
    {
        mb.emplace(d.ws.rd_buf_.prepare(len));
    }
    catch(std::length_error const&)
    {
        ec = error::buffer_overflow;
        return (*this)(ec);
    }
    d.ws.rd_buf_.commit(
        buffer_copy(*mb, buffers));
    (*this)(ec);
}

template<class NextLayer>
template<class Decorator, class Handler>
void
stream<NextLayer>::
accept_op<Decorator, Handler>::
operator()(error_code ec, std::size_t)
{
    auto& d = *d_;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        if(ec)
        {
            BOOST_ASIO_CORO_YIELD
            boost::asio::post(
                d.ws.get_executor(),
                bind_handler(std::move(*this), ec));
        }
        else
        {
            BOOST_ASIO_CORO_YIELD
            http::async_read(
                d.ws.next_layer(), d.ws.rd_buf_,
                    d.p, std::move(*this));
            if(ec == http::error::end_of_stream)
                ec = error::closed;
            if(! ec)
            {
                // Arguments from our state must be
                // moved to the stack before releasing
                // the handler.
                auto& ws = d.ws;
                auto const req = d.p.release();
                auto const decorator = d.decorator;
            #if 1
                return response_op<Handler>{
                    d_.release_handler(),
                        ws, req, decorator}(ec);
            #else
                // VFALCO This *should* work but breaks
                //        coroutine invariants in the unit test.
                //        Also it calls reset() when it shouldn't.
                return ws.async_accept_ex(
                    req, decorator, d_.release_handler());
            #endif
            }
        }
        d_.invoke(ec);
    }
}

//------------------------------------------------------------------------------

template<class NextLayer>
void
stream<NextLayer>::
accept()
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    error_code ec;
    accept(ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer>
template<class ResponseDecorator>
void
stream<NextLayer>::
accept_ex(ResponseDecorator const& decorator)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(detail::is_ResponseDecorator<
        ResponseDecorator>::value,
            "ResponseDecorator requirements not met");
    error_code ec;
    accept_ex(decorator, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer>
void
stream<NextLayer>::
accept(error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    reset();
    do_accept(&default_decorate_res, ec);
}

template<class NextLayer>
template<class ResponseDecorator>
void
stream<NextLayer>::
accept_ex(ResponseDecorator const& decorator, error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(detail::is_ResponseDecorator<
        ResponseDecorator>::value,
            "ResponseDecorator requirements not met");
    reset();
    do_accept(decorator, ec);
}

template<class NextLayer>
template<class ConstBufferSequence>
typename std::enable_if<! http::detail::is_header<
    ConstBufferSequence>::value>::type
stream<NextLayer>::
accept(ConstBufferSequence const& buffers)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    error_code ec;
    accept(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer>
template<
    class ConstBufferSequence,
    class ResponseDecorator>
typename std::enable_if<! http::detail::is_header<
    ConstBufferSequence>::value>::type
stream<NextLayer>::
accept_ex(
    ConstBufferSequence const& buffers,
    ResponseDecorator const &decorator)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    static_assert(detail::is_ResponseDecorator<
        ResponseDecorator>::value,
            "ResponseDecorator requirements not met");
    error_code ec;
    accept_ex(buffers, decorator, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer>
template<class ConstBufferSequence>
typename std::enable_if<! http::detail::is_header<
    ConstBufferSequence>::value>::type
stream<NextLayer>::
accept(
    ConstBufferSequence const& buffers, error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    reset();
    boost::optional<typename
        static_buffer_base::mutable_buffers_type> mb;
    try
    {
        mb.emplace(rd_buf_.prepare(
            buffer_size(buffers)));
    }
    catch(std::length_error const&)
    {
        ec = error::buffer_overflow;
        return;
    }
    rd_buf_.commit(
        buffer_copy(*mb, buffers));
    do_accept(&default_decorate_res, ec);
}

template<class NextLayer>
template<
    class ConstBufferSequence,
    class ResponseDecorator>
typename std::enable_if<! http::detail::is_header<
    ConstBufferSequence>::value>::type
stream<NextLayer>::
accept_ex(
    ConstBufferSequence const& buffers,
    ResponseDecorator const& decorator,
    error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    reset();
    boost::optional<typename
        static_buffer_base::mutable_buffers_type> mb;
    try
    {
        mb.emplace(rd_buf_.prepare(
            buffer_size(buffers)));
    }
    catch(std::length_error const&)
    {
        ec = error::buffer_overflow;
        return;
    }
    rd_buf_.commit(buffer_copy(*mb, buffers));
    do_accept(decorator, ec);
}

template<class NextLayer>
template<class Body, class Allocator>
void
stream<NextLayer>::
accept(
    http::request<Body,
        http::basic_fields<Allocator>> const& req)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    error_code ec;
    accept(req, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer>
template<
    class Body, class Allocator,
    class ResponseDecorator>
void
stream<NextLayer>::
accept_ex(
    http::request<Body,
        http::basic_fields<Allocator>> const& req,
    ResponseDecorator const& decorator)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(detail::is_ResponseDecorator<
        ResponseDecorator>::value,
            "ResponseDecorator requirements not met");
    error_code ec;
    accept_ex(req, decorator, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer>
template<class Body, class Allocator>
void
stream<NextLayer>::
accept(
    http::request<Body,
        http::basic_fields<Allocator>> const& req,
    error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    reset();
    do_accept(req, &default_decorate_res, ec);
}

template<class NextLayer>
template<
    class Body, class Allocator,
    class ResponseDecorator>
void
stream<NextLayer>::
accept_ex(
    http::request<Body,
        http::basic_fields<Allocator>> const& req,
    ResponseDecorator const& decorator,
    error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(detail::is_ResponseDecorator<
        ResponseDecorator>::value,
            "ResponseDecorator requirements not met");
    reset();
    do_accept(req, decorator, ec);
}

//------------------------------------------------------------------------------

template<class NextLayer>
template<
    class AcceptHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    AcceptHandler, void(error_code))
stream<NextLayer>::
async_accept(
    AcceptHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    boost::asio::async_completion<AcceptHandler,
        void(error_code)> init{handler};
    reset();
    accept_op<
        decltype(&default_decorate_res),
        BOOST_ASIO_HANDLER_TYPE(
            AcceptHandler, void(error_code))>{
                init.completion_handler,
                *this,
                &default_decorate_res}({});
    return init.result.get();
}

template<class NextLayer>
template<
    class ResponseDecorator,
    class AcceptHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    AcceptHandler, void(error_code))
stream<NextLayer>::
async_accept_ex(
    ResponseDecorator const& decorator,
    AcceptHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    static_assert(detail::is_ResponseDecorator<
        ResponseDecorator>::value,
            "ResponseDecorator requirements not met");
    boost::asio::async_completion<AcceptHandler,
        void(error_code)> init{handler};
    reset();
    accept_op<
        ResponseDecorator,
        BOOST_ASIO_HANDLER_TYPE(
            AcceptHandler, void(error_code))>{
                init.completion_handler,
                *this,
                decorator}({});
    return init.result.get();
}

template<class NextLayer>
template<
    class ConstBufferSequence,
    class AcceptHandler>
typename std::enable_if<
    ! http::detail::is_header<ConstBufferSequence>::value,
    BOOST_ASIO_INITFN_RESULT_TYPE(
        AcceptHandler, void(error_code))>::type
stream<NextLayer>::
async_accept(
    ConstBufferSequence const& buffers,
    AcceptHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    boost::asio::async_completion<AcceptHandler,
        void(error_code)> init{handler};
    reset();
    accept_op<
        decltype(&default_decorate_res),
        BOOST_ASIO_HANDLER_TYPE(
            AcceptHandler, void(error_code))>{
                init.completion_handler,
                *this,
                &default_decorate_res}.run(buffers);
    return init.result.get();
}

template<class NextLayer>
template<
    class ConstBufferSequence,
    class ResponseDecorator,
    class AcceptHandler>
typename std::enable_if<
    ! http::detail::is_header<ConstBufferSequence>::value,
    BOOST_ASIO_INITFN_RESULT_TYPE(
        AcceptHandler, void(error_code))>::type
stream<NextLayer>::
async_accept_ex(
    ConstBufferSequence const& buffers,
    ResponseDecorator const& decorator,
    AcceptHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    static_assert(detail::is_ResponseDecorator<
        ResponseDecorator>::value,
            "ResponseDecorator requirements not met");
    boost::asio::async_completion<AcceptHandler,
        void(error_code)> init{handler};
    reset();
    accept_op<
        ResponseDecorator,
        BOOST_ASIO_HANDLER_TYPE(
            AcceptHandler, void(error_code))>{
                init.completion_handler,
                *this,
                decorator}.run(buffers);
    return init.result.get();
}

template<class NextLayer>
template<
    class Body, class Allocator,
    class AcceptHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    AcceptHandler, void(error_code))
stream<NextLayer>::
async_accept(
    http::request<Body, http::basic_fields<Allocator>> const& req,
    AcceptHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    boost::asio::async_completion<AcceptHandler,
        void(error_code)> init{handler};
    reset();
    using boost::asio::asio_handler_is_continuation;
    response_op<
        BOOST_ASIO_HANDLER_TYPE(
            AcceptHandler, void(error_code))>{
                init.completion_handler,
                *this,
                req,
                &default_decorate_res}();
    return init.result.get();
}

template<class NextLayer>
template<
    class Body, class Allocator,
    class ResponseDecorator,
    class AcceptHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    AcceptHandler, void(error_code))
stream<NextLayer>::
async_accept_ex(
    http::request<Body, http::basic_fields<Allocator>> const& req,
    ResponseDecorator const& decorator,
    AcceptHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    static_assert(detail::is_ResponseDecorator<
        ResponseDecorator>::value,
            "ResponseDecorator requirements not met");
    boost::asio::async_completion<AcceptHandler,
        void(error_code)> init{handler};
    reset();
    using boost::asio::asio_handler_is_continuation;
    response_op<
        BOOST_ASIO_HANDLER_TYPE(
            AcceptHandler, void(error_code))>{
                init.completion_handler,
                *this,
                req,
                decorator}();
    return init.result.get();
}

//------------------------------------------------------------------------------

template<class NextLayer>
template<class Decorator>
void
stream<NextLayer>::
do_accept(
    Decorator const& decorator,
    error_code& ec)
{
    http::request_parser<http::empty_body> p;
    http::read(next_layer(), rd_buf_, p, ec);
    if(ec == http::error::end_of_stream)
        ec = error::closed;
    if(ec)
        return;
    do_accept(p.get(), decorator, ec);
}

template<class NextLayer>
template<class Body, class Allocator,
    class Decorator>
void
stream<NextLayer>::
do_accept(
    http::request<Body,http::basic_fields<Allocator>> const& req,
    Decorator const& decorator,
    error_code& ec)
{
    auto const res = build_response(req, decorator);
    http::write(stream_, res, ec);
    if(ec)
        return;
    if(res.result() != http::status::switching_protocols)
    {
        ec = error::handshake_failed;
        // VFALCO TODO Respect keep alive setting, perform
        //             teardown if Connection: close.
        return;
    }
    pmd_read(pmd_config_, res);
    open(role_type::server);
}

} // websocket
} // beast
} // boost

#endif
