//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_PING_IPP
#define BOOST_BEAST_WEBSOCKET_IMPL_PING_IPP

#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/websocket/detail/frame.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/post.hpp>
#include <boost/throw_exception.hpp>
#include <memory>

namespace boost {
namespace beast {
namespace websocket {

/*
    This composed operation handles sending ping and pong frames.
    It only sends the frames it does not make attempts to read
    any frame data.
*/
template<class NextLayer>
template<class Handler>
class stream<NextLayer>::ping_op
    : public boost::asio::coroutine
{
    struct state
    {
        stream<NextLayer>& ws;
        detail::frame_buffer fb;
        token tok;

        state(
            Handler&,
            stream<NextLayer>& ws_,
            detail::opcode op,
            ping_data const& payload)
            : ws(ws_)
            , tok(ws.tok_.unique())
        {
            // Serialize the control frame
            ws.template write_ping<
                flat_static_buffer_base>(
                    fb, op, payload);
        }
    };

    handler_ptr<state, Handler> d_;

public:
    ping_op(ping_op&&) = default;
    ping_op(ping_op const&) = default;

    template<class DeducedHandler>
    ping_op(
        DeducedHandler&& h,
        stream<NextLayer>& ws,
        detail::opcode op,
        ping_data const& payload)
        : d_(std::forward<DeducedHandler>(h),
            ws, op, payload)
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
    bool asio_handler_is_continuation(ping_op* op)
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
ping_op<Handler>::
operator()(error_code ec, std::size_t)
{
    auto& d = *d_;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        // Maybe suspend
        if(! d.ws.wr_block_)
        {
            // Acquire the write block
            d.ws.wr_block_ = d.tok;

            // Make sure the stream is open
            if(! d.ws.check_open(ec))
            {
                BOOST_ASIO_CORO_YIELD
                boost::asio::post(
                    d.ws.get_executor(),
                    bind_handler(std::move(*this), ec));
                goto upcall;
            }
        }
        else
        {
            // Suspend
            BOOST_ASSERT(d.ws.wr_block_ != d.tok);
            BOOST_ASIO_CORO_YIELD
            d.ws.paused_ping_.emplace(std::move(*this));

            // Acquire the write block
            BOOST_ASSERT(! d.ws.wr_block_);
            d.ws.wr_block_ = d.tok;

            // Resume
            BOOST_ASIO_CORO_YIELD
            boost::asio::post(
                d.ws.get_executor(), std::move(*this));
            BOOST_ASSERT(d.ws.wr_block_ == d.tok);

            // Make sure the stream is open
            if(! d.ws.check_open(ec))
                goto upcall;
        }

        // Send ping frame
        BOOST_ASIO_CORO_YIELD
        boost::asio::async_write(d.ws.stream_,
            d.fb.data(), std::move(*this));
        if(! d.ws.check_ok(ec))
            goto upcall;

    upcall:
        BOOST_ASSERT(d.ws.wr_block_ == d.tok);
        d.ws.wr_block_.reset();
        d.ws.paused_close_.maybe_invoke() ||
            d.ws.paused_rd_.maybe_invoke() ||
            d.ws.paused_wr_.maybe_invoke();
        d_.invoke(ec);
    }
}

//------------------------------------------------------------------------------

template<class NextLayer>
void
stream<NextLayer>::
ping(ping_data const& payload)
{
    error_code ec;
    ping(payload, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer>
void
stream<NextLayer>::
ping(ping_data const& payload, error_code& ec)
{
    // Make sure the stream is open
    if(! check_open(ec))
        return;
    detail::frame_buffer fb;
    write_ping<flat_static_buffer_base>(
        fb, detail::opcode::ping, payload);
    boost::asio::write(stream_, fb.data(), ec);
    if(! check_ok(ec))
        return;
}

template<class NextLayer>
void
stream<NextLayer>::
pong(ping_data const& payload)
{
    error_code ec;
    pong(payload, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer>
void
stream<NextLayer>::
pong(ping_data const& payload, error_code& ec)
{
    // Make sure the stream is open
    if(! check_open(ec))
        return;
    detail::frame_buffer fb;
    write_ping<flat_static_buffer_base>(
        fb, detail::opcode::pong, payload);
    boost::asio::write(stream_, fb.data(), ec);
    if(! check_ok(ec))
        return;
}

template<class NextLayer>
template<class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code))
stream<NextLayer>::
async_ping(ping_data const& payload, WriteHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    boost::asio::async_completion<WriteHandler,
        void(error_code)> init{handler};
    ping_op<BOOST_ASIO_HANDLER_TYPE(
        WriteHandler, void(error_code))>{
            init.completion_handler, *this,
                detail::opcode::ping, payload}();
    return init.result.get();
}

template<class NextLayer>
template<class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code))
stream<NextLayer>::
async_pong(ping_data const& payload, WriteHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    boost::asio::async_completion<WriteHandler,
        void(error_code)> init{handler};
    ping_op<BOOST_ASIO_HANDLER_TYPE(
        WriteHandler, void(error_code))>{
            init.completion_handler, *this,
                detail::opcode::pong, payload}();
    return init.result.get();
}

} // websocket
} // beast
} // boost

#endif
