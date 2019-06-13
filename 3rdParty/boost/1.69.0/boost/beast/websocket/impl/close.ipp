//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_CLOSE_IPP
#define BOOST_BEAST_WEBSOCKET_IMPL_CLOSE_IPP

#include <boost/beast/websocket/teardown.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/asio/post.hpp>
#include <boost/throw_exception.hpp>
#include <memory>

namespace boost {
namespace beast {
namespace websocket {

/*  Close the WebSocket Connection

    This composed operation sends the close frame if it hasn't already
    been sent, then reads and discards frames until receiving a close
    frame. Finally it invokes the teardown operation to shut down the
    underlying connection.
*/
template<class NextLayer, bool deflateSupported>
template<class Handler>
class stream<NextLayer, deflateSupported>::close_op
    : public boost::asio::coroutine
{
    struct state
    {
        stream<NextLayer, deflateSupported>& ws;
        boost::asio::executor_work_guard<decltype(std::declval<
            stream<NextLayer, deflateSupported>&>().get_executor())> wg;
        detail::frame_buffer fb;
        error_code ev;
        bool cont = false;

        state(
            Handler const&,
            stream<NextLayer, deflateSupported>& ws_,
            close_reason const& cr)
            : ws(ws_)
            , wg(ws.get_executor())
        {
            // Serialize the close frame
            ws.template write_close<
                flat_static_buffer_base>(fb, cr);
        }
    };

    handler_ptr<state, Handler> d_;

public:
    static constexpr int id = 4; // for soft_mutex

    close_op(close_op&&) = default;
    close_op(close_op const&) = delete;

    template<class DeducedHandler>
    close_op(
        DeducedHandler&& h,
        stream<NextLayer, deflateSupported>& ws,
        close_reason const& cr)
        : d_(std::forward<DeducedHandler>(h), ws, cr)
    {
    }

    using allocator_type =
        boost::asio::associated_allocator_t<Handler>;

    allocator_type
    get_allocator() const noexcept
    {
        return (boost::asio::get_associated_allocator)(d_.handler());
    }

    using executor_type = boost::asio::associated_executor_t<
        Handler, decltype(std::declval<stream<NextLayer, deflateSupported>&>().get_executor())>;

    executor_type
    get_executor() const noexcept
    {
        return (boost::asio::get_associated_executor)(
            d_.handler(), d_->ws.get_executor());
    }

    void
    operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0,
        bool cont = true);

    friend
    bool asio_handler_is_continuation(close_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return op->d_->cont || asio_handler_is_continuation(
            std::addressof(op->d_.handler()));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, close_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f,
            std::addressof(op->d_.handler()));
    }
};

template<class NextLayer, bool deflateSupported>
template<class Handler>
void
stream<NextLayer, deflateSupported>::
close_op<Handler>::
operator()(
    error_code ec,
    std::size_t bytes_transferred,
    bool cont)
{
    using beast::detail::clamp;
    auto& d = *d_;
    d.cont = cont;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        // Attempt to acquire write block
        if(! d.ws.wr_block_.try_lock(this))
        {
            // Suspend
            BOOST_ASIO_CORO_YIELD
            d.ws.paused_close_.emplace(std::move(*this));

            // Acquire the write block
            d.ws.wr_block_.lock(this);

            // Resume
            BOOST_ASIO_CORO_YIELD
            boost::asio::post(
                d.ws.get_executor(), std::move(*this));
            BOOST_ASSERT(d.ws.wr_block_.is_locked(this));
        }

        // Make sure the stream is open
        if(! d.ws.check_open(ec))
            goto upcall;

        // Can't call close twice
        BOOST_ASSERT(! d.ws.wr_close_);

        // Change status to closing
        BOOST_ASSERT(d.ws.status_ == status::open);
        d.ws.status_ = status::closing;

        // Send close frame
        d.ws.wr_close_ = true;
        BOOST_ASIO_CORO_YIELD
        boost::asio::async_write(d.ws.stream_,
            d.fb.data(), std::move(*this));
        if(! d.ws.check_ok(ec))
            goto upcall;

        if(d.ws.rd_close_)
        {
            // This happens when the read_op gets a close frame
            // at the same time close_op is sending the close frame.
            // The read_op will be suspended on the write block.
            goto teardown;
        }
        
        // Maybe suspend
        if(! d.ws.rd_block_.try_lock(this))
        {
            // Suspend
            BOOST_ASIO_CORO_YIELD
            d.ws.paused_r_close_.emplace(std::move(*this));

            // Acquire the read block
            d.ws.rd_block_.lock(this);

            // Resume
            BOOST_ASIO_CORO_YIELD
            boost::asio::post(
                d.ws.get_executor(), std::move(*this));
            BOOST_ASSERT(d.ws.rd_block_.is_locked(this));

            // Make sure the stream is open
            BOOST_ASSERT(d.ws.status_ != status::open);
            BOOST_ASSERT(d.ws.status_ != status::closed);
            if( d.ws.status_ == status::failed)
                goto upcall;

            BOOST_ASSERT(! d.ws.rd_close_);
        }

        // Drain
        if(d.ws.rd_remain_ > 0)
            goto read_payload;
        for(;;)
        {
            // Read frame header
            while(! d.ws.parse_fh(
                d.ws.rd_fh_, d.ws.rd_buf_, d.ev))
            {
                if(d.ev)
                    goto teardown;
                BOOST_ASIO_CORO_YIELD
                d.ws.stream_.async_read_some(
                    d.ws.rd_buf_.prepare(read_size(d.ws.rd_buf_,
                        d.ws.rd_buf_.max_size())),
                            std::move(*this));
                if(! d.ws.check_ok(ec))
                    goto upcall;
                d.ws.rd_buf_.commit(bytes_transferred);
            }
            if(detail::is_control(d.ws.rd_fh_.op))
            {
                // Process control frame
                if(d.ws.rd_fh_.op == detail::opcode::close)
                {
                    BOOST_ASSERT(! d.ws.rd_close_);
                    d.ws.rd_close_ = true;
                    auto const mb = buffers_prefix(
                        clamp(d.ws.rd_fh_.len),
                        d.ws.rd_buf_.mutable_data());
                    if(d.ws.rd_fh_.len > 0 && d.ws.rd_fh_.mask)
                        detail::mask_inplace(mb, d.ws.rd_key_);
                    detail::read_close(d.ws.cr_, mb, d.ev);
                    if(d.ev)
                        goto teardown;
                    d.ws.rd_buf_.consume(clamp(d.ws.rd_fh_.len));
                    goto teardown;
                }
                d.ws.rd_buf_.consume(clamp(d.ws.rd_fh_.len));
            }
            else
            {
            read_payload:
                while(d.ws.rd_buf_.size() < d.ws.rd_remain_)
                {
                    d.ws.rd_remain_ -= d.ws.rd_buf_.size();
                    d.ws.rd_buf_.consume(d.ws.rd_buf_.size());
                    BOOST_ASIO_CORO_YIELD
                    d.ws.stream_.async_read_some(
                        d.ws.rd_buf_.prepare(read_size(d.ws.rd_buf_,
                            d.ws.rd_buf_.max_size())),
                                std::move(*this));
                    if(! d.ws.check_ok(ec))
                        goto upcall;
                    d.ws.rd_buf_.commit(bytes_transferred);
                }
                BOOST_ASSERT(d.ws.rd_buf_.size() >= d.ws.rd_remain_);
                d.ws.rd_buf_.consume(clamp(d.ws.rd_remain_));
                d.ws.rd_remain_ = 0;
            }
        }

    teardown:
        // Teardown
        BOOST_ASSERT(d.ws.wr_block_.is_locked(this));
        using beast::websocket::async_teardown;
        BOOST_ASIO_CORO_YIELD
        async_teardown(d.ws.role_,
            d.ws.stream_, std::move(*this));
        BOOST_ASSERT(d.ws.wr_block_.is_locked(this));
        if(ec == boost::asio::error::eof)
        {
            // Rationale:
            // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
            ec.assign(0, ec.category());
        }
        if(! ec)
            ec = d.ev;
        if(ec)
            d.ws.status_ = status::failed;
        else
            d.ws.status_ = status::closed;
        d.ws.close();

    upcall:
        BOOST_ASSERT(d.ws.wr_block_.is_locked(this));
        d.ws.wr_block_.unlock(this);
        if(d.ws.rd_block_.try_unlock(this))
            d.ws.paused_r_rd_.maybe_invoke();
        d.ws.paused_rd_.maybe_invoke() ||
            d.ws.paused_ping_.maybe_invoke() ||
            d.ws.paused_wr_.maybe_invoke();
        if(! d.cont)
        {
            BOOST_ASIO_CORO_YIELD
            boost::asio::post(
                d.ws.get_executor(),
                bind_handler(std::move(*this), ec));
        }
        {
            auto wg = std::move(d.wg);
            d_.invoke(ec);
        }
    }
}

//------------------------------------------------------------------------------

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
close(close_reason const& cr)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    error_code ec;
    close(cr, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
}

template<class NextLayer, bool deflateSupported>
void
stream<NextLayer, deflateSupported>::
close(close_reason const& cr, error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    using beast::detail::clamp;
    ec.assign(0, ec.category());
    // Make sure the stream is open
    if(! check_open(ec))
        return;
    // If rd_close_ is set then we already sent a close
    BOOST_ASSERT(! rd_close_);
    BOOST_ASSERT(! wr_close_);
    wr_close_ = true;
    {
        detail::frame_buffer fb;
        write_close<flat_static_buffer_base>(fb, cr);
        boost::asio::write(stream_, fb.data(), ec);
    }
    if(! check_ok(ec))
        return;
    status_ = status::closing;
    error_code result;
    // Drain the connection
    if(rd_remain_ > 0)
        goto read_payload;
    for(;;)
    {
        // Read frame header
        while(! parse_fh(rd_fh_, rd_buf_, result))
        {
            if(result)
                return do_fail(
                    close_code::none, result, ec);
            auto const bytes_transferred =
                stream_.read_some(
                    rd_buf_.prepare(read_size(rd_buf_,
                        rd_buf_.max_size())), ec);
            if(! check_ok(ec))
                return;
            rd_buf_.commit(bytes_transferred);
        }
        if(detail::is_control(rd_fh_.op))
        {
            // Process control frame
            if(rd_fh_.op == detail::opcode::close)
            {
                BOOST_ASSERT(! rd_close_);
                rd_close_ = true;
                auto const mb = buffers_prefix(
                    clamp(rd_fh_.len),
                    rd_buf_.mutable_data());
                if(rd_fh_.len > 0 && rd_fh_.mask)
                    detail::mask_inplace(mb, rd_key_);
                detail::read_close(cr_, mb, result);
                if(result)
                {
                    // Protocol violation
                    return do_fail(
                        close_code::none, result, ec);
                }
                rd_buf_.consume(clamp(rd_fh_.len));
                break;
            }
            rd_buf_.consume(clamp(rd_fh_.len));
        }
        else
        {
        read_payload:
            while(rd_buf_.size() < rd_remain_)
            {
                rd_remain_ -= rd_buf_.size();
                rd_buf_.consume(rd_buf_.size());
                auto const bytes_transferred =
                    stream_.read_some(
                        rd_buf_.prepare(read_size(rd_buf_,
                            rd_buf_.max_size())), ec);
                if(! check_ok(ec))
                    return;
                rd_buf_.commit(bytes_transferred);
            }
            BOOST_ASSERT(rd_buf_.size() >= rd_remain_);
            rd_buf_.consume(clamp(rd_remain_));
            rd_remain_ = 0;
        }
    }
    // _Close the WebSocket Connection_
    do_fail(close_code::none, error::closed, ec);
    if(ec == error::closed)
        ec.assign(0, ec.category());
}

template<class NextLayer, bool deflateSupported>
template<class CloseHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    CloseHandler, void(error_code))
stream<NextLayer, deflateSupported>::
async_close(close_reason const& cr, CloseHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        CloseHandler, void(error_code));
    close_op<BOOST_ASIO_HANDLER_TYPE(
        CloseHandler, void(error_code))>{
            std::move(init.completion_handler), *this, cr}(
                {}, 0, false);
    return init.result.get();
}

} // websocket
} // beast
} // boost

#endif
