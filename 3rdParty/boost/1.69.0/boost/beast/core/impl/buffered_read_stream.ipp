//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_BUFFERED_READ_STREAM_IPP
#define BOOST_BEAST_IMPL_BUFFERED_READ_STREAM_IPP

#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/read_size.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/asio/post.hpp>
#include <boost/throw_exception.hpp>

namespace boost {
namespace beast {

template<class Stream, class DynamicBuffer>
template<class MutableBufferSequence, class Handler>
class buffered_read_stream<
    Stream, DynamicBuffer>::read_some_op
{
    buffered_read_stream& s_;
    boost::asio::executor_work_guard<decltype(
        std::declval<Stream&>().get_executor())> wg_;
    MutableBufferSequence b_;
    Handler h_;
    int step_ = 0;

public:
    read_some_op(read_some_op&&) = default;
    read_some_op(read_some_op const&) = delete;

    template<class DeducedHandler, class... Args>
    read_some_op(DeducedHandler&& h,
        buffered_read_stream& s,
            MutableBufferSequence const& b)
        : s_(s)
        , wg_(s_.get_executor())
        , b_(b)
        , h_(std::forward<DeducedHandler>(h))
    {
    }

    using allocator_type =
        boost::asio::associated_allocator_t<Handler>;

    allocator_type
    get_allocator() const noexcept
    {
        return (boost::asio::get_associated_allocator)(h_);
    }

    using executor_type =
        boost::asio::associated_executor_t<Handler, decltype(
            std::declval<buffered_read_stream&>().get_executor())>;

    executor_type
    get_executor() const noexcept
    {
        return (boost::asio::get_associated_executor)(
            h_, s_.get_executor());
    }

    void
    operator()(error_code const& ec,
        std::size_t bytes_transferred);

    friend
    bool asio_handler_is_continuation(read_some_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, read_some_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(op->h_));
    }
};

template<class Stream, class DynamicBuffer>
template<class MutableBufferSequence, class Handler>
void
buffered_read_stream<Stream, DynamicBuffer>::
read_some_op<MutableBufferSequence, Handler>::operator()(
    error_code const& ec, std::size_t bytes_transferred)
{
    switch(step_)
    {
    case 0:
        if(s_.buffer_.size() == 0)
        {
            if(s_.capacity_ == 0)
            {
                // read (unbuffered)
                step_ = 1;
                return s_.next_layer_.async_read_some(
                    b_, std::move(*this));
            }

            // read
            step_ = 2;
            return s_.next_layer_.async_read_some(
                s_.buffer_.prepare(read_size(
                    s_.buffer_, s_.capacity_)),
                        std::move(*this));

        }
        step_ = 3;
        return boost::asio::post(
            s_.get_executor(),
            bind_handler(std::move(*this), ec, 0));

    case 1:
        // upcall
        break;

    case 2:
        s_.buffer_.commit(bytes_transferred);
        BOOST_FALLTHROUGH;

    case 3:
        bytes_transferred =
            boost::asio::buffer_copy(b_, s_.buffer_.data());
        s_.buffer_.consume(bytes_transferred);
        break;
    }
    h_(ec, bytes_transferred);
}

//------------------------------------------------------------------------------

template<class Stream, class DynamicBuffer>
template<class... Args>
buffered_read_stream<Stream, DynamicBuffer>::
buffered_read_stream(Args&&... args)
    : next_layer_(std::forward<Args>(args)...)
{
}

template<class Stream, class DynamicBuffer>
template<class ConstBufferSequence, class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code, std::size_t))
buffered_read_stream<Stream, DynamicBuffer>::
async_write_some(
    ConstBufferSequence const& buffers,
    WriteHandler&& handler)
{
    static_assert(is_async_write_stream<next_layer_type>::value,
        "AsyncWriteStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    static_assert(is_completion_handler<WriteHandler,
        void(error_code, std::size_t)>::value,
            "WriteHandler requirements not met");
    return next_layer_.async_write_some(buffers,
        std::forward<WriteHandler>(handler));
}

template<class Stream, class DynamicBuffer>
template<class MutableBufferSequence>
std::size_t
buffered_read_stream<Stream, DynamicBuffer>::
read_some(
    MutableBufferSequence const& buffers)
{
    static_assert(is_sync_read_stream<next_layer_type>::value,
        "SyncReadStream requirements not met");
    static_assert(boost::asio::is_mutable_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence requirements not met");
    error_code ec;
    auto n = read_some(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return n;
}

template<class Stream, class DynamicBuffer>
template<class MutableBufferSequence>
std::size_t
buffered_read_stream<Stream, DynamicBuffer>::
read_some(MutableBufferSequence const& buffers,
    error_code& ec)
{
    static_assert(is_sync_read_stream<next_layer_type>::value,
        "SyncReadStream requirements not met");
    static_assert(boost::asio::is_mutable_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence requirements not met");
    using boost::asio::buffer_size;
    using boost::asio::buffer_copy;
    if(buffer_.size() == 0)
    {
        if(capacity_ == 0)
            return next_layer_.read_some(buffers, ec);
        buffer_.commit(next_layer_.read_some(
            buffer_.prepare(read_size(buffer_,
                capacity_)), ec));
        if(ec)
            return 0;
    }
    else
    {
        ec.assign(0, ec.category());
    }
    auto bytes_transferred =
        buffer_copy(buffers, buffer_.data());
    buffer_.consume(bytes_transferred);
    return bytes_transferred;
}

template<class Stream, class DynamicBuffer>
template<class MutableBufferSequence, class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
buffered_read_stream<Stream, DynamicBuffer>::
async_read_some(
    MutableBufferSequence const& buffers,
    ReadHandler&& handler)
{
    static_assert(is_async_read_stream<next_layer_type>::value,
        "AsyncReadStream requirements not met");
    static_assert(boost::asio::is_mutable_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence requirements not met");
    if(buffer_.size() == 0 && capacity_ == 0)
        return next_layer_.async_read_some(buffers,
            std::forward<ReadHandler>(handler));
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    read_some_op<MutableBufferSequence, BOOST_ASIO_HANDLER_TYPE(
        ReadHandler, void(error_code, std::size_t))>{
            std::move(init.completion_handler), *this, buffers}(
                error_code{}, 0);
    return init.result.get();
}

} // beast
} // boost

#endif
