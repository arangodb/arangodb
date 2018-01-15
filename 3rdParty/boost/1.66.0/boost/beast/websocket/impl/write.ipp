//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_IMPL_WRITE_IPP
#define BOOST_BEAST_WEBSOCKET_IMPL_WRITE_IPP

#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/buffers_cat.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/core/detail/clamp.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/websocket/detail/frame.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <memory>

namespace boost {
namespace beast {
namespace websocket {

template<class NextLayer>
template<class Buffers, class Handler>
class stream<NextLayer>::write_some_op
    : public boost::asio::coroutine
{
    Handler h_;
    stream<NextLayer>& ws_;
    buffers_suffix<Buffers> cb_;
    detail::frame_header fh_;
    detail::prepared_key key_;
    std::size_t bytes_transferred_ = 0;
    std::size_t remain_;
    std::size_t in_;
    token tok_;
    int how_;
    bool fin_;
    bool more_;
    bool cont_ = false;

public:
    write_some_op(write_some_op&&) = default;
    write_some_op(write_some_op const&) = default;

    template<class DeducedHandler>
    write_some_op(
        DeducedHandler&& h,
        stream<NextLayer>& ws,
        bool fin,
        Buffers const& bs)
        : h_(std::forward<DeducedHandler>(h))
        , ws_(ws)
        , cb_(bs)
        , tok_(ws_.tok_.unique())
        , fin_(fin)
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
    bool asio_handler_is_continuation(write_some_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return op->cont_ || asio_handler_is_continuation(
            std::addressof(op->h_));
    }
};

template<class NextLayer>
template<class Buffers, class Handler>
void
stream<NextLayer>::
write_some_op<Buffers, Handler>::
operator()(
    error_code ec,
    std::size_t bytes_transferred,
    bool cont)
{
    using beast::detail::clamp;
    using boost::asio::buffer;
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    using boost::asio::mutable_buffer;
    enum
    {
        do_nomask_nofrag,
        do_nomask_frag,
        do_mask_nofrag,
        do_mask_frag,
        do_deflate
    };
    std::size_t n;
    boost::asio::mutable_buffer b;
    cont_ = cont;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        // Set up the outgoing frame header
        if(! ws_.wr_cont_)
        {
            ws_.begin_msg();
            fh_.rsv1 = ws_.wr_compress_;
        }
        else
        {
            fh_.rsv1 = false;
        }
        fh_.rsv2 = false;
        fh_.rsv3 = false;
        fh_.op = ws_.wr_cont_ ?
            detail::opcode::cont : ws_.wr_opcode_;
        fh_.mask =
            ws_.role_ == role_type::client;

        // Choose a write algorithm
        if(ws_.wr_compress_)
        {
            how_ = do_deflate;
        }
        else if(! fh_.mask)
        {
            if(! ws_.wr_frag_)
            {
                how_ = do_nomask_nofrag;
            }
            else
            {
                BOOST_ASSERT(ws_.wr_buf_size_ != 0);
                remain_ = buffer_size(cb_);
                if(remain_ > ws_.wr_buf_size_)
                    how_ = do_nomask_frag;
                else
                    how_ = do_nomask_nofrag;
            }
        }
        else
        {
            if(! ws_.wr_frag_)
            {
                how_ = do_mask_nofrag;
            }
            else
            {
                BOOST_ASSERT(ws_.wr_buf_size_ != 0);
                remain_ = buffer_size(cb_);
                if(remain_ > ws_.wr_buf_size_)
                    how_ = do_mask_frag;
                else
                    how_ = do_mask_nofrag;
            }
        }

        // Maybe suspend
        if(! ws_.wr_block_)
        {
            // Acquire the write block
            ws_.wr_block_ = tok_;

            // Make sure the stream is open
            if(! ws_.check_open(ec))
                goto upcall;
        }
        else
        {
        do_suspend:
            // Suspend
            BOOST_ASSERT(ws_.wr_block_ != tok_);
            BOOST_ASIO_CORO_YIELD
            ws_.paused_wr_.save(std::move(*this));

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

        //------------------------------------------------------------------

        if(how_ == do_nomask_nofrag)
        {
            fh_.fin = fin_;
            fh_.len = buffer_size(cb_);
            ws_.wr_fb_.reset();
            detail::write<flat_static_buffer_base>(
                ws_.wr_fb_, fh_);
            ws_.wr_cont_ = ! fin_;
            // Send frame
            BOOST_ASIO_CORO_YIELD
            boost::asio::async_write(ws_.stream_,
                buffers_cat(ws_.wr_fb_.data(), cb_),
                    std::move(*this));
            if(! ws_.check_ok(ec))
                goto upcall;
            bytes_transferred_ += clamp(fh_.len);
            goto upcall;
        }

        //------------------------------------------------------------------

        else if(how_ == do_nomask_frag)
        {
            for(;;)
            {
                n = clamp(remain_, ws_.wr_buf_size_);
                fh_.len = n;
                remain_ -= n;
                fh_.fin = fin_ ? remain_ == 0 : false;
                ws_.wr_fb_.reset();
                detail::write<flat_static_buffer_base>(
                    ws_.wr_fb_, fh_);
                ws_.wr_cont_ = ! fin_;
                // Send frame
                BOOST_ASIO_CORO_YIELD
                boost::asio::async_write(
                    ws_.stream_, buffers_cat(
                        ws_.wr_fb_.data(), buffers_prefix(
                            clamp(fh_.len), cb_)),
                                std::move(*this));
                if(! ws_.check_ok(ec))
                    goto upcall;
                n = clamp(fh_.len); // because yield
                bytes_transferred_ += n;
                if(remain_ == 0)
                    break;
                cb_.consume(n);
                fh_.op = detail::opcode::cont;
                // Allow outgoing control frames to
                // be sent in between message frames
                ws_.wr_block_.reset();
                if( ws_.paused_close_.maybe_invoke() ||
                    ws_.paused_rd_.maybe_invoke() ||
                    ws_.paused_ping_.maybe_invoke())
                {
                    BOOST_ASSERT(ws_.wr_block_);
                    goto do_suspend;
                }
                ws_.wr_block_ = tok_;
            }
            goto upcall;
        }

        //------------------------------------------------------------------

        else if(how_ == do_mask_nofrag)
        {
            remain_ = buffer_size(cb_);
            fh_.fin = fin_;
            fh_.len = remain_;
            fh_.key = ws_.wr_gen_();
            detail::prepare_key(key_, fh_.key);
            ws_.wr_fb_.reset();
            detail::write<flat_static_buffer_base>(
                ws_.wr_fb_, fh_);
            n = clamp(remain_, ws_.wr_buf_size_);
            buffer_copy(buffer(
                ws_.wr_buf_.get(), n), cb_);
            detail::mask_inplace(buffer(
                ws_.wr_buf_.get(), n), key_);
            remain_ -= n;
            ws_.wr_cont_ = ! fin_;
            // Send frame header and partial payload
            BOOST_ASIO_CORO_YIELD
            boost::asio::async_write(
                ws_.stream_, buffers_cat(ws_.wr_fb_.data(),
                    buffer(ws_.wr_buf_.get(), n)),
                        std::move(*this));
            if(! ws_.check_ok(ec))
                goto upcall;
            bytes_transferred_ +=
                bytes_transferred - ws_.wr_fb_.size();
            while(remain_ > 0)
            {
                cb_.consume(ws_.wr_buf_size_);
                n = clamp(remain_, ws_.wr_buf_size_);
                buffer_copy(buffer(
                    ws_.wr_buf_.get(), n), cb_);
                detail::mask_inplace(buffer(
                    ws_.wr_buf_.get(), n), key_);
                remain_ -= n;
                // Send partial payload
                BOOST_ASIO_CORO_YIELD
                boost::asio::async_write(ws_.stream_,
                    buffer(ws_.wr_buf_.get(), n),
                        std::move(*this));
                if(! ws_.check_ok(ec))
                    goto upcall;
                bytes_transferred_ += bytes_transferred;
            }
            goto upcall;
        }

        //------------------------------------------------------------------

        else if(how_ == do_mask_frag)
        {
            for(;;)
            {
                n = clamp(remain_, ws_.wr_buf_size_);
                remain_ -= n;
                fh_.len = n;
                fh_.key = ws_.wr_gen_();
                fh_.fin = fin_ ? remain_ == 0 : false;
                detail::prepare_key(key_, fh_.key);
                buffer_copy(buffer(
                    ws_.wr_buf_.get(), n), cb_);
                detail::mask_inplace(buffer(
                    ws_.wr_buf_.get(), n), key_);
                ws_.wr_fb_.reset();
                detail::write<flat_static_buffer_base>(
                    ws_.wr_fb_, fh_);
                ws_.wr_cont_ = ! fin_;
                // Send frame
                BOOST_ASIO_CORO_YIELD
                boost::asio::async_write(ws_.stream_,
                    buffers_cat(ws_.wr_fb_.data(),
                        buffer(ws_.wr_buf_.get(), n)),
                            std::move(*this));
                if(! ws_.check_ok(ec))
                    goto upcall;
                n = bytes_transferred - ws_.wr_fb_.size();
                bytes_transferred_ += n;
                if(remain_ == 0)
                    break;
                cb_.consume(n);
                fh_.op = detail::opcode::cont;
                // Allow outgoing control frames to
                // be sent in between message frames:
                ws_.wr_block_.reset();
                if( ws_.paused_close_.maybe_invoke() ||
                    ws_.paused_rd_.maybe_invoke() ||
                    ws_.paused_ping_.maybe_invoke())
                {
                    BOOST_ASSERT(ws_.wr_block_);
                    goto do_suspend;
                }
                ws_.wr_block_ = tok_;
            }
            goto upcall;
        }

        //------------------------------------------------------------------

        else if(how_ == do_deflate)
        {
            for(;;)
            {
                b = buffer(ws_.wr_buf_.get(),
                    ws_.wr_buf_size_);
                more_ = detail::deflate(ws_.pmd_->zo,
                    b, cb_, fin_, in_, ec);
                if(! ws_.check_ok(ec))
                    goto upcall;
                n = buffer_size(b);
                if(n == 0)
                {
                    // The input was consumed, but there
                    // is no output due to compression
                    // latency.
                    BOOST_ASSERT(! fin_);
                    BOOST_ASSERT(buffer_size(cb_) == 0);
                    goto upcall;
                }
                if(fh_.mask)
                {
                    fh_.key = ws_.wr_gen_();
                    detail::prepared_key key;
                    detail::prepare_key(key, fh_.key);
                    detail::mask_inplace(b, key);
                }
                fh_.fin = ! more_;
                fh_.len = n;
                ws_.wr_fb_.reset();
                detail::write<
                    flat_static_buffer_base>(ws_.wr_fb_, fh_);
                ws_.wr_cont_ = ! fin_;
                // Send frame
                BOOST_ASIO_CORO_YIELD
                boost::asio::async_write(ws_.stream_,
                    buffers_cat(ws_.wr_fb_.data(), b),
                        std::move(*this));
                if(! ws_.check_ok(ec))
                    goto upcall;
                bytes_transferred_ += in_;
                if(more_)
                {
                    fh_.op = detail::opcode::cont;
                    fh_.rsv1 = false;
                    // Allow outgoing control frames to
                    // be sent in between message frames:
                    ws_.wr_block_.reset();
                    if( ws_.paused_close_.maybe_invoke() ||
                        ws_.paused_rd_.maybe_invoke() ||
                        ws_.paused_ping_.maybe_invoke())
                    {
                        BOOST_ASSERT(ws_.wr_block_);
                        goto do_suspend;
                    }
                    ws_.wr_block_ = tok_;
                }
                else
                {
                    if(fh_.fin && (
                        (ws_.role_ == role_type::client &&
                            ws_.pmd_config_.client_no_context_takeover) ||
                        (ws_.role_ == role_type::server &&
                            ws_.pmd_config_.server_no_context_takeover)))
                        ws_.pmd_->zo.reset();
                    goto upcall;
                }
            }
        }

    //--------------------------------------------------------------------------

    upcall:
        BOOST_ASSERT(ws_.wr_block_ == tok_);
        ws_.wr_block_.reset();
        ws_.paused_close_.maybe_invoke() ||
            ws_.paused_rd_.maybe_invoke() ||
            ws_.paused_ping_.maybe_invoke();
        if(! cont_)
            return boost::asio::post(
                ws_.stream_.get_executor(),
                bind_handler(h_, ec, bytes_transferred_));
        h_(ec, bytes_transferred_);
    }
}

//------------------------------------------------------------------------------

template<class NextLayer>
template<class ConstBufferSequence>
std::size_t
stream<NextLayer>::
write_some(bool fin, ConstBufferSequence const& buffers)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    error_code ec;
    auto const bytes_transferred =
        write_some(fin, buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<class NextLayer>
template<class ConstBufferSequence>
std::size_t
stream<NextLayer>::
write_some(bool fin,
    ConstBufferSequence const& buffers, error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    using beast::detail::clamp;
    using boost::asio::buffer;
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    std::size_t bytes_transferred = 0;
    ec.assign(0, ec.category());
    // Make sure the stream is open
    if(! check_open(ec))
        return bytes_transferred;
    detail::frame_header fh;
    if(! wr_cont_)
    {
        begin_msg();
        fh.rsv1 = wr_compress_;
    }
    else
    {
        fh.rsv1 = false;
    }
    fh.rsv2 = false;
    fh.rsv3 = false;
    fh.op = wr_cont_ ?
        detail::opcode::cont : wr_opcode_;
    fh.mask = role_ == role_type::client;
    auto remain = buffer_size(buffers);
    if(wr_compress_)
    {
        buffers_suffix<
            ConstBufferSequence> cb{buffers};
        for(;;)
        {
            auto b = buffer(
                wr_buf_.get(), wr_buf_size_);
            auto const more = detail::deflate(
                pmd_->zo, b, cb, fin,
                    bytes_transferred, ec);
            if(! check_ok(ec))
                return bytes_transferred;
            auto const n = buffer_size(b);
            if(n == 0)
            {
                // The input was consumed, but there
                // is no output due to compression
                // latency.
                BOOST_ASSERT(! fin);
                BOOST_ASSERT(buffer_size(cb) == 0);
                fh.fin = false;
                break;
            }
            if(fh.mask)
            {
                fh.key = wr_gen_();
                detail::prepared_key key;
                detail::prepare_key(key, fh.key);
                detail::mask_inplace(b, key);
            }
            fh.fin = ! more;
            fh.len = n;
            detail::fh_buffer fh_buf;
            detail::write<
                flat_static_buffer_base>(fh_buf, fh);
            wr_cont_ = ! fin;
            boost::asio::write(stream_,
                buffers_cat(fh_buf.data(), b), ec);
            if(! check_ok(ec))
                return bytes_transferred;
            if(! more)
                break;
            fh.op = detail::opcode::cont;
            fh.rsv1 = false;
        }
        if(fh.fin && (
            (role_ == role_type::client &&
                pmd_config_.client_no_context_takeover) ||
            (role_ == role_type::server &&
                pmd_config_.server_no_context_takeover)))
            pmd_->zo.reset();
    }
    else if(! fh.mask)
    {
        if(! wr_frag_)
        {
            // no mask, no autofrag
            fh.fin = fin;
            fh.len = remain;
            detail::fh_buffer fh_buf;
            detail::write<
                flat_static_buffer_base>(fh_buf, fh);
            wr_cont_ = ! fin;
            boost::asio::write(stream_,
                buffers_cat(fh_buf.data(), buffers), ec);
            if(! check_ok(ec))
                return bytes_transferred;
            bytes_transferred += remain;
        }
        else
        {
            // no mask, autofrag
            BOOST_ASSERT(wr_buf_size_ != 0);
            buffers_suffix<
                ConstBufferSequence> cb{buffers};
            for(;;)
            {
                auto const n = clamp(remain, wr_buf_size_);
                remain -= n;
                fh.len = n;
                fh.fin = fin ? remain == 0 : false;
                detail::fh_buffer fh_buf;
                detail::write<
                    flat_static_buffer_base>(fh_buf, fh);
                wr_cont_ = ! fin;
                boost::asio::write(stream_,
                    buffers_cat(fh_buf.data(),
                        buffers_prefix(n, cb)), ec);
                if(! check_ok(ec))
                    return bytes_transferred;
                bytes_transferred += n;
                if(remain == 0)
                    break;
                fh.op = detail::opcode::cont;
                cb.consume(n);
            }
        }
    }
    else if(! wr_frag_)
    {
        // mask, no autofrag
        fh.fin = fin;
        fh.len = remain;
        fh.key = wr_gen_();
        detail::prepared_key key;
        detail::prepare_key(key, fh.key);
        detail::fh_buffer fh_buf;
        detail::write<
            flat_static_buffer_base>(fh_buf, fh);
        buffers_suffix<
            ConstBufferSequence> cb{buffers};
        {
            auto const n = clamp(remain, wr_buf_size_);
            auto const b = buffer(wr_buf_.get(), n);
            buffer_copy(b, cb);
            cb.consume(n);
            remain -= n;
            detail::mask_inplace(b, key);
            wr_cont_ = ! fin;
            boost::asio::write(stream_,
                buffers_cat(fh_buf.data(), b), ec);
            if(! check_ok(ec))
                return bytes_transferred;
            bytes_transferred += n;
        }
        while(remain > 0)
        {
            auto const n = clamp(remain, wr_buf_size_);
            auto const b = buffer(wr_buf_.get(), n);
            buffer_copy(b, cb);
            cb.consume(n);
            remain -= n;
            detail::mask_inplace(b, key);
            boost::asio::write(stream_, b, ec);
            if(! check_ok(ec))
                return bytes_transferred;
            bytes_transferred += n;
        }
    }
    else
    {
        // mask, autofrag
        BOOST_ASSERT(wr_buf_size_ != 0);
        buffers_suffix<
            ConstBufferSequence> cb{buffers};
        for(;;)
        {
            fh.key = wr_gen_();
            detail::prepared_key key;
            detail::prepare_key(key, fh.key);
            auto const n = clamp(remain, wr_buf_size_);
            auto const b = buffer(wr_buf_.get(), n);
            buffer_copy(b, cb);
            detail::mask_inplace(b, key);
            fh.len = n;
            remain -= n;
            fh.fin = fin ? remain == 0 : false;
            wr_cont_ = ! fh.fin;
            detail::fh_buffer fh_buf;
            detail::write<
                flat_static_buffer_base>(fh_buf, fh);
            boost::asio::write(stream_,
                buffers_cat(fh_buf.data(), b), ec);
            if(! check_ok(ec))
                return bytes_transferred;
            bytes_transferred += n;
            if(remain == 0)
                break;
            fh.op = detail::opcode::cont;
            cb.consume(n);
        }
    }
    return bytes_transferred;
}

template<class NextLayer>
template<class ConstBufferSequence, class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code, std::size_t))
stream<NextLayer>::
async_write_some(bool fin,
    ConstBufferSequence const& bs, WriteHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    boost::asio::async_completion<WriteHandler,
        void(error_code, std::size_t)> init{handler};
    write_some_op<ConstBufferSequence, BOOST_ASIO_HANDLER_TYPE(
        WriteHandler, void(error_code, std::size_t))>{
            init.completion_handler, *this, fin, bs}(
                {}, 0, false);
    return init.result.get();
}

//------------------------------------------------------------------------------

template<class NextLayer>
template<class ConstBufferSequence>
std::size_t
stream<NextLayer>::
write(ConstBufferSequence const& buffers)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    error_code ec;
    auto const bytes_transferred = write(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<class NextLayer>
template<class ConstBufferSequence>
std::size_t
stream<NextLayer>::
write(ConstBufferSequence const& buffers, error_code& ec)
{
    static_assert(is_sync_stream<next_layer_type>::value,
        "SyncStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    return write_some(true, buffers, ec);
}

template<class NextLayer>
template<class ConstBufferSequence, class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code, std::size_t))
stream<NextLayer>::
async_write(
    ConstBufferSequence const& bs, WriteHandler&& handler)
{
    static_assert(is_async_stream<next_layer_type>::value,
        "AsyncStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    boost::asio::async_completion<WriteHandler,
        void(error_code, std::size_t)> init{handler};
    write_some_op<ConstBufferSequence, BOOST_ASIO_HANDLER_TYPE(
        WriteHandler, void(error_code, std::size_t))>{
            init.completion_handler, *this, true, bs}(
                {}, 0, false);
    return init.result.get();
}

} // websocket
} // beast
} // boost

#endif
