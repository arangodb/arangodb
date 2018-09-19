//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_IMPL_FLAT_STREAM_IPP
#define BOOST_BEAST_CORE_IMPL_FLAT_STREAM_IPP

#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/websocket/teardown.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>

namespace boost {
namespace beast {

template<class NextLayer>
template<class ConstBufferSequence, class Handler>
class flat_stream<NextLayer>::write_op
    : public boost::asio::coroutine
{
    using alloc_type = typename 
#if defined(BOOST_NO_CXX11_ALLOCATOR)
        boost::asio::associated_allocator_t<Handler>::template
            rebind<char>::other;
#else
        std::allocator_traits<boost::asio::associated_allocator_t<Handler>>
            ::template rebind_alloc<char>;
#endif

    struct deleter
    {
        alloc_type alloc;
        std::size_t size = 0;

        explicit
        deleter(alloc_type const& alloc_)
            : alloc(alloc_)
        {
        }

        void
        operator()(char* p)
        {
            alloc.deallocate(p, size);
        }
    };

    flat_stream<NextLayer>& s_;
    ConstBufferSequence b_;
    std::unique_ptr<char, deleter> p_;
    Handler h_;

public:
    template<class DeducedHandler>
    write_op(
        flat_stream<NextLayer>& s,
        ConstBufferSequence const& b,
        DeducedHandler&& h)
        : s_(s)
        , b_(b)
        , p_(nullptr, deleter{
            (boost::asio::get_associated_allocator)(h)})
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

    using executor_type = boost::asio::associated_executor_t<
        Handler, decltype(std::declval<NextLayer&>().get_executor())>;

    executor_type
    get_executor() const noexcept
    {
        return (boost::asio::get_associated_executor)(
            h_, s_.get_executor());
    }

    void
    operator()(
        boost::system::error_code ec,
        std::size_t bytes_transferred);

    friend
    bool asio_handler_is_continuation(write_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
                std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, write_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(op->h_));
    }
};

template<class NextLayer>
template<class ConstBufferSequence, class Handler>
void
flat_stream<NextLayer>::
write_op<ConstBufferSequence, Handler>::
operator()(
    error_code ec,
    std::size_t bytes_transferred)
{
    BOOST_ASIO_CORO_REENTER(*this)
    {
        BOOST_ASIO_CORO_YIELD
        {
            auto const result = coalesce(b_,  coalesce_limit);
            if(result.second)
            {
                p_.get_deleter().size = result.first;
                p_.reset(p_.get_deleter().alloc.allocate(
                    p_.get_deleter().size));
                boost::asio::buffer_copy(
                    boost::asio::buffer(
                        p_.get(), p_.get_deleter().size),
                    b_, result.first);
                s_.stream_.async_write_some(
                    boost::asio::buffer(
                        p_.get(), p_.get_deleter().size),
                            std::move(*this));
            }
            else
            {
                s_.stream_.async_write_some(
                    boost::beast::buffers_prefix(result.first, b_),
                        std::move(*this));
            }
        }
        p_.reset();
        h_(ec, bytes_transferred);
    }
}

//------------------------------------------------------------------------------

template<class NextLayer>
template<class... Args>
flat_stream<NextLayer>::
flat_stream(Args&&... args)
    : stream_(std::forward<Args>(args)...)
{
}

template<class NextLayer>
template<class MutableBufferSequence>
std::size_t
flat_stream<NextLayer>::
read_some(MutableBufferSequence const& buffers)
{
    static_assert(boost::beast::is_sync_read_stream<next_layer_type>::value,
        "SyncReadStream requirements not met");
    static_assert(boost::asio::is_mutable_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence requirements not met");
    error_code ec;
    auto n = read_some(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(boost::system::system_error{ec});
    return n;
}

template<class NextLayer>
template<class MutableBufferSequence>
std::size_t
flat_stream<NextLayer>::
read_some(MutableBufferSequence const& buffers, error_code& ec)
{
    return stream_.read_some(buffers, ec);
}

template<class NextLayer>
template<
    class MutableBufferSequence,
    class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
flat_stream<NextLayer>::
async_read_some(
    MutableBufferSequence const& buffers,
    ReadHandler&& handler)
{
    static_assert(boost::beast::is_async_read_stream<next_layer_type>::value,
        "AsyncReadStream requirements not met");
    static_assert(boost::asio::is_mutable_buffer_sequence<
            MutableBufferSequence >::value,
        "MutableBufferSequence  requirements not met");
    return stream_.async_read_some(
        buffers, std::forward<ReadHandler>(handler));
}

template<class NextLayer>
template<class ConstBufferSequence>
std::size_t
flat_stream<NextLayer>::
write_some(ConstBufferSequence const& buffers)
{
    static_assert(boost::beast::is_sync_write_stream<next_layer_type>::value,
        "SyncWriteStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    auto const result = coalesce(buffers, coalesce_limit);
    if(result.second)
    {
        std::unique_ptr<char[]> p{new char[result.first]};
        auto const b = boost::asio::buffer(p.get(), result.first);
        boost::asio::buffer_copy(b, buffers);
        return stream_.write_some(b);
    }
    return stream_.write_some(
        boost::beast::buffers_prefix(result.first, buffers));
}

template<class NextLayer>
template<class ConstBufferSequence>
std::size_t
flat_stream<NextLayer>::
write_some(ConstBufferSequence const& buffers, error_code& ec)
{
    static_assert(boost::beast::is_sync_write_stream<next_layer_type>::value,
        "SyncWriteStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    auto const result = coalesce(buffers, coalesce_limit);
    if(result.second)
    {
        std::unique_ptr<char[]> p{new char[result.first]};
        auto const b = boost::asio::buffer(p.get(), result.first);
        boost::asio::buffer_copy(b, buffers);
        return stream_.write_some(b, ec);
    }
    return stream_.write_some(
        boost::beast::buffers_prefix(result.first, buffers), ec);
}

template<class NextLayer>
template<
    class ConstBufferSequence,
    class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code, std::size_t))
flat_stream<NextLayer>::
async_write_some(
    ConstBufferSequence const& buffers,
    WriteHandler&& handler)
{
    static_assert(boost::beast::is_async_write_stream<next_layer_type>::value,
        "AsyncWriteStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
            ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(error_code, std::size_t));
    write_op<ConstBufferSequence, BOOST_ASIO_HANDLER_TYPE(
        WriteHandler, void(error_code, std::size_t))>{
            *this, buffers, std::move(init.completion_handler)}({}, 0);
    return init.result.get();
}

template<class NextLayer>
void
teardown(
    boost::beast::websocket::role_type role,
    flat_stream<NextLayer>& s,
    error_code& ec)
{
    using boost::beast::websocket::teardown;
    teardown(role, s.next_layer(), ec);
}

template<class NextLayer, class TeardownHandler>
void
async_teardown(
    boost::beast::websocket::role_type role,
    flat_stream<NextLayer>& s,
    TeardownHandler&& handler)
{
    using boost::beast::websocket::async_teardown;
    async_teardown(role, s.next_layer(), std::move(handler));
}

} // beast
} // boost

#endif
