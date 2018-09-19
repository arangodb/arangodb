//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_IMPL_READ_IPP_HPP
#define BOOST_BEAST_HTTP_IMPL_READ_IPP_HPP

#include <boost/beast/http/type_traits.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/read_size.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/asio/post.hpp>
#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/optional.hpp>
#include <boost/throw_exception.hpp>

namespace boost {
namespace beast {
namespace http {

namespace detail {

//------------------------------------------------------------------------------

template<class Stream, class DynamicBuffer,
    bool isRequest, class Derived, class Handler>
class read_some_op
    : public boost::asio::coroutine
{
    Stream& s_;
    boost::asio::executor_work_guard<decltype(
        std::declval<Stream&>().get_executor())> wg_;
    DynamicBuffer& b_;
    basic_parser<isRequest, Derived>& p_;
    std::size_t bytes_transferred_ = 0;
    Handler h_;
    bool cont_ = false;

public:
    read_some_op(read_some_op&&) = default;
    read_some_op(read_some_op const&) = delete;

    template<class DeducedHandler>
    read_some_op(DeducedHandler&& h, Stream& s,
        DynamicBuffer& b, basic_parser<isRequest, Derived>& p)
        : s_(s)
        , wg_(s_.get_executor())
        , b_(b)
        , p_(p)
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
        Handler, decltype(std::declval<Stream&>().get_executor())>;

    executor_type
    get_executor() const noexcept
    {
        return (boost::asio::get_associated_executor)(
            h_, s_.get_executor());
    }

    void
    operator()(
        error_code ec,
        std::size_t bytes_transferred = 0,
        bool cont = true);

    friend
    bool asio_handler_is_continuation(read_some_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return op->cont_ ? true :
            asio_handler_is_continuation(
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

template<class Stream, class DynamicBuffer,
    bool isRequest, class Derived, class Handler>
void
read_some_op<Stream, DynamicBuffer,
    isRequest, Derived, Handler>::
operator()(
    error_code ec,
    std::size_t bytes_transferred,
    bool cont)
{
    cont_ = cont;
    boost::optional<typename
        DynamicBuffer::mutable_buffers_type> mb;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        if(b_.size() == 0)
            goto do_read;
        for(;;)
        {
            // parse
            {
                auto const used = p_.put(b_.data(), ec);
                bytes_transferred_ += used;
                b_.consume(used);
            }
            if(ec != http::error::need_more)
                break;

        do_read:
            try
            {
                mb.emplace(b_.prepare(
                    read_size_or_throw(b_, 65536)));
            }
            catch(std::length_error const&)
            {
                ec = error::buffer_overflow;
                break;
            }
            BOOST_ASIO_CORO_YIELD
            s_.async_read_some(*mb, std::move(*this));
            if(ec == boost::asio::error::eof)
            {
                BOOST_ASSERT(bytes_transferred == 0);
                if(p_.got_some())
                {
                    // caller sees EOF on next read
                    ec.assign(0, ec.category());
                    p_.put_eof(ec);
                    if(ec)
                        goto upcall;
                    BOOST_ASSERT(p_.is_done());
                    goto upcall;
                }
                ec = error::end_of_stream;
                break;
            }
            if(ec)
                break;
            b_.commit(bytes_transferred);
        }

    upcall:
        if(! cont_)
        {
            BOOST_ASIO_CORO_YIELD
            boost::asio::post(
                s_.get_executor(),
                bind_handler(std::move(*this),
                    ec, bytes_transferred_));
        }
        h_(ec, bytes_transferred_);
    }
}

//------------------------------------------------------------------------------

struct parser_is_done
{
    template<bool isRequest, class Derived>
    bool
    operator()(basic_parser<
        isRequest, Derived> const& p) const
    {
        return p.is_done();
    }
};

struct parser_is_header_done
{
    template<bool isRequest, class Derived>
    bool
    operator()(basic_parser<
        isRequest, Derived> const& p) const
    {
        return p.is_header_done();
    }
};

template<class Stream, class DynamicBuffer,
    bool isRequest, class Derived, class Condition,
        class Handler>
class read_op
    : public boost::asio::coroutine
{
    Stream& s_;
    boost::asio::executor_work_guard<decltype(
        std::declval<Stream&>().get_executor())> wg_;
    DynamicBuffer& b_;
    basic_parser<isRequest, Derived>& p_;
    std::size_t bytes_transferred_ = 0;
    Handler h_;
    bool cont_ = false;

public:
    read_op(read_op&&) = default;
    read_op(read_op const&) = delete;

    template<class DeducedHandler>
    read_op(DeducedHandler&& h, Stream& s,
        DynamicBuffer& b, basic_parser<isRequest,
            Derived>& p)
        : s_(s)
        , wg_(s_.get_executor())
        , b_(b)
        , p_(p)
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
        Handler, decltype(std::declval<Stream&>().get_executor())>;

    executor_type
    get_executor() const noexcept
    {
        return (boost::asio::get_associated_executor)(
            h_, s_.get_executor());
    }

    void
    operator()(
        error_code ec,
        std::size_t bytes_transferred = 0,
        bool cont = true);

    friend
    bool asio_handler_is_continuation(read_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return op->cont_ ? true :
            asio_handler_is_continuation(
                std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, read_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(op->h_));
    }
};

template<class Stream, class DynamicBuffer,
    bool isRequest, class Derived, class Condition,
        class Handler>
void
read_op<Stream, DynamicBuffer,
    isRequest, Derived, Condition, Handler>::
operator()(
    error_code ec,
    std::size_t bytes_transferred,
    bool cont)
{
    cont_ = cont;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        if(Condition{}(p_))
        {
            BOOST_ASIO_CORO_YIELD
            boost::asio::post(s_.get_executor(),
                bind_handler(std::move(*this), ec));
            goto upcall;
        }
        for(;;)
        {
            BOOST_ASIO_CORO_YIELD
            async_read_some(
                s_, b_, p_, std::move(*this));
            if(ec)
                goto upcall;
            bytes_transferred_ += bytes_transferred;
            if(Condition{}(p_))
                goto upcall;
        }
    upcall:
        h_(ec, bytes_transferred_);
    }
}

//------------------------------------------------------------------------------

template<class Stream, class DynamicBuffer,
    bool isRequest, class Body, class Allocator,
        class Handler>
class read_msg_op
    : public boost::asio::coroutine
{
    using parser_type =
        parser<isRequest, Body, Allocator>;

    using message_type =
        typename parser_type::value_type;

    struct data
    {
        Stream& s;
        boost::asio::executor_work_guard<decltype(
            std::declval<Stream&>().get_executor())> wg;
        DynamicBuffer& b;
        message_type& m;
        parser_type p;
        std::size_t bytes_transferred = 0;
        bool cont = false;

        data(Handler const&, Stream& s_,
                DynamicBuffer& b_, message_type& m_)
            : s(s_)
            , wg(s.get_executor())
            , b(b_)
            , m(m_)
            , p(std::move(m))
        {
            p.eager(true);
        }
    };

    handler_ptr<data, Handler> d_;

public:
    read_msg_op(read_msg_op&&) = default;
    read_msg_op(read_msg_op const&) = delete;

    template<class DeducedHandler, class... Args>
    read_msg_op(DeducedHandler&& h, Stream& s, Args&&... args)
        : d_(std::forward<DeducedHandler>(h),
            s, std::forward<Args>(args)...)
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
        Handler, decltype(std::declval<Stream&>().get_executor())>;

    executor_type
    get_executor() const noexcept
    {
        return (boost::asio::get_associated_executor)(
            d_.handler(), d_->s.get_executor());
    }

    void
    operator()(
        error_code ec,
        std::size_t bytes_transferred = 0,
        bool cont = true);

    friend
    bool asio_handler_is_continuation(read_msg_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return op->d_->cont ? true :
            asio_handler_is_continuation(
                std::addressof(op->d_.handler()));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, read_msg_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(op->d_.handler()));
    }
};

template<class Stream, class DynamicBuffer,
    bool isRequest, class Body, class Allocator,
        class Handler>
void
read_msg_op<Stream, DynamicBuffer,
    isRequest, Body, Allocator, Handler>::
operator()(
    error_code ec,
    std::size_t bytes_transferred,
    bool cont)
{
    auto& d = *d_;
    d.cont = cont;
    BOOST_ASIO_CORO_REENTER(*this)
    {
        for(;;)
        {
            BOOST_ASIO_CORO_YIELD
            async_read_some(
                d.s, d.b, d.p, std::move(*this));
            if(ec)
                goto upcall;
            d.bytes_transferred +=
                bytes_transferred;
            if(d.p.is_done())
            {
                d.m = d.p.release();
                goto upcall;
            }
        }
    upcall:
        bytes_transferred = d.bytes_transferred;
        {
            auto wg = std::move(d.wg);
            d_.invoke(ec, bytes_transferred);
        }
    }
}

} // detail

//------------------------------------------------------------------------------

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read_some(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser)
{
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(! parser.is_done());
    error_code ec;
    auto const bytes_transferred =
        read_some(stream, buffer, parser, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read_some(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    error_code& ec)
{
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(! parser.is_done());
    std::size_t bytes_transferred = 0;
    if(buffer.size() == 0)
        goto do_read;
    for(;;)
    {
        // invoke parser
        {
            auto const n = parser.put(buffer.data(), ec);
            bytes_transferred += n;
            buffer.consume(n);
            if(! ec)
                break;
            if(ec != http::error::need_more)
                break;
        }
    do_read:
        boost::optional<typename
            DynamicBuffer::mutable_buffers_type> b;
        try
        {
            b.emplace(buffer.prepare(
                read_size_or_throw(buffer, 65536)));
        }
        catch(std::length_error const&)
        {
            ec = error::buffer_overflow;
            return bytes_transferred;
        }
        auto const n = stream.read_some(*b, ec);
        if(ec == boost::asio::error::eof)
        {
            BOOST_ASSERT(n == 0);
            if(parser.got_some())
            {
                // caller sees EOF on next read
                parser.put_eof(ec);
                if(ec)
                    break;
                BOOST_ASSERT(parser.is_done());
                break;
            }
            ec = error::end_of_stream;
            break;
        }
        if(ec)
            break;
        buffer.commit(n);
    }
    return bytes_transferred;
}

template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived,
    class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
async_read_some(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    ReadHandler&& handler)
{
    static_assert(is_async_read_stream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    BOOST_ASSERT(! parser.is_done());
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    detail::read_some_op<AsyncReadStream,
        DynamicBuffer, isRequest, Derived, BOOST_ASIO_HANDLER_TYPE(
            ReadHandler, void(error_code, std::size_t))>{
                std::move(init.completion_handler), stream, buffer, parser}(
                    {}, 0, false);
    return init.result.get();
}

//------------------------------------------------------------------------------

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read_header(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser)
{
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    error_code ec;
    auto const bytes_transferred =
        read_header(stream, buffer, parser, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read_header(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    error_code& ec)
{
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    parser.eager(false);
    if(parser.is_header_done())
    {
        ec.assign(0, ec.category());
        return 0;
    }
    std::size_t bytes_transferred = 0;
    do
    {
        bytes_transferred += read_some(
            stream, buffer, parser, ec);
        if(ec)
            return bytes_transferred;
    }
    while(! parser.is_header_done());
    return bytes_transferred;
}

template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived,
    class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
async_read_header(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    ReadHandler&& handler)
{
    static_assert(is_async_read_stream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    parser.eager(false);
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    detail::read_op<AsyncReadStream, DynamicBuffer,
        isRequest, Derived, detail::parser_is_header_done,
            BOOST_ASIO_HANDLER_TYPE(ReadHandler, void(error_code, std::size_t))>{
                std::move(init.completion_handler), stream,
                buffer, parser}({}, 0, false);
    return init.result.get();
}

//------------------------------------------------------------------------------

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser)
{
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    error_code ec;
    auto const bytes_transferred =
        read(stream, buffer, parser, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived>
std::size_t
read(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    error_code& ec)
{
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    parser.eager(true);
    if(parser.is_done())
    {
        ec.assign(0, ec.category());
        return 0;
    }
    std::size_t bytes_transferred = 0;
    do
    {
        bytes_transferred += read_some(
            stream, buffer, parser, ec);
        if(ec)
            return bytes_transferred;
    }
    while(! parser.is_done());
    return bytes_transferred;
}

template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Derived,
    class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
async_read(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    basic_parser<isRequest, Derived>& parser,
    ReadHandler&& handler)
{
    static_assert(is_async_read_stream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    parser.eager(true);
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    detail::read_op<AsyncReadStream, DynamicBuffer,
        isRequest, Derived, detail::parser_is_done,
            BOOST_ASIO_HANDLER_TYPE(ReadHandler, void(error_code, std::size_t))>{
                std::move(init.completion_handler), stream, buffer, parser}(
                    {}, 0, false);
    return init.result.get();
}

//------------------------------------------------------------------------------

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Body, class Allocator>
std::size_t
read(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    message<isRequest, Body, basic_fields<Allocator>>& msg)
{
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    error_code ec;
    auto const bytes_transferred =
        read(stream, buffer, msg, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Body, class Allocator>
std::size_t
read(
    SyncReadStream& stream,
    DynamicBuffer& buffer,
    message<isRequest, Body, basic_fields<Allocator>>& msg,
    error_code& ec)
{
    static_assert(is_sync_read_stream<SyncReadStream>::value,
        "SyncReadStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    parser<isRequest, Body, Allocator> p{std::move(msg)};
    p.eager(true);
    auto const bytes_transferred =
        read(stream, buffer, p.base(), ec);
    if(ec)
        return bytes_transferred;
    msg = p.release();
    return bytes_transferred;
}

template<
    class AsyncReadStream,
    class DynamicBuffer,
    bool isRequest, class Body, class Allocator,
    class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
async_read(
    AsyncReadStream& stream,
    DynamicBuffer& buffer,
    message<isRequest, Body, basic_fields<Allocator>>& msg,
    ReadHandler&& handler)
{
    static_assert(is_async_read_stream<AsyncReadStream>::value,
        "AsyncReadStream requirements not met");
    static_assert(
        boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
        "DynamicBuffer requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_reader<Body>::value,
        "BodyReader requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    detail::read_msg_op<
        AsyncReadStream,
        DynamicBuffer,
        isRequest, Body, Allocator,
        BOOST_ASIO_HANDLER_TYPE(
            ReadHandler, void(error_code, std::size_t))>{
                std::move(init.completion_handler), stream, buffer, msg}(
                    {}, 0, false);
    return init.result.get();
}

} // http
} // beast
} // boost

#endif
