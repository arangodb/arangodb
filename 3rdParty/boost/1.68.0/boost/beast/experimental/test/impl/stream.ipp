//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_IMPL_STREAM_IPP
#define BOOST_BEAST_TEST_IMPL_STREAM_IPP

#include <boost/beast/core/buffers_prefix.hpp>

namespace boost {
namespace beast {
namespace test {

inline
stream::
~stream()
{
    {
        std::unique_lock<std::mutex> lock{in_->m};
        in_->op.reset();
    }
    auto out = out_.lock();
    if(out)
    {
        std::unique_lock<std::mutex> lock{out->m};
        if(out->code == status::ok)
        {
            out->code = status::reset;
            out->on_write();
        }
    }
}

inline
stream::
stream(stream&& other)
{
    auto in = std::make_shared<state>(
        other.in_->ioc, other.in_->fc);
    in_ = std::move(other.in_);
    out_ = std::move(other.out_);
    other.in_ = in;
}

inline
stream&
stream::
operator=(stream&& other)
{
    auto in = std::make_shared<state>(
        other.in_->ioc, other.in_->fc);
    in_ = std::move(other.in_);
    out_ = std::move(other.out_);
    other.in_ = in;
    return *this;
}

inline
stream::
stream(boost::asio::io_context& ioc)
    : in_(std::make_shared<state>(ioc, nullptr))
{
}

inline
stream::
stream(
    boost::asio::io_context& ioc,
    fail_count& fc)
    : in_(std::make_shared<state>(ioc, &fc))
{
}

inline
stream::
stream(
    boost::asio::io_context& ioc,
    string_view s)
    : in_(std::make_shared<state>(ioc, nullptr))
{
    using boost::asio::buffer;
    using boost::asio::buffer_copy;
    in_->b.commit(buffer_copy(
        in_->b.prepare(s.size()),
        buffer(s.data(), s.size())));
}

inline
stream::
stream(
    boost::asio::io_context& ioc,
    fail_count& fc,
    string_view s)
    : in_(std::make_shared<state>(ioc, &fc))
{
    using boost::asio::buffer;
    using boost::asio::buffer_copy;
    in_->b.commit(buffer_copy(
        in_->b.prepare(s.size()),
        buffer(s.data(), s.size())));
}

inline
void
stream::
connect(stream& remote)
{
    BOOST_ASSERT(! out_.lock());
    BOOST_ASSERT(! remote.out_.lock());
    out_ = remote.in_;
    remote.out_ = in_;
}
inline
string_view
stream::
str() const
{
    auto const bs = in_->b.data();
    if(boost::asio::buffer_size(bs) == 0)
        return {};
    auto const b = buffers_front(bs);
    return {static_cast<char const*>(b.data()), b.size()};
}

inline
void
stream::
append(string_view s)
{
    using boost::asio::buffer;
    using boost::asio::buffer_copy;
    std::lock_guard<std::mutex> lock{in_->m};
    in_->b.commit(buffer_copy(
        in_->b.prepare(s.size()),
        buffer(s.data(), s.size())));
}

inline
void
stream::
clear()
{
    std::lock_guard<std::mutex> lock{in_->m};
    in_->b.consume(in_->b.size());
}

inline
void
stream::
close()
{
    BOOST_ASSERT(! in_->op);
    auto out = out_.lock();
    if(! out)
        return;
    std::lock_guard<std::mutex> lock{out->m};
    if(out->code == status::ok)
    {
        out->code = status::eof;
        out->on_write();
    }
}

inline
void
stream::
close_remote()
{
    std::lock_guard<std::mutex> lock{in_->m};
    if(in_->code == status::ok)
    {
        in_->code = status::eof;
        in_->on_write();
    }
}

template<class MutableBufferSequence>
std::size_t
stream::
read_some(MutableBufferSequence const& buffers)
{
    static_assert(boost::asio::is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    error_code ec;
    auto const n = read_some(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return n;
}

template<class MutableBufferSequence>
std::size_t
stream::
read_some(MutableBufferSequence const& buffers,
    error_code& ec)
{
    static_assert(boost::asio::is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    if(in_->fc && in_->fc->fail(ec))
        return 0;
    if(buffer_size(buffers) == 0)
    {
        ec.clear();
        return 0;
    }
    std::unique_lock<std::mutex> lock{in_->m};
    BOOST_ASSERT(! in_->op);
    in_->cv.wait(lock,
        [&]()
        {
            return
                in_->b.size() > 0 ||
                in_->code != status::ok;
        });
    std::size_t bytes_transferred;
    if(in_->b.size() > 0)
    {   
        ec.assign(0, ec.category());
        bytes_transferred = buffer_copy(
            buffers, in_->b.data(), in_->read_max);
        in_->b.consume(bytes_transferred);
    }
    else
    {
        BOOST_ASSERT(in_->code != status::ok);
        bytes_transferred = 0;
        if(in_->code == status::eof)
            ec = boost::asio::error::eof;
        else if(in_->code == status::reset)
            ec = boost::asio::error::connection_reset;
    }
    ++in_->nread;
    return bytes_transferred;
}

template<class MutableBufferSequence, class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    ReadHandler, void(error_code, std::size_t))
stream::
async_read_some(
    MutableBufferSequence const& buffers,
    ReadHandler&& handler)
{
    static_assert(boost::asio::is_mutable_buffer_sequence<
            MutableBufferSequence>::value,
        "MutableBufferSequence requirements not met");
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    if(in_->fc)
    {
        error_code ec;
        if(in_->fc->fail(ec))
            return boost::asio::post(
                in_->ioc.get_executor(),
                bind_handler(
                    std::move(init.completion_handler),
                    ec,
                    0));
    }
    {
        std::unique_lock<std::mutex> lock{in_->m};
        BOOST_ASSERT(! in_->op);
        if(buffer_size(buffers) == 0 ||
            buffer_size(in_->b.data()) > 0)
        {
            auto const bytes_transferred = buffer_copy(
                buffers, in_->b.data(), in_->read_max);
            in_->b.consume(bytes_transferred);
            lock.unlock();
            ++in_->nread;
            boost::asio::post(
                in_->ioc.get_executor(),
                bind_handler(
                    std::move(init.completion_handler),
                    error_code{},
                    bytes_transferred));
        }
        else if(in_->code != status::ok)
        {
            lock.unlock();
            ++in_->nread;
            error_code ec;
            if(in_->code == status::eof)
                ec = boost::asio::error::eof;
            else if(in_->code == status::reset)
                ec = boost::asio::error::connection_reset;
            boost::asio::post(
                in_->ioc.get_executor(),
                bind_handler(
                    std::move(init.completion_handler),
                    ec,
                    0));
        }
        else
        {
            in_->op.reset(new read_op<BOOST_ASIO_HANDLER_TYPE(
                ReadHandler, void(error_code, std::size_t)),
                    MutableBufferSequence>{*in_, buffers,
                        std::move(init.completion_handler)});
        }
    }
    return init.result.get();
}

template<class ConstBufferSequence>
std::size_t
stream::
write_some(ConstBufferSequence const& buffers)
{
    static_assert(boost::asio::is_const_buffer_sequence<
            ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
    error_code ec;
    auto const bytes_transferred =
        write_some(buffers, ec);
    if(ec)
        BOOST_THROW_EXCEPTION(system_error{ec});
    return bytes_transferred;
}

template<class ConstBufferSequence>
std::size_t
stream::
write_some(
    ConstBufferSequence const& buffers, error_code& ec)
{
    static_assert(boost::asio::is_const_buffer_sequence<
            ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    auto out = out_.lock();
    if(! out)
    {
        ec = boost::asio::error::connection_reset;
        return 0;
    }
    BOOST_ASSERT(out->code == status::ok);
    if(in_->fc && in_->fc->fail(ec))
        return 0;
    auto const n = (std::min)(
        buffer_size(buffers), in_->write_max);
    std::unique_lock<std::mutex> lock{out->m};
    auto const bytes_transferred =
        buffer_copy(out->b.prepare(n), buffers);
    out->b.commit(bytes_transferred);
    out->on_write();
    lock.unlock();
    ++in_->nwrite;
    ec.assign(0, ec.category());
    return bytes_transferred;
}

template<class ConstBufferSequence, class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(
    WriteHandler, void(error_code, std::size_t))
stream::
async_write_some(ConstBufferSequence const& buffers,
    WriteHandler&& handler)
{
    static_assert(boost::asio::is_const_buffer_sequence<
            ConstBufferSequence>::value,
        "ConstBufferSequence requirements not met");
    using boost::asio::buffer_copy;
    using boost::asio::buffer_size;
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(error_code, std::size_t));
    auto out = out_.lock();
    if(! out)
        return boost::asio::post(
            in_->ioc.get_executor(),
            bind_handler(
                std::move(init.completion_handler),
                boost::asio::error::connection_reset,
                0));
    BOOST_ASSERT(out->code == status::ok);
    if(in_->fc)
    {
        error_code ec;
        if(in_->fc->fail(ec))
            return boost::asio::post(
                in_->ioc.get_executor(),
                bind_handler(
                    std::move(init.completion_handler),
                    ec,
                    0));
    }
    auto const n =
        (std::min)(buffer_size(buffers), in_->write_max);
    std::unique_lock<std::mutex> lock{out->m};
    auto const bytes_transferred =
        buffer_copy(out->b.prepare(n), buffers);
    out->b.commit(bytes_transferred);
    out->on_write();
    lock.unlock();
    ++in_->nwrite;
    boost::asio::post(
        in_->ioc.get_executor(),
        bind_handler(
            std::move(init.completion_handler),
            error_code{},
            bytes_transferred));
    return init.result.get();
}

inline
void
teardown(
websocket::role_type,
stream& s,
boost::system::error_code& ec)
{
    if( s.in_->fc &&
        s.in_->fc->fail(ec))
        return;

    s.close();

    if( s.in_->fc &&
        s.in_->fc->fail(ec))
        ec = boost::asio::error::eof;
    else
        ec.assign(0, ec.category());
}

template<class TeardownHandler>
inline
void
async_teardown(
websocket::role_type,
stream& s,
TeardownHandler&& handler)
{
    error_code ec;
    if( s.in_->fc &&
        s.in_->fc->fail(ec))
        return boost::asio::post(
            s.get_executor(),
            bind_handler(std::move(handler), ec));
    s.close();
    if( s.in_->fc &&
        s.in_->fc->fail(ec))
        ec = boost::asio::error::eof;
    else
        ec.assign(0, ec.category());

    boost::asio::post(
        s.get_executor(),
        bind_handler(std::move(handler), ec));
}

//------------------------------------------------------------------------------

template<class Handler, class Buffers>
class stream::read_op : public stream::read_op_base
{
    class lambda
    {
        state& s_;
        Buffers b_;
        Handler h_;
        boost::asio::executor_work_guard<
            boost::asio::io_context::executor_type> work_;

    public:
        lambda(lambda&&) = default;
        lambda(lambda const&) = default;

        template<class DeducedHandler>
        lambda(state& s, Buffers const& b, DeducedHandler&& h)
            : s_(s)
            , b_(b)
            , h_(std::forward<DeducedHandler>(h))
            , work_(s_.ioc.get_executor())
        {
        }

        void
        post()
        {
            boost::asio::post(
                s_.ioc.get_executor(),
                std::move(*this));
            work_.reset();
        }

        void
        operator()()
        {
            using boost::asio::buffer_copy;
            using boost::asio::buffer_size;
            std::unique_lock<std::mutex> lock{s_.m};
            BOOST_ASSERT(! s_.op);
            if(s_.b.size() > 0)
            {
                auto const bytes_transferred = buffer_copy(
                    b_, s_.b.data(), s_.read_max);
                s_.b.consume(bytes_transferred);
                auto& s = s_;
                Handler h{std::move(h_)};
                lock.unlock();
                ++s.nread;
                boost::asio::post(
                    s.ioc.get_executor(),
                    bind_handler(
                        std::move(h),
                        error_code{},
                        bytes_transferred));
            }
            else
            {
                BOOST_ASSERT(s_.code != status::ok);
                auto& s = s_;
                Handler h{std::move(h_)};
                lock.unlock();
                ++s.nread;
                error_code ec;
                if(s.code == status::eof)
                    ec = boost::asio::error::eof;
                else if(s.code == status::reset)
                    ec = boost::asio::error::connection_reset;
                boost::asio::post(
                    s.ioc.get_executor(),
                    bind_handler(std::move(h), ec, 0));
            }
        }
    };

    lambda fn_;

public:
    template<class DeducedHandler>
    read_op(state& s, Buffers const& b, DeducedHandler&& h)
        : fn_(s, b, std::forward<DeducedHandler>(h))
    {
    }

    void
    operator()() override
    {
        fn_.post();
    }
};

inline
stream
connect(stream& to)
{
    stream from{to.get_executor().context()};
    from.connect(to);
    return from;
}

template<class Arg1, class... ArgN>
stream
connect(stream& to, Arg1&& arg1, ArgN&&... argn)
{
    stream from{
        std::forward<Arg1>(arg1),
        std::forward<ArgN>(argn)...};
    from.connect(to);
    return from;
}

} // test
} // beast
} // boost

#endif
