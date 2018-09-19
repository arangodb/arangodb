//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_HTTP_IMPL_WRITE_IPP
#define BOOST_BEAST_HTTP_IMPL_WRITE_IPP

#include <boost/beast/http/type_traits.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/write.hpp>
#include <boost/optional.hpp>
#include <boost/throw_exception.hpp>
#include <ostream>
#include <sstream>

namespace boost {
namespace beast {
namespace http {
namespace detail {

template<
    class Stream, class Handler,
    bool isRequest, class Body, class Fields>
class write_some_op
{
    Stream& s_;
    boost::asio::executor_work_guard<decltype(
        std::declval<Stream&>().get_executor())> wg_;
    serializer<isRequest,Body, Fields>& sr_;
    Handler h_;

    class lambda
    {
        write_some_op& op_;

    public:
        bool invoked = false;

        explicit
        lambda(write_some_op& op)
            : op_(op)
        {
        }

        template<class ConstBufferSequence>
        void
        operator()(error_code& ec,
            ConstBufferSequence const& buffers)
        {
            invoked = true;
            ec.assign(0, ec.category());
            return op_.s_.async_write_some(
                buffers, std::move(op_));
        }
    };

public:
    write_some_op(write_some_op&&) = default;
    write_some_op(write_some_op const&) = delete;

    template<class DeducedHandler>
    write_some_op(DeducedHandler&& h, Stream& s,
            serializer<isRequest, Body, Fields>& sr)
        : s_(s)
        , wg_(s_.get_executor())
        , sr_(sr)
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
    operator()();

    void
    operator()(
        error_code ec,
        std::size_t bytes_transferred);

    friend
    bool asio_handler_is_continuation(write_some_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, write_some_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(op->h_));
    }
};

template<
    class Stream, class Handler,
    bool isRequest, class Body, class Fields>
void
write_some_op<
    Stream, Handler, isRequest, Body, Fields>::
operator()()
{
    error_code ec;
    if(! sr_.is_done())
    {
        lambda f{*this};
        sr_.next(ec, f);
        if(ec)
        {
            BOOST_ASSERT(! f.invoked);
            return boost::asio::post(
                s_.get_executor(),
                bind_handler(std::move(*this), ec, 0));
        }
        if(f.invoked)
        {
            // *this has been moved from,
            // cannot access members here.
            return;
        }
        // What else could it be?
        BOOST_ASSERT(sr_.is_done());
    }
    return boost::asio::post(
        s_.get_executor(),
        bind_handler(std::move(*this), ec, 0));
}

template<
    class Stream, class Handler,
    bool isRequest, class Body, class Fields>
void
write_some_op<
    Stream, Handler, isRequest, Body, Fields>::
operator()(
    error_code ec, std::size_t bytes_transferred)
{
    if(! ec)
        sr_.consume(bytes_transferred);
    h_(ec, bytes_transferred);
}

//------------------------------------------------------------------------------

struct serializer_is_header_done
{
    template<
        bool isRequest, class Body, class Fields>
    bool
    operator()(
        serializer<isRequest, Body, Fields>& sr) const
    {
        return sr.is_header_done();
    }
};

struct serializer_is_done
{
    template<
        bool isRequest, class Body, class Fields>
    bool
    operator()(
        serializer<isRequest, Body, Fields>& sr) const
    {
        return sr.is_done();
    }
};

//------------------------------------------------------------------------------

template<
    class Stream, class Handler, class Predicate,
    bool isRequest, class Body, class Fields>
class write_op : public boost::asio::coroutine
{
    Stream& s_;
    boost::asio::executor_work_guard<decltype(
        std::declval<Stream&>().get_executor())> wg_;
    serializer<isRequest, Body, Fields>& sr_;
    std::size_t bytes_transferred_ = 0;
    Handler h_;
    bool cont_;

public:
    write_op(write_op&&) = default;
    write_op(write_op const&) = delete;

    template<class DeducedHandler>
    write_op(DeducedHandler&& h, Stream& s,
            serializer<isRequest, Body, Fields>& sr)
        : s_(s)
        , wg_(s_.get_executor())
        , sr_(sr)
        , h_(std::forward<DeducedHandler>(h))
        , cont_([&]
            {
                using boost::asio::asio_handler_is_continuation;
                return asio_handler_is_continuation(
                    std::addressof(h_));
            }())
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
        error_code ec = {},
        std::size_t bytes_transferred = 0);

    friend
    bool asio_handler_is_continuation(write_op* op)
    {
        return op->cont_;
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, write_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(op->h_));
    }
};

template<
    class Stream, class Handler, class Predicate,
    bool isRequest, class Body, class Fields>
void
write_op<Stream, Handler, Predicate,
    isRequest, Body, Fields>::
operator()(
    error_code ec,
    std::size_t bytes_transferred)
{
    BOOST_ASIO_CORO_REENTER(*this)
    {
        if(Predicate{}(sr_))
        {
            BOOST_ASIO_CORO_YIELD
            boost::asio::post(
                s_.get_executor(),
                bind_handler(std::move(*this)));
            goto upcall;
        }
        for(;;)
        {
            BOOST_ASIO_CORO_YIELD
            beast::http::async_write_some(
                s_, sr_, std::move(*this));
            bytes_transferred_ += bytes_transferred;
            if(ec)
                goto upcall;
            if(Predicate{}(sr_))
                break;
            cont_ = true;
        }
    upcall:
        h_(ec, bytes_transferred_);
    }
}

//------------------------------------------------------------------------------

template<class Stream, class Handler,
    bool isRequest, class Body, class Fields>
class write_msg_op
{
    struct data
    {
        Stream& s;
        boost::asio::executor_work_guard<decltype(
            std::declval<Stream&>().get_executor())> wg;
        serializer<isRequest, Body, Fields> sr;

        data(Handler const&, Stream& s_, message<
                isRequest, Body, Fields>& m_)
            : s(s_)
            , wg(s.get_executor())
            , sr(m_)
        {
        }

        data(Handler const&, Stream& s_, message<
                isRequest, Body, Fields> const& m_)
            : s(s_)
            , wg(s.get_executor())
            , sr(m_)
        {
        }
    };

    handler_ptr<data, Handler> d_;

public:
    write_msg_op(write_msg_op&&) = default;
    write_msg_op(write_msg_op const&) = delete;

    template<class DeducedHandler, class... Args>
    write_msg_op(DeducedHandler&& h, Stream& s, Args&&... args)
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
    operator()();

    void
    operator()(
        error_code ec, std::size_t bytes_transferred);

    friend
    bool asio_handler_is_continuation(write_msg_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->d_.handler()));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, write_msg_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(op->d_.handler()));
    }
};

template<class Stream, class Handler,
    bool isRequest, class Body, class Fields>
void
write_msg_op<
    Stream, Handler, isRequest, Body, Fields>::
operator()()
{
    auto& d = *d_;
    return async_write(d.s, d.sr, std::move(*this));
}

template<class Stream, class Handler,
    bool isRequest, class Body, class Fields>
void
write_msg_op<
    Stream, Handler, isRequest, Body, Fields>::
operator()(error_code ec, std::size_t bytes_transferred)
{
    auto wg = std::move(d_->wg);
    d_.invoke(ec, bytes_transferred);
}

//------------------------------------------------------------------------------

template<class Stream>
class write_some_lambda
{
    Stream& stream_;

public:
    bool invoked = false;
    std::size_t bytes_transferred = 0;

    explicit
    write_some_lambda(Stream& stream)
        : stream_(stream)
    {
    }

    template<class ConstBufferSequence>
    void
    operator()(error_code& ec,
        ConstBufferSequence const& buffers)
    {
        invoked = true;
        bytes_transferred =
            stream_.write_some(buffers, ec);
    }
};

template<class Stream>
class write_lambda
{
    Stream& stream_;

public:
    bool invoked = false;
    std::size_t bytes_transferred = 0;

    explicit
    write_lambda(Stream& stream)
        : stream_(stream)
    {
    }

    template<class ConstBufferSequence>
    void
    operator()(error_code& ec,
        ConstBufferSequence const& buffers)
    {
        invoked = true;
        bytes_transferred = boost::asio::write(
            stream_, buffers, ec);
    }
};

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
std::size_t
write_some_impl(
    SyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr,
    error_code& ec)
{
    if(! sr.is_done())
    {
        write_some_lambda<SyncWriteStream> f{stream};
        sr.next(ec, f);
        if(ec)
            return f.bytes_transferred;
        if(f.invoked)
            sr.consume(f.bytes_transferred);
        return f.bytes_transferred;
    }
    ec.assign(0, ec.category());
    return 0;
}

template<
    class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
    class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code, std::size_t))
async_write_some_impl(
    AsyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr,
    WriteHandler&& handler)
{
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(error_code, std::size_t));
    detail::write_some_op<
        AsyncWriteStream,
        BOOST_ASIO_HANDLER_TYPE(WriteHandler,
            void(error_code, std::size_t)),
        isRequest, Body, Fields>{
            std::move(init.completion_handler), stream, sr}();
    return init.result.get();
}

} // detail

//------------------------------------------------------------------------------

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
std::size_t
write_some(
    SyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
    error_code ec;
    auto const bytes_transferred =
        write_some(stream, sr, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
std::size_t
write_some(
    SyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr,
    error_code& ec)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
    return detail::write_some_impl(stream, sr, ec);
}

template<
    class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
    class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code, std::size_t))
async_write_some(
    AsyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr,
    WriteHandler&& handler)
{
    static_assert(is_async_write_stream<
            AsyncWriteStream>::value,
        "AsyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
    return detail::async_write_some_impl(stream, sr,
        std::forward<WriteHandler>(handler));
}

//------------------------------------------------------------------------------

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
std::size_t
write_header(SyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
    error_code ec;
    auto const bytes_transferred =
        write_header(stream, sr, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
std::size_t
write_header(
    SyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr,
    error_code& ec)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
    sr.split(true);
    std::size_t bytes_transferred = 0;
    if(! sr.is_header_done())
    {
        detail::write_lambda<SyncWriteStream> f{stream};
        do
        {
            sr.next(ec, f);
            bytes_transferred += f.bytes_transferred;
            if(ec)
                return bytes_transferred;
            BOOST_ASSERT(f.invoked);
            sr.consume(f.bytes_transferred);
        }
        while(! sr.is_header_done());
    }
    else
    {
        ec.assign(0, ec.category());
    }
    return bytes_transferred;
}

template<
    class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
    class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code, std::size_t))
async_write_header(
    AsyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr,
    WriteHandler&& handler)
{
    static_assert(is_async_write_stream<
            AsyncWriteStream>::value,
        "AsyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
    sr.split(true);
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(error_code, std::size_t));
    detail::write_op<
        AsyncWriteStream,
        BOOST_ASIO_HANDLER_TYPE(WriteHandler,
            void(error_code, std::size_t)),
        detail::serializer_is_header_done,
        isRequest, Body, Fields>{
        std::move(init.completion_handler), stream, sr}();
    return init.result.get();
}

//------------------------------------------------------------------------------

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
std::size_t
write(
    SyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    error_code ec;
    auto const bytes_transferred =
        write(stream, sr, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
std::size_t
write(
    SyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr,
    error_code& ec)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    std::size_t bytes_transferred = 0;
    sr.split(false);
    for(;;)
    {
        bytes_transferred +=
            write_some(stream, sr, ec);
        if(ec)
            return bytes_transferred;
        if(sr.is_done())
            break;
    }
    return bytes_transferred;
}

template<
    class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
    class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code, std::size_t))
async_write(
    AsyncWriteStream& stream,
    serializer<isRequest, Body, Fields>& sr,
    WriteHandler&& handler)
{
    static_assert(is_async_write_stream<
            AsyncWriteStream>::value,
        "AsyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
    sr.split(false);
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(error_code, std::size_t));
    detail::write_op<
        AsyncWriteStream,
        BOOST_ASIO_HANDLER_TYPE(WriteHandler,
            void(error_code, std::size_t)),
        detail::serializer_is_done,
        isRequest, Body, Fields>{
            std::move(init.completion_handler), stream, sr}();
    return init.result.get();
}

//------------------------------------------------------------------------------

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
typename std::enable_if<
    is_mutable_body_writer<Body>::value,
    std::size_t>::type
write(
    SyncWriteStream& stream,
    message<isRequest, Body, Fields>& msg)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
    error_code ec;
    auto const bytes_transferred =
        write(stream, msg, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
typename std::enable_if<
    ! is_mutable_body_writer<Body>::value,
    std::size_t>::type
write(
    SyncWriteStream& stream,
    message<isRequest, Body, Fields> const& msg)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
    error_code ec;
    auto const bytes_transferred =
        write(stream, msg, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
typename std::enable_if<
    is_mutable_body_writer<Body>::value,
    std::size_t>::type
write(
    SyncWriteStream& stream,
    message<isRequest, Body, Fields>& msg,
    error_code& ec)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
    serializer<isRequest, Body, Fields> sr{msg};
    return write(stream, sr, ec);
}

template<
    class SyncWriteStream,
    bool isRequest, class Body, class Fields>
typename std::enable_if<
    ! is_mutable_body_writer<Body>::value,
    std::size_t>::type
write(
    SyncWriteStream& stream,
    message<isRequest, Body, Fields> const& msg,
    error_code& ec)
{
    static_assert(is_sync_write_stream<SyncWriteStream>::value,
        "SyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
    serializer<isRequest, Body, Fields> sr{msg};
    return write(stream, sr, ec);
}

template<
    class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
    class WriteHandler>
typename std::enable_if<
    is_mutable_body_writer<Body>::value,
    BOOST_ASIO_INITFN_RESULT_TYPE(
        WriteHandler, void(error_code, std::size_t))>::type
async_write(
    AsyncWriteStream& stream,
    message<isRequest, Body, Fields>& msg,
    WriteHandler&& handler)
{
    static_assert(
        is_async_write_stream<AsyncWriteStream>::value,
        "AsyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(error_code, std::size_t));
    detail::write_msg_op<
        AsyncWriteStream,
        BOOST_ASIO_HANDLER_TYPE(WriteHandler,
            void(error_code, std::size_t)),
        isRequest, Body, Fields>{
            std::move(init.completion_handler), stream, msg}();
    return init.result.get();
}

template<
    class AsyncWriteStream,
    bool isRequest, class Body, class Fields,
    class WriteHandler>
typename std::enable_if<
    ! is_mutable_body_writer<Body>::value,
    BOOST_ASIO_INITFN_RESULT_TYPE(
        WriteHandler, void(error_code, std::size_t))>::type
async_write(
    AsyncWriteStream& stream,
    message<isRequest, Body, Fields> const& msg,
    WriteHandler&& handler)
{
    static_assert(
        is_async_write_stream<AsyncWriteStream>::value,
        "AsyncWriteStream requirements not met");
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(error_code, std::size_t));
    detail::write_msg_op<
        AsyncWriteStream,
        BOOST_ASIO_HANDLER_TYPE(WriteHandler,
            void(error_code, std::size_t)),
        isRequest, Body, Fields>{
            std::move(init.completion_handler), stream, msg}();
    return init.result.get();
}

//------------------------------------------------------------------------------

namespace detail {

template<class Serializer>
class write_ostream_lambda
{
    std::ostream& os_;
    Serializer& sr_;

public:
    write_ostream_lambda(std::ostream& os,
            Serializer& sr)
        : os_(os)
        , sr_(sr)
    {
    }

    template<class ConstBufferSequence>
    void
    operator()(error_code& ec,
        ConstBufferSequence const& buffers) const
    {
        ec.assign(0, ec.category());
        if(os_.fail())
            return;
        std::size_t bytes_transferred = 0;
        for(auto b : buffers_range(buffers))
        {
            os_.write(static_cast<char const*>(
                b.data()), b.size());
            if(os_.fail())
                return;
            bytes_transferred += b.size();
        }
        sr_.consume(bytes_transferred);
    }
};

} // detail

template<class Fields>
std::ostream&
operator<<(std::ostream& os,
    header<true, Fields> const& h)
{
    typename Fields::writer fr{
        h, h.version(), h.method()};
    return os << buffers(fr.get());
}

template<class Fields>
std::ostream&
operator<<(std::ostream& os,
    header<false, Fields> const& h)
{
    typename Fields::writer fr{
        h, h.version(), h.result_int()};
    return os << buffers(fr.get());
}

template<bool isRequest, class Body, class Fields>
std::ostream&
operator<<(std::ostream& os,
    message<isRequest, Body, Fields> const& msg)
{
    static_assert(is_body<Body>::value,
        "Body requirements not met");
    static_assert(is_body_writer<Body>::value,
        "BodyWriter requirements not met");
    serializer<isRequest, Body, Fields> sr{msg};
    error_code ec;
    detail::write_ostream_lambda<decltype(sr)> f{os, sr};
    do
    {
        sr.next(ec, f);
        if(os.fail())
            break;
        if(ec)
        {
            os.setstate(std::ios::failbit);
            break;
        }   
    }
    while(! sr.is_done());
    return os;
}

} // http
} // beast
} // boost

#endif
