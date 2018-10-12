//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_IMPL_ICY_STREAM_IPP
#define BOOST_BEAST_CORE_IMPL_ICY_STREAM_IPP

#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/buffers_adapter.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/detail/buffers_ref.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/assert.hpp>
#include <boost/throw_exception.hpp>
#include <algorithm>
#include <memory>
#include <utility>

namespace boost {
namespace beast {
namespace http {

namespace detail {

template<class DynamicBuffer>
class dynamic_buffer_ref
{
    DynamicBuffer& b_;

public:
    using const_buffers_type =
        typename DynamicBuffer::const_buffers_type;

    using mutable_buffers_type =
        typename DynamicBuffer::mutable_buffers_type;

    dynamic_buffer_ref(dynamic_buffer_ref&&) = default;

    explicit
    dynamic_buffer_ref(DynamicBuffer& b)
        : b_(b)
    {
    }

    std::size_t
    size() const
    {
        return b_.size();
    }
    
    std::size_t
    max_size() const
    {
        return b_.max_size();
    }

    std::size_t
    capacity() const
    {
        return b_.capacity();
    }

    const_buffers_type
    data() const
    {
        return b_.data();
    }

    mutable_buffers_type
    prepare(std::size_t n)
    {
        return b_.prepare(n);
    }

    void
    commit(std::size_t n)
    {
        b_.commit(n);
    }

    void
    consume(std::size_t n)
    {
        b_.consume(n);
    }
};

template<class DynamicBuffer>
typename std::enable_if<
    boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
    dynamic_buffer_ref<DynamicBuffer>>::type
ref(DynamicBuffer& b)
{
    return dynamic_buffer_ref<DynamicBuffer>(b);
}

template<class MutableBuffers, class ConstBuffers>
void
buffer_shift(MutableBuffers const& out, ConstBuffers const& in)
{
    using boost::asio::buffer_size;
    auto in_pos  = boost::asio::buffer_sequence_end(in);
    auto out_pos = boost::asio::buffer_sequence_end(out);
    auto const in_begin  = boost::asio::buffer_sequence_begin(in);
    auto const out_begin = boost::asio::buffer_sequence_begin(out);
    BOOST_ASSERT(buffer_size(in) == buffer_size(out));
    if(in_pos == in_begin || out_pos == out_begin)
        return;
    boost::asio::const_buffer cb{*--in_pos};
    boost::asio::mutable_buffer mb{*--out_pos};
    for(;;)
    {
        if(mb.size() >= cb.size())
        {
            std::memmove(
                static_cast<char*>(
                    mb.data()) + mb.size() - cb.size(),
                cb.data(),
                cb.size());
            mb = boost::asio::mutable_buffer{
                mb.data(), mb.size() - cb.size()};
            if(in_pos == in_begin)
                break;
            cb = *--in_pos;
        }
        else
        {
            std::memmove(
                mb.data(),
                static_cast<char const*>(
                    cb.data()) + cb.size() - mb.size(),
                mb.size());
            cb = boost::asio::const_buffer{
                cb.data(), cb.size() - mb.size()};
            if(out_pos == out_begin)
                break;
            mb = *--out_pos;
        }
    }
}

template<class FwdIt>
class match_icy
{
    bool& match_;

public:
    using result_type = std::pair<FwdIt, bool>;
    explicit
    match_icy(bool& b)
        : match_(b)
    {
    }

    result_type
    operator()(FwdIt first, FwdIt last) const
    {
        auto it = first;
        if(it == last)
            return {first, false};
        if(*it != 'I')
            return {last, true};
        if(++it == last)
            return {first, false};
        if(*it != 'C')
            return {last, true};
        if(++it == last)
            return {first, false};
        if(*it != 'Y')
            return {last, true};
        match_ = true;
        return {last, true};
    };
};

} // detail

template<class NextLayer>
template<class MutableBufferSequence, class Handler>
class icy_stream<NextLayer>::read_op
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

    struct data
    {
        icy_stream<NextLayer>& s;
        buffers_adapter<MutableBufferSequence> b;
        bool match = false;

        data(
            Handler const&,
            icy_stream<NextLayer>& s_,
            MutableBufferSequence const& b_)
            : s(s_)
            , b(b_)
        {
        }
    };

    handler_ptr<data, Handler> d_;

public:
    read_op(read_op&&) = default;
    read_op(read_op const&) = delete;

    template<class DeducedHandler, class... Args>
    read_op(
        DeducedHandler&& h,
        icy_stream<NextLayer>& s,
        MutableBufferSequence const& b)
        : d_(std::forward<DeducedHandler>(h), s, b)
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
        Handler, decltype(std::declval<NextLayer&>().get_executor())>;

    executor_type
    get_executor() const noexcept
    {
        return (boost::asio::get_associated_executor)(
            d_.handler(), d_->s.get_executor());
    }

    void
    operator()(
        boost::system::error_code ec,
        std::size_t bytes_transferred);

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, read_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(op->d_.handler()));
    }
};

template<class NextLayer>
template<class MutableBufferSequence, class Handler>
void
icy_stream<NextLayer>::
read_op<MutableBufferSequence, Handler>::
operator()(
    error_code ec,
    std::size_t bytes_transferred)
{
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    using iterator = boost::asio::buffers_iterator<
        typename detail::dynamic_buffer_ref<
            buffers_adapter<MutableBufferSequence>>::const_buffers_type>;
    auto& d = *d_;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        if(d.b.max_size() == 0)
        {
            BOOST_ASIO_CORO_YIELD
            boost::asio::post(d.s.get_executor(),
                bind_handler(std::move(*this), ec, 0));
            goto upcall;
        }
        if(! d.s.detect_)
        {
            if(d.s.copy_ > 0)
            {
                auto const n = buffer_copy(
                    d.b.prepare(std::min<std::size_t>(
                        d.s.copy_, d.b.max_size())),
                    boost::asio::buffer(d.s.buf_));
                d.b.commit(n);
                d.s.copy_ = static_cast<unsigned char>(
                    d.s.copy_ - n);
                if(d.s.copy_ > 0)
                    std::memmove(
                        d.s.buf_,
                        &d.s.buf_[n],
                        d.s.copy_);
            }
            if(d.b.size() < d.b.max_size())
            {
                BOOST_ASIO_CORO_YIELD
                d.s.next_layer().async_read_some(
                    d.b.prepare(d.b.max_size() - d.b.size()),
                    std::move(*this));
                d.b.commit(bytes_transferred);
            }
            bytes_transferred = d.b.size();
            goto upcall;
        }

        d.s.detect_ = false;
        if(d.b.max_size() < 8)
        {
            BOOST_ASIO_CORO_YIELD
            boost::asio::async_read(
                d.s.next_layer(),
                boost::asio::buffer(d.s.buf_, 3),
                std::move(*this));
            if(ec)
                goto upcall;
            auto n = bytes_transferred;
            BOOST_ASSERT(n == 3);
            if(
                d.s.buf_[0] != 'I' ||
                d.s.buf_[1] != 'C' ||
                d.s.buf_[2] != 'Y')
            {
                buffer_copy(
                    d.b.value(),
                    boost::asio::buffer(d.s.buf_, n));
                if(d.b.max_size() < 3)
                {
                    d.s.copy_ = static_cast<unsigned char>(
                        3 - d.b.max_size());
                    std::memmove(
                        d.s.buf_,
                        &d.s.buf_[d.b.max_size()],
                        d.s.copy_);

                }
                bytes_transferred = (std::min)(
                    n, d.b.max_size());
                goto upcall;
            }
            d.s.copy_ = static_cast<unsigned char>(
                buffer_copy(
                    boost::asio::buffer(d.s.buf_),
                    icy_stream::version() + d.b.max_size()));
            bytes_transferred = buffer_copy(
                d.b.value(),
                icy_stream::version());
            goto upcall;
        }

        BOOST_ASIO_CORO_YIELD
        boost::asio::async_read_until(
            d.s.next_layer(),
            detail::ref(d.b),
            detail::match_icy<iterator>(d.match),
            std::move(*this));
        if(ec)
            goto upcall;
        {
            auto n = bytes_transferred;
            BOOST_ASSERT(n == d.b.size());
            if(! d.match)
                goto upcall;
            if(d.b.size() + 5 > d.b.max_size())
            {
                d.s.copy_ = static_cast<unsigned char>(
                    n + 5 - d.b.max_size());
                std::copy(
                    boost::asio::buffers_begin(d.b.value()) + n - d.s.copy_,
                    boost::asio::buffers_begin(d.b.value()) + n,
                    d.s.buf_);
                n = d.b.max_size() - 5;
            }
            {
                buffers_suffix<beast::detail::buffers_ref<
                    MutableBufferSequence>> dest(
                        boost::in_place_init, d.b.value());
                dest.consume(5);
                detail::buffer_shift(
                    buffers_prefix(n, dest),
                    buffers_prefix(n, d.b.value()));
                buffer_copy(d.b.value(), icy_stream::version());
                n += 5;
                bytes_transferred = n;
            }
        }
    upcall:
        d_.invoke(ec, bytes_transferred);
    }
}

//------------------------------------------------------------------------------

template<class NextLayer>
template<class... Args>
icy_stream<NextLayer>::
icy_stream(Args&&... args)
    : stream_(std::forward<Args>(args)...)
{
}

template<class NextLayer>
template<class MutableBufferSequence>
std::size_t
icy_stream<NextLayer>::
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
icy_stream<NextLayer>::
read_some(MutableBufferSequence const& buffers, error_code& ec)
{
    static_assert(boost::beast::is_sync_read_stream<next_layer_type>::value,
        "SyncReadStream requirements not met");
    static_assert(boost::asio::is_mutable_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence requirements not met");
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    using iterator = boost::asio::buffers_iterator<
        typename detail::dynamic_buffer_ref<
            buffers_adapter<MutableBufferSequence>>::const_buffers_type>;
    buffers_adapter<MutableBufferSequence> b(buffers);
    if(b.max_size() == 0)
    {
        ec.assign(0, ec.category());
        return 0;
    }
    if(! detect_)
    {
        if(copy_ > 0)
        {
            auto const n = buffer_copy(
                b.prepare(std::min<std::size_t>(
                    copy_, b.max_size())),
                boost::asio::buffer(buf_));
            b.commit(n);
            copy_ = static_cast<unsigned char>(
                copy_ - n);
            if(copy_ > 0)
                std::memmove(
                    buf_,
                    &buf_[n],
                    copy_);
        }
        if(b.size() < b.max_size())
            b.commit(stream_.read_some(
                b.prepare(b.max_size() - b.size()), ec));
        return b.size();
    }

    detect_ = false;
    if(b.max_size() < 8)
    {
        auto n = boost::asio::read(
            stream_,
            boost::asio::buffer(buf_, 3),
            ec);
        if(ec)
            return 0;
        BOOST_ASSERT(n == 3);
        if(
            buf_[0] != 'I' ||
            buf_[1] != 'C' ||
            buf_[2] != 'Y')
        {
            buffer_copy(
                buffers,
                boost::asio::buffer(buf_, n));
            if(b.max_size() < 3)
            {
                copy_ = static_cast<unsigned char>(
                    3 - b.max_size());
                std::memmove(
                    buf_,
                    &buf_[b.max_size()],
                    copy_);

            }
            return (std::min)(n, b.max_size());
        }
        copy_ = static_cast<unsigned char>(
            buffer_copy(
                boost::asio::buffer(buf_),
                version() + b.max_size()));
        return buffer_copy(
            buffers,
            version());
    }

    bool match = false;
    auto n = boost::asio::read_until(
        stream_,
        detail::ref(b),
        detail::match_icy<iterator>(match),
        ec);
    if(ec)
        return n;
    BOOST_ASSERT(n == b.size());
    if(! match)
        return n;
    if(b.size() + 5 > b.max_size())
    {
        copy_ = static_cast<unsigned char>(
            n + 5 - b.max_size());
        std::copy(
            boost::asio::buffers_begin(buffers) + n - copy_,
            boost::asio::buffers_begin(buffers) + n,
            buf_);
        n = b.max_size() - 5;
    }
    buffers_suffix<beast::detail::buffers_ref<
        MutableBufferSequence>> dest(
            boost::in_place_init, buffers);
    dest.consume(5);
    detail::buffer_shift(
        buffers_prefix(n, dest),
        buffers_prefix(n, buffers));
    buffer_copy(buffers, version());
    n += 5;
    return n;
}

template<class NextLayer>
template<
    class MutableBufferSequence,
    class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
icy_stream<NextLayer>::
async_read_some(
    MutableBufferSequence const& buffers,
    ReadHandler&& handler)
{
    static_assert(boost::beast::is_async_read_stream<next_layer_type>::value,
        "AsyncReadStream requirements not met");
    static_assert(boost::asio::is_mutable_buffer_sequence<
            MutableBufferSequence >::value,
        "MutableBufferSequence  requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    read_op<
        MutableBufferSequence,
        BOOST_ASIO_HANDLER_TYPE(
            ReadHandler, void(error_code, std::size_t))>{
                std::move(init.completion_handler), *this, buffers}(
                    {}, 0);
    return init.result.get();
}

template<class NextLayer>
template<class MutableBufferSequence>
std::size_t
icy_stream<NextLayer>::
write_some(MutableBufferSequence const& buffers)
{
    static_assert(boost::beast::is_sync_write_stream<next_layer_type>::value,
        "SyncWriteStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence requirements not met");
    return stream_.write_some(buffers);
}

template<class NextLayer>
template<class MutableBufferSequence>
std::size_t
icy_stream<NextLayer>::
write_some(MutableBufferSequence const& buffers, error_code& ec)
{
    static_assert(boost::beast::is_sync_write_stream<next_layer_type>::value,
        "SyncWriteStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence requirements not met");
    return stream_.write_some(buffers, ec);
}

template<class NextLayer>
template<
    class MutableBufferSequence,
    class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code, std::size_t))
icy_stream<NextLayer>::
async_write_some(
    MutableBufferSequence const& buffers,
    WriteHandler&& handler)
{
    static_assert(boost::beast::is_async_write_stream<next_layer_type>::value,
        "AsyncWriteStream requirements not met");
    static_assert(boost::asio::is_const_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    return stream_.async_write_some(buffers, std::forward<WriteHandler>(handler));
}

} // http
} // beast
} // boost

#endif
