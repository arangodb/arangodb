//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_READ_IPP
#define BOOST_BEAST_WEBSOCKET_IMPL_READ_IPP

#include <boost/beast/websocket/teardown.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/core/detail/clamp.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/post.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/optional.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <limits>
#include <memory>

namespace boost {
namespace beast {
namespace websocket {

/*  Read some message frame data.

    Also reads and handles control frames.
*/
template<class NextLayer>
template<
    class MutableBufferSequence,
    class Handler>
class stream<NextLayer>::read_some_op
    : public boost::asio::coroutine
{
    Handler h_;
    stream<NextLayer>& ws_;
    MutableBufferSequence bs_;
    buffers_suffix<MutableBufferSequence> cb_;
    std::size_t bytes_written_ = 0;
    error_code ev_;
    token tok_;
    close_code code_;
    bool did_read_ = false;
    bool cont_ = false;

public:
    read_some_op(read_some_op&&) = default;
    read_some_op(read_some_op const&) = default;

    template<class DeducedHandler>
    read_some_op(
        DeducedHandler&& h,
        stream<NextLayer>& ws,
        MutableBufferSequence const& bs)
        : h_(std::forward<DeducedHandler>(h))
        , ws_(ws)
        , bs_(bs)
        , cb_(bs)
        , tok_(ws_.tok_.unique())
        , code_(close_code::none)
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
        Handler, decltype(std::declval<stream<NextLayer>&>().get_executor())>;

    executor_type
    get_executor() const noexcept
    {
        return boost::asio::get_associated_executor(
            h_, ws_.get_executor());
    }

    Handler&
    handler()
    {
        return h_;
    }

    void operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0,
        bool cont = true);

    friend
    bool asio_handler_is_continuation(read_some_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return op->cont_ || asio_handler_is_continuation(
            std::addressof(op->h_));
    }
};

template<class NextLayer>
template<class MutableBufferSequence, class Handler>
void
stream<NextLayer>::
read_some_op<MutableBufferSequence, Handler>::
operator()(
    error_code ec,
    std::size_t bytes_transferred,
    bool cont)
{
    using beast::detail::clamp;
    using boost::asio::buffer;
    using boost::asio::buffer_size;
    close_code code{};
    cont_ = cont;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        // Maybe suspend
    do_maybe_suspend:
        if(! ws_.rd_block_)
        {
            // Acquire the read block
            ws_.rd_block_ = tok_;

            // Make sure the stream is not closed
            if( ws_.status_ == status::closed ||
                ws_.status_ == status::failed)
            {
                ec = boost::asio::error::operation_aborted;
                goto upcall;
            }
        }
        else
        {
        do_suspend:
            // Suspend
            BOOST_ASSERT(ws_.rd_block_ != tok_);
            BOOST_ASIO_CORO_YIELD
            ws_.paused_r_rd_.save(std::move(*this));

            // Acquire the read block
            BOOST_ASSERT(! ws_.rd_block_);
            ws_.rd_block_ = tok_;

            // Resume
            BOOST_ASIO_CORO_YIELD
            boost::asio::post(
                ws_.get_executor(), std::move(*this));
            BOOST_ASSERT(ws_.rd_block_ == tok_);

            // The only way to get read blocked is if
            // a `close_op` wrote a close frame
            BOOST_ASSERT(ws_.wr_close_);
            BOOST_ASSERT(ws_.status_ != status::open);
            ec = boost::asio::error::operation_aborted;
            goto upcall;
        }

        // if status_ == status::closing, we want to suspend
        // the read operation until the close completes,
        // then finish the read with operation_aborted.

    loop:
        BOOST_ASSERT(ws_.rd_block_ == tok_);
        // See if we need to read a frame header. This
        // condition is structured to give the decompressor
        // a chance to emit the final empty deflate block
        //
        if(ws_.rd_remain_ == 0 &&
            (! ws_.rd_fh_.fin || ws_.rd_done_))
        {
            // Read frame header
            while(! ws_.parse_fh(
                ws_.rd_fh_, ws_.rd_buf_, code))
            {
                if(code != close_code::none)
                {
                    // _Fail the WebSocket Connection_
                    code_ = code;
                    ev_ = error::failed;
                    goto close;
                }
                BOOST_ASSERT(ws_.rd_block_ == tok_);
                BOOST_ASIO_CORO_YIELD
                ws_.stream_.async_read_some(
                    ws_.rd_buf_.prepare(read_size(
                        ws_.rd_buf_, ws_.rd_buf_.max_size())),
                            std::move(*this));
                BOOST_ASSERT(ws_.rd_block_ == tok_);
                if(! ws_.check_ok(ec))
                    goto upcall;
                ws_.rd_buf_.commit(bytes_transferred);

                // Allow a close operation
                // to acquire the read block
                BOOST_ASSERT(ws_.rd_block_ == tok_);
                ws_.rd_block_.reset();
                if( ws_.paused_r_close_.maybe_invoke())
                {
                    // Suspend
                    BOOST_ASSERT(ws_.rd_block_);
                    goto do_suspend;
                }
                // Acquire read block
                ws_.rd_block_ = tok_;
            }
            // Immediately apply the mask to the portion
            // of the buffer holding payload data.
            if(ws_.rd_fh_.len > 0 && ws_.rd_fh_.mask)
                detail::mask_inplace(buffers_prefix(
                    clamp(ws_.rd_fh_.len),
                        ws_.rd_buf_.data()),
                            ws_.rd_key_);
            if(detail::is_control(ws_.rd_fh_.op))
            {
                // Clear this otherwise the next
                // frame will be considered final.
                ws_.rd_fh_.fin = false;

                // Handle ping frame
                if(ws_.rd_fh_.op == detail::opcode::ping)
                {
                    {
                        auto const b = buffers_prefix(
                            clamp(ws_.rd_fh_.len),
                                ws_.rd_buf_.data());
                        auto const len = buffer_size(b);
                        BOOST_ASSERT(len == ws_.rd_fh_.len);
                        ping_data payload;
                        detail::read_ping(payload, b);
                        ws_.rd_buf_.consume(len);
                        // Ignore ping when closing
                        if(ws_.status_ == status::closing)
                            goto loop;
                        if(ws_.ctrl_cb_)
                            ws_.ctrl_cb_(frame_type::ping, payload);
                        ws_.rd_fb_.reset();
                        ws_.template write_ping<
                            flat_static_buffer_base>(ws_.rd_fb_,
                                detail::opcode::pong, payload);
                    }

                    //BOOST_ASSERT(! ws_.paused_r_close_);

                    // Allow a close operation
                    // to acquire the read block
                    BOOST_ASSERT(ws_.rd_block_ == tok_);
                    ws_.rd_block_.reset();
                    ws_.paused_r_close_.maybe_invoke();

                    // Maybe suspend
                    if(! ws_.wr_block_)
                    {
                        // Acquire the write block
                        ws_.wr_block_ = tok_;
                    }
                    else
                    {
                        // Suspend
                        BOOST_ASSERT(ws_.wr_block_ != tok_);
                        BOOST_ASIO_CORO_YIELD
                        ws_.paused_rd_.save(std::move(*this));

                        // Acquire the write block
                        BOOST_ASSERT(! ws_.wr_block_);
                        ws_.wr_block_ = tok_;

                        // Resume
                        BOOST_ASIO_CORO_YIELD
                        boost::asio::post(
                            ws_.get_executor(), std::move(*this));
                        BOOST_ASSERT(ws_.wr_block_ == tok_);

                        // Make sure the stream is open
                        if(! ws_.check_open(ec))
                            goto upcall;
                    }

                    // Send pong
                    BOOST_ASSERT(ws_.wr_block_ == tok_);
                    BOOST_ASIO_CORO_YIELD
                    boost::asio::async_write(ws_.stream_,
                        ws_.rd_fb_.data(), std::move(*this));
                    BOOST_ASSERT(ws_.wr_block_ == tok_);
                    if(! ws_.check_ok(ec))
                        goto upcall;
                    ws_.wr_block_.reset();
                    ws_.paused_close_.maybe_invoke() ||
                        ws_.paused_ping_.maybe_invoke() ||
                        ws_.paused_wr_.maybe_invoke();
                    goto do_maybe_suspend;
                }
                // Handle pong frame
                if(ws_.rd_fh_.op == detail::opcode::pong)
                {
                    auto const cb = buffers_prefix(clamp(
                        ws_.rd_fh_.len), ws_.rd_buf_.data());
                    auto const len = buffer_size(cb);
                    BOOST_ASSERT(len == ws_.rd_fh_.len);
                    code = close_code::none;
                    ping_data payload;
                    detail::read_ping(payload, cb);
                    ws_.rd_buf_.consume(len);
                    // Ignore pong when closing
                    if(! ws_.wr_close_ && ws_.ctrl_cb_)
                        ws_.ctrl_cb_(frame_type::pong, payload);
                    goto loop;
                }
                // Handle close frame
                BOOST_ASSERT(ws_.rd_fh_.op == detail::opcode::close);
                {
                    auto const cb = buffers_prefix(clamp(
                        ws_.rd_fh_.len), ws_.rd_buf_.data());
                    auto const len = buffer_size(cb);
                    BOOST_ASSERT(len == ws_.rd_fh_.len);
                    BOOST_ASSERT(! ws_.rd_close_);
                    ws_.rd_close_ = true;
                    close_reason cr;
                    detail::read_close(cr, cb, code);
                    if(code != close_code::none)
                    {
                        // _Fail the WebSocket Connection_
                        code_ = code;
                        ev_ = error::failed;
                        goto close;
                    }
                    ws_.cr_ = cr;
                    ws_.rd_buf_.consume(len);
                    if(ws_.ctrl_cb_)
                        ws_.ctrl_cb_(frame_type::close,
                            ws_.cr_.reason);
                    // See if we are already closing
                    if(ws_.status_ == status::closing)
                    {
                        // _Close the WebSocket Connection_
                        BOOST_ASSERT(ws_.wr_close_);
                        code_ = close_code::none;
                        ev_ = error::closed;
                        goto close;
                    }
                    // _Start the WebSocket Closing Handshake_
                    code_ = cr.code == close_code::none ?
                        close_code::normal :
                        static_cast<close_code>(cr.code);
                    ev_ = error::closed;
                    goto close;
                }
            }
            if(ws_.rd_fh_.len == 0 && ! ws_.rd_fh_.fin)
            {
                // Empty non-final frame
                goto loop;
            }
            ws_.rd_done_ = false;
        }
        if(! ws_.pmd_ || ! ws_.pmd_->rd_set)
        {
            if(ws_.rd_remain_ > 0)
            {
                if(ws_.rd_buf_.size() == 0 && ws_.rd_buf_.max_size() >
                    (std::min)(clamp(ws_.rd_remain_),
                        buffer_size(cb_)))
                {
                    // Fill the read buffer first, otherwise we
                    // get fewer bytes at the cost of one I/O.
                    BOOST_ASIO_CORO_YIELD
                    ws_.stream_.async_read_some(
                        ws_.rd_buf_.prepare(read_size(
                            ws_.rd_buf_, ws_.rd_buf_.max_size())),
                                std::move(*this));
                    if(! ws_.check_ok(ec))
                        goto upcall;
                    ws_.rd_buf_.commit(bytes_transferred);
                    if(ws_.rd_fh_.mask)
                        detail::mask_inplace(buffers_prefix(clamp(
                            ws_.rd_remain_), ws_.rd_buf_.data()),
                                ws_.rd_key_);
                }
                if(ws_.rd_buf_.size() > 0)
                {
                    // Copy from the read buffer.
                    // The mask was already applied.
                    bytes_transferred = buffer_copy(cb_,
                        ws_.rd_buf_.data(), clamp(ws_.rd_remain_));
                    auto const mb = buffers_prefix(
                        bytes_transferred, cb_);
                    ws_.rd_remain_ -= bytes_transferred;
                    if(ws_.rd_op_ == detail::opcode::text)
                    {
                        if(! ws_.rd_utf8_.write(mb) ||
                            (ws_.rd_remain_ == 0 && ws_.rd_fh_.fin &&
                                ! ws_.rd_utf8_.finish()))
                        {
                            // _Fail the WebSocket Connection_
                            code_ = close_code::bad_payload;
                            ev_ = error::failed;
                            goto close;
                        }
                    }
                    bytes_written_ += bytes_transferred;
                    ws_.rd_size_ += bytes_transferred;
                    ws_.rd_buf_.consume(bytes_transferred);
                }
                else
                {
                    // Read into caller's buffer
                    BOOST_ASSERT(ws_.rd_remain_ > 0);
                    BOOST_ASSERT(buffer_size(cb_) > 0);
                    BOOST_ASSERT(buffer_size(buffers_prefix(
                        clamp(ws_.rd_remain_), cb_)) > 0);
                    BOOST_ASIO_CORO_YIELD
                    ws_.stream_.async_read_some(buffers_prefix(
                        clamp(ws_.rd_remain_), cb_), std::move(*this));
                    if(! ws_.check_ok(ec))
                        goto upcall;
                    BOOST_ASSERT(bytes_transferred > 0);
                    auto const mb = buffers_prefix(
                        bytes_transferred, cb_);
                    ws_.rd_remain_ -= bytes_transferred;
                    if(ws_.rd_fh_.mask)
                        detail::mask_inplace(mb, ws_.rd_key_);
                    if(ws_.rd_op_ == detail::opcode::text)
                    {
                        if(! ws_.rd_utf8_.write(mb) ||
                            (ws_.rd_remain_ == 0 && ws_.rd_fh_.fin &&
                                ! ws_.rd_utf8_.finish()))
                        {
                            // _Fail the WebSocket Connection_
                            code_ = close_code::bad_payload;
                            ev_ = error::failed;
                            goto close;
                        }
                    }
                    bytes_written_ += bytes_transferred;
                    ws_.rd_size_ += bytes_transferred;
                }
            }
            ws_.rd_done_ = ws_.rd_remain_ == 0 && ws_.rd_fh_.fin;
        }
        else
        {
            // Read compressed message frame payload:
            // inflate even if rd_fh_.len == 0, otherwise we
            // never emit the end-of-stream deflate block.
            while(buffer_size(cb_) > 0)
            {
                if( ws_.rd_remain_ > 0 &&
                    ws_.rd_buf_.size() == 0 &&
                    ! did_read_)
                {
                    // read new
                    BOOST_ASIO_CORO_YIELD
                    ws_.stream_.async_read_some(
                        ws_.rd_buf_.prepare(read_size(
                            ws_.rd_buf_, ws_.rd_buf_.max_size())),
                                std::move(*this));
                    if(! ws_.check_ok(ec))
                        goto upcall;
                    BOOST_ASSERT(bytes_transferred > 0);
                    ws_.rd_buf_.commit(bytes_transferred);
                    if(ws_.rd_fh_.mask)
                        detail::mask_inplace(
                            buffers_prefix(clamp(ws_.rd_remain_),
                                ws_.rd_buf_.data()), ws_.rd_key_);
                    did_read_ = true;
                }
                zlib::z_params zs;
                {
                    auto const out = buffers_front(cb_);
                    zs.next_out = out.data();
                    zs.avail_out = out.size();
                    BOOST_ASSERT(zs.avail_out > 0);
                }
                if(ws_.rd_remain_ > 0)
                {
                    if(ws_.rd_buf_.size() > 0)
                    {
                        // use what's there
                        auto const in = buffers_prefix(
                            clamp(ws_.rd_remain_), buffers_front(
                                ws_.rd_buf_.data()));
                        zs.avail_in = in.size();
                        zs.next_in = in.data();
                    }
                    else
                    {
                        break;
                    }
                }
                else if(ws_.rd_fh_.fin)
                {
                    // append the empty block codes
                    static std::uint8_t constexpr
                        empty_block[4] = {
                            0x00, 0x00, 0xff, 0xff };
                    zs.next_in = empty_block;
                    zs.avail_in = sizeof(empty_block);
                    ws_.pmd_->zi.write(zs, zlib::Flush::sync, ec);
                    if(! ec)
                    {
                        // https://github.com/madler/zlib/issues/280
                        if(zs.total_out > 0)
                            ec = error::partial_deflate_block;
                    }
                    if(! ws_.check_ok(ec))
                        goto upcall;
                    if(
                        (ws_.role_ == role_type::client &&
                            ws_.pmd_config_.server_no_context_takeover) ||
                        (ws_.role_ == role_type::server &&
                            ws_.pmd_config_.client_no_context_takeover))
                        ws_.pmd_->zi.reset();
                    ws_.rd_done_ = true;
                    break;
                }
                else
                {
                    break;
                }
                ws_.pmd_->zi.write(zs, zlib::Flush::sync, ec);
                if(! ws_.check_ok(ec))
                    goto upcall;
                if(ws_.rd_msg_max_ && beast::detail::sum_exceeds(
                    ws_.rd_size_, zs.total_out, ws_.rd_msg_max_))
                {
                    // _Fail the WebSocket Connection_
                    code_ = close_code::too_big;
                    ev_ = error::failed;
                    goto close;
                }
                cb_.consume(zs.total_out);
                ws_.rd_size_ += zs.total_out;
                ws_.rd_remain_ -= zs.total_in;
                ws_.rd_buf_.consume(zs.total_in);
                bytes_written_ += zs.total_out;
            }
            if(ws_.rd_op_ == detail::opcode::text)
            {
                // check utf8
                if(! ws_.rd_utf8_.write(
                    buffers_prefix(bytes_written_, bs_)) || (
                        ws_.rd_done_ && ! ws_.rd_utf8_.finish()))
                {
                    // _Fail the WebSocket Connection_
                    code_ = close_code::bad_payload;
                    ev_ = error::failed;
                    goto close;
                }
            }
        }
        goto upcall;

    close:
        if(! ws_.wr_block_)
        {
            // Acquire the write block
            ws_.wr_block_ = tok_;

            // Make sure the stream is open
            BOOST_ASSERT(ws_.status_ == status::open);
        }
        else
        {
            // Suspend
            BOOST_ASSERT(ws_.wr_block_ != tok_);
            BOOST_ASIO_CORO_YIELD
            ws_.paused_rd_.save(std::move(*this));

            // Acquire the write block
            BOOST_ASSERT(! ws_.wr_block_);
            ws_.wr_block_ = tok_;

            // Resume
            BOOST_ASIO_CORO_YIELD
            boost::asio::post(
                ws_.get_executor(), std::move(*this));
            BOOST_ASSERT(ws_.wr_block_ == tok_);

            // Make sure the stream is open
            if(! ws_.check_open(ec))
                goto upcall;
        }

        // Set the status
        ws_.status_ = status::closing;

        if(! ws_.wr_close_)
        {
            ws_.wr_close_ = true;

            // Serialize close frame
            ws_.rd_fb_.reset();
            ws_.template write_close<
                flat_static_buffer_base>(
                    ws_.rd_fb_, code_);

            // Send close frame
            BOOST_ASSERT(ws_.wr_block_ == tok_);
            BOOST_ASIO_CORO_YIELD
            boost::asio::async_write(
                ws_.stream_, ws_.rd_fb_.data(),
                    std::move(*this));
            BOOST_ASSERT(ws_.wr_block_ == tok_);
            if(! ws_.check_ok(ec))
                goto upcall;
        }

        // Teardown
        using beast::websocket::async_teardown;
        BOOST_ASSERT(ws_.wr_block_ == tok_);
        BOOST_ASIO_CORO_YIELD
        async_teardown(ws_.role_,
            ws_.stream_, std::move(*this));
        BOOST_ASSERT(ws_.wr_block_ == tok_);
        if(ec == boost::asio::error::eof)
        {
            // Rationale:
            // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
            ec.assign(0, ec.category());
        }
        if(! ec)
            ec = ev_;
        if(ec && ec != error::closed)
            ws_.status_ = status::failed;
        else
            ws_.status_ = status::closed;
        ws_.close();

    upcall:
        if(ws_.rd_block_ == tok_)
            ws_.rd_block_.reset();
        ws_.paused_r_close_.maybe_invoke();
        if(ws_.wr_block_ == tok_)
        {
            ws_.wr_block_.reset();
            ws_.paused_close_.maybe_invoke() ||
                ws_.paused_ping_.maybe_invoke() ||
                ws_.paused_wr_.maybe_invoke();
        }
        if(! cont_)
            return boost::asio::post(
                ws_.stream_.get_executor(),
                bind_handler(std::move(h_),
                    ec, bytes_written_));
        h_(ec, bytes_written_);
    }
}

//------------------------------------------------------------------------------

template<class NextLayer>
template<
    class DynamicBuffer,
    class Handler>
class stream<NextLayer>::read_op
    : public boost::asio::coroutine
{
    Handler h_;
    stream<NextLayer>& ws_;
    DynamicBuffer& b_;
    std::size_t limit_;
    std::size_t bytes_written_ = 0;
    bool some_;

public:
    using allocator_type =
        boost::asio::associated_allocator_t<Handler>;

    read_op(read_op&&) = default;
    read_op(read_op const&) = default;

    template<class DeducedHandler>
    read_op(
        DeducedHandler&& h,
        stream<NextLayer>& ws,
        DynamicBuffer& b,
        std::size_t limit,
        bool some)
        : h_(std::forward<DeducedHandler>(h))
        , ws_(ws)
        , b_(b)
        , limit_(limit ? limit : (
            std::numeric_limits<std::size_t>::max)())
        , some_(some)
    {
    }

    allocator_type
    get_allocator() const noexcept
    {
        return boost::asio::get_associated_allocator(h_);
    }

    using executor_type = boost::asio::associated_executor_t<
        Handler, decltype(std::declval<stream<NextLayer>&>().get_executor())>;

    executor_type
    get_executor() const noexcept
    {
        return boost::asio::get_associated_executor(
            h_, ws_.get_executor());
    }

    void operator()(
        error_code ec = {},
        std::size_t bytes_transferred = 0);

    friend
    bool asio_handler_is_continuation(read_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->h_));
    }
};

template<class NextLayer>
template<class DynamicBuffer, class Handler>
void
stream<NextLayer>::
read_op<DynamicBuffer, Handler>::
operator()(
    error_code ec,
    std::size_t bytes_transferred)
{
    using beast::detail::clamp;
    using buffers_type = typename
        DynamicBuffer::mutable_buffers_type;
    boost::optional<buffers_type> mb;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        do
        {
            try
            {
                mb.emplace(b_.prepare(clamp(
                    ws_.read_size_hint(b_), limit_)));
            }
            catch(std::length_error const&)
            {
                ec = error::buffer_overflow;
            }
            if(ec)
            {
                BOOST_ASIO_CORO_YIELD
                boost::asio::post(
                    ws_.get_executor(),
                    bind_handler(std::move(*this),
                        error::buffer_overflow, 0));
                break;
            }
            BOOST_ASIO_CORO_YIELD
            read_some_op<buffers_type, read_op>{
                std::move(*this), ws_, *mb}(
                    {}, 0, false);
            if(ec)
                break;
            b_.commit(bytes_transferred);
            bytes_written_ += bytes_transferred;
        }
        while(! some_ && ! ws_.is_message_done());
        h_(ec, bytes_written_);
    }
}

//------------------------------------------------------------------------------

template<class NextLayer>
template<class DynamicBuffer>
std::size_t
stream<NextLayer>::
read(DynamicBuffer& buffer)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    error_code ec;
    auto const bytes_written = read(buffer, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_written;
}

template<class NextLayer>
template<class DynamicBuffer>
std::size_t
stream<NextLayer>::
read(DynamicBuffer& buffer, error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    std::size_t bytes_written = 0;
    do
    {
        bytes_written += read_some(buffer, 0, ec);
        if(ec)
            return bytes_written;
    }
    while(! is_message_done());
    return bytes_written;
}

template<class NextLayer>
template<class DynamicBuffer, class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
stream<NextLayer>::
async_read(DynamicBuffer& buffer, ReadHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    boost::asio::async_completion<
        ReadHandler, void(error_code, std::size_t)> init{handler};
    read_op<
        DynamicBuffer,
        BOOST_ASIO_HANDLER_TYPE(
            ReadHandler, void(error_code, std::size_t))>{
                init.completion_handler,
                *this,
                buffer,
                0,
                false}();
    return init.result.get();
}

//------------------------------------------------------------------------------

template<class NextLayer>
template<class DynamicBuffer>
std::size_t
stream<NextLayer>::
read_some(
    DynamicBuffer& buffer,
    std::size_t limit)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    error_code ec;
    auto const bytes_written =
        read_some(buffer, limit, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_written;
}

template<class NextLayer>
template<class DynamicBuffer>
std::size_t
stream<NextLayer>::
read_some(
    DynamicBuffer& buffer,
    std::size_t limit,
    error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    using beast::detail::clamp;
    if(! limit)
        limit = (std::numeric_limits<std::size_t>::max)();
    auto const size =
        clamp(read_size_hint(buffer), limit);
    BOOST_ASSERT(size > 0);
    boost::optional<typename
        DynamicBuffer::mutable_buffers_type> mb;
    try
    {
        mb.emplace(buffer.prepare(size));
    }
    catch(std::length_error const&)
    {
        ec = error::buffer_overflow;
        return 0;
    }
    auto const bytes_written = read_some(*mb, ec);
    buffer.commit(bytes_written);
    return bytes_written;
}

template<class NextLayer>
template<class DynamicBuffer, class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
stream<NextLayer>::
async_read_some(
    DynamicBuffer& buffer,
    std::size_t limit,
    ReadHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    boost::asio::async_completion<ReadHandler,
        void(error_code, std::size_t)> init{handler};
    read_op<
        DynamicBuffer,
        BOOST_ASIO_HANDLER_TYPE(
            ReadHandler, void(error_code, std::size_t))>{
                init.completion_handler,
                *this,
                buffer,
                limit,
                true}({}, 0);
    return init.result.get();
}

//------------------------------------------------------------------------------

template<class NextLayer>
template<class MutableBufferSequence>
std::size_t
stream<NextLayer>::
read_some(
    MutableBufferSequence const& buffers)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(boost::asio::is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    error_code ec;
    auto const bytes_written = read_some(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_written;
}

template<class NextLayer>
template<class MutableBufferSequence>
std::size_t
stream<NextLayer>::
read_some(
    MutableBufferSequence const& buffers,
    error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(boost::asio::is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    using beast::detail::clamp;
    using boost::asio::buffer;
    using boost::asio::buffer_size;
    close_code code{};
    std::size_t bytes_written = 0;
    ec.assign(0, ec.category());
    // Make sure the stream is open
    if(! check_open(ec))
        return 0;
loop:
    // See if we need to read a frame header. This
    // condition is structured to give the decompressor
    // a chance to emit the final empty deflate block
    //
    if(rd_remain_ == 0 && (! rd_fh_.fin || rd_done_))
    {
        // Read frame header
        while(! parse_fh(rd_fh_, rd_buf_, code))
        {
            if(code != close_code::none)
            {
                // _Fail the WebSocket Connection_
                do_fail(code, error::failed, ec);
                return bytes_written;
            }
            auto const bytes_transferred =
                stream_.read_some(
                    rd_buf_.prepare(read_size(
                        rd_buf_, rd_buf_.max_size())),
                    ec);
            if(! check_ok(ec))
                return bytes_written;
            rd_buf_.commit(bytes_transferred);
        }
        // Immediately apply the mask to the portion
        // of the buffer holding payload data.
        if(rd_fh_.len > 0 && rd_fh_.mask)
            detail::mask_inplace(buffers_prefix(
                clamp(rd_fh_.len), rd_buf_.data()),
                    rd_key_);
        if(detail::is_control(rd_fh_.op))
        {
            // Get control frame payload
            auto const b = buffers_prefix(
                clamp(rd_fh_.len), rd_buf_.data());
            auto const len = buffer_size(b);
            BOOST_ASSERT(len == rd_fh_.len);

            // Clear this otherwise the next
            // frame will be considered final.
            rd_fh_.fin = false;

            // Handle ping frame
            if(rd_fh_.op == detail::opcode::ping)
            {
                ping_data payload;
                detail::read_ping(payload, b);
                rd_buf_.consume(len);
                if(wr_close_)
                {
                    // Ignore ping when closing
                    goto loop;
                }
                if(ctrl_cb_)
                    ctrl_cb_(frame_type::ping, payload);
                detail::frame_buffer fb;
                write_ping<flat_static_buffer_base>(fb,
                    detail::opcode::pong, payload);
                boost::asio::write(stream_, fb.data(), ec);
                if(! check_ok(ec))
                    return bytes_written;
                goto loop;
            }
            // Handle pong frame
            if(rd_fh_.op == detail::opcode::pong)
            {
                ping_data payload;
                detail::read_ping(payload, b);
                rd_buf_.consume(len);
                if(ctrl_cb_)
                    ctrl_cb_(frame_type::pong, payload);
                goto loop;
            }
            // Handle close frame
            BOOST_ASSERT(rd_fh_.op == detail::opcode::close);
            {
                BOOST_ASSERT(! rd_close_);
                rd_close_ = true;
                close_reason cr;
                detail::read_close(cr, b, code);
                if(code != close_code::none)
                {
                    // _Fail the WebSocket Connection_
                    do_fail(code, error::failed, ec);
                    return bytes_written;
                }
                cr_ = cr;
                rd_buf_.consume(len);
                if(ctrl_cb_)
                    ctrl_cb_(frame_type::close, cr_.reason);
                BOOST_ASSERT(! wr_close_);
                // _Start the WebSocket Closing Handshake_
                do_fail(
                    cr.code == close_code::none ?
                        close_code::normal : 
                        static_cast<close_code>(cr.code),
                    error::closed, ec);
                return bytes_written;
            }
        }
        if(rd_fh_.len == 0 && ! rd_fh_.fin)
        {
            // Empty non-final frame
            goto loop;
        }
        rd_done_ = false;
    }
    else
    {
        ec.assign(0, ec.category());
    }
    if(! pmd_ || ! pmd_->rd_set)
    {
        if(rd_remain_ > 0)
        {
            if(rd_buf_.size() == 0 && rd_buf_.max_size() >
                (std::min)(clamp(rd_remain_),
                    buffer_size(buffers)))
            {
                // Fill the read buffer first, otherwise we
                // get fewer bytes at the cost of one I/O.
                rd_buf_.commit(stream_.read_some(
                    rd_buf_.prepare(read_size(rd_buf_,
                        rd_buf_.max_size())), ec));
                if(! check_ok(ec))
                    return bytes_written;
                if(rd_fh_.mask)
                    detail::mask_inplace(
                        buffers_prefix(clamp(rd_remain_),
                            rd_buf_.data()), rd_key_);
            }
            if(rd_buf_.size() > 0)
            {
                // Copy from the read buffer.
                // The mask was already applied.
                auto const bytes_transferred =
                    buffer_copy(buffers, rd_buf_.data(),
                        clamp(rd_remain_));
                auto const mb = buffers_prefix(
                    bytes_transferred, buffers);
                rd_remain_ -= bytes_transferred;
                if(rd_op_ == detail::opcode::text)
                {
                    if(! rd_utf8_.write(mb) ||
                        (rd_remain_ == 0 && rd_fh_.fin &&
                            ! rd_utf8_.finish()))
                    {
                        // _Fail the WebSocket Connection_
                        do_fail(
                            close_code::bad_payload,
                            error::failed,
                            ec);
                        return bytes_written;
                    }
                }
                bytes_written += bytes_transferred;
                rd_size_ += bytes_transferred;
                rd_buf_.consume(bytes_transferred);
            }
            else
            {
                // Read into caller's buffer
                BOOST_ASSERT(rd_remain_ > 0);
                BOOST_ASSERT(buffer_size(buffers) > 0);
                BOOST_ASSERT(buffer_size(buffers_prefix(
                    clamp(rd_remain_), buffers)) > 0);
                auto const bytes_transferred =
                    stream_.read_some(buffers_prefix(
                        clamp(rd_remain_), buffers), ec);
                if(! check_ok(ec))
                    return bytes_written;
                BOOST_ASSERT(bytes_transferred > 0);
                auto const mb = buffers_prefix(
                    bytes_transferred, buffers);
                rd_remain_ -= bytes_transferred;
                if(rd_fh_.mask)
                    detail::mask_inplace(mb, rd_key_);
                if(rd_op_ == detail::opcode::text)
                {
                    if(! rd_utf8_.write(mb) ||
                        (rd_remain_ == 0 && rd_fh_.fin &&
                            ! rd_utf8_.finish()))
                    {
                        // _Fail the WebSocket Connection_
                        do_fail(close_code::bad_payload,
                            error::failed, ec);
                        return bytes_written;
                    }
                }
                bytes_written += bytes_transferred;
                rd_size_ += bytes_transferred;
            }
        }
        rd_done_ = rd_remain_ == 0 && rd_fh_.fin;
    }
    else
    {
        // Read compressed message frame payload:
        // inflate even if rd_fh_.len == 0, otherwise we
        // never emit the end-of-stream deflate block.
        //
        bool did_read = false;
        buffers_suffix<MutableBufferSequence> cb{buffers};
        while(buffer_size(cb) > 0)
        {
            zlib::z_params zs;
            {
                auto const out = buffers_front(cb);
                zs.next_out = out.data();
                zs.avail_out = out.size();
                BOOST_ASSERT(zs.avail_out > 0);
            }
            if(rd_remain_ > 0)
            {
                if(rd_buf_.size() > 0)
                {
                    // use what's there
                    auto const in = buffers_prefix(
                        clamp(rd_remain_), buffers_front(
                            rd_buf_.data()));
                    zs.avail_in = in.size();
                    zs.next_in = in.data();
                }
                else if(! did_read)
                {
                    // read new
                    auto const bytes_transferred =
                        stream_.read_some(
                            rd_buf_.prepare(read_size(
                                rd_buf_, rd_buf_.max_size())),
                            ec);
                    if(! check_ok(ec))
                        return bytes_written;
                    BOOST_ASSERT(bytes_transferred > 0);
                    rd_buf_.commit(bytes_transferred);
                    if(rd_fh_.mask)
                        detail::mask_inplace(
                            buffers_prefix(clamp(rd_remain_),
                                rd_buf_.data()), rd_key_);
                    auto const in = buffers_prefix(
                        clamp(rd_remain_), buffers_front(
                            rd_buf_.data()));
                    zs.avail_in = in.size();
                    zs.next_in = in.data();
                    did_read = true;
                }
                else
                {
                    break;
                }
            }
            else if(rd_fh_.fin)
            {
                // append the empty block codes
                static std::uint8_t constexpr
                    empty_block[4] = {
                        0x00, 0x00, 0xff, 0xff };
                zs.next_in = empty_block;
                zs.avail_in = sizeof(empty_block);
                pmd_->zi.write(zs, zlib::Flush::sync, ec);
                if(! ec)
                {
                    // https://github.com/madler/zlib/issues/280
                    if(zs.total_out > 0)
                        ec = error::partial_deflate_block;
                }
                if(! check_ok(ec))
                    return bytes_written;
                if(
                    (role_ == role_type::client &&
                        pmd_config_.server_no_context_takeover) ||
                    (role_ == role_type::server &&
                        pmd_config_.client_no_context_takeover))
                    pmd_->zi.reset();
                rd_done_ = true;
                break;
            }
            else
            {
                break;
            }
            pmd_->zi.write(zs, zlib::Flush::sync, ec);
            if(! check_ok(ec))
                return bytes_written;
            if(rd_msg_max_ && beast::detail::sum_exceeds(
                rd_size_, zs.total_out, rd_msg_max_))
            {
                do_fail(close_code::too_big,
                    error::failed, ec);
                return bytes_written;
            }
            cb.consume(zs.total_out);
            rd_size_ += zs.total_out;
            rd_remain_ -= zs.total_in;
            rd_buf_.consume(zs.total_in);
            bytes_written += zs.total_out;
        }
        if(rd_op_ == detail::opcode::text)
        {
            // check utf8
            if(! rd_utf8_.write(
                buffers_prefix(bytes_written, buffers)) || (
                    rd_done_ && ! rd_utf8_.finish()))
            {
                // _Fail the WebSocket Connection_
                do_fail(close_code::bad_payload,
                    error::failed, ec);
                return bytes_written;
            }
        }
    }
    return bytes_written;
}

template<class NextLayer>
template<class MutableBufferSequence, class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
stream<NextLayer>::
async_read_some(
    MutableBufferSequence const& buffers,
    ReadHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements requirements not met");
    static_assert(boost::asio::is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    boost::asio::async_completion<ReadHandler,
        void(error_code, std::size_t)> init{handler};
    read_some_op<MutableBufferSequence, BOOST_ASIO_HANDLER_TYPE(
        ReadHandler, void(error_code, std::size_t))>{
            init.completion_handler,*this, buffers}(
                {}, 0, false);
    return init.result.get();
}

} // websocket
} // beast
} // boost

#endif