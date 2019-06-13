//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_IMPL_TIMOUT_SOCKET_HPP
#define BOOST_BEAST_CORE_IMPL_TIMOUT_SOCKET_HPP

#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/experimental/core/detail/timeout_work_guard.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <memory>
#include <utility>

namespace boost {
namespace beast {

template<class Protocol, class Executor>
template<class Handler>
class basic_timeout_socket<Protocol, Executor>::async_op
{
    Handler h_;
    basic_timeout_socket& s_;
    detail::timeout_work_guard work_;

public:
    async_op(async_op&&) = default;
    async_op(async_op const&) = delete;

    template<class Buffers, class DeducedHandler>
    async_op(
        Buffers const& b,
        DeducedHandler&& h,
        basic_timeout_socket& s,
        std::true_type)
        : h_(std::forward<DeducedHandler>(h))
        , s_(s)
        , work_(s.rd_timer_)
    {
        s_.sock_.async_read_some(b, std::move(*this));
    }

    template<class Buffers, class DeducedHandler>
    async_op(
        Buffers const& b,
        DeducedHandler&& h,
        basic_timeout_socket& s,
        std::false_type)
        : h_(std::forward<DeducedHandler>(h))
        , s_(s)
        , work_(s.wr_timer_)
    {
        s_.sock_.async_write_some(b, std::move(*this));
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
            std::declval<basic_timeout_socket<Protocol>&>().get_executor())>;

    executor_type
    get_executor() const noexcept
    {
        return (boost::asio::get_associated_executor)(
            h_, s_.get_executor());
    }

    friend
    bool asio_handler_is_continuation(async_op* op)
    {
        using boost::asio::asio_handler_is_continuation;
        return asio_handler_is_continuation(
            std::addressof(op->h_));
    }

    template<class Function>
    friend
    void asio_handler_invoke(Function&& f, async_op* op)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f, std::addressof(op->h_));
    }

    void
    operator()(error_code ec, std::size_t bytes_transferred)
    {
        if(s_.expired_)
        {
            BOOST_ASSERT(ec == boost::asio::error::operation_aborted);
            ec = boost::asio::error::timed_out;
        }
        else
        {
            work_.complete();
        }
        h_(ec, bytes_transferred);
    }
};

//------------------------------------------------------------------------------

template<class Protocol, class Executor>
basic_timeout_socket<Protocol, Executor>::
timer::
timer(basic_timeout_socket& s)
    : detail::timeout_object(s.ex_.context())
    , s_(s)
{
}

template<class Protocol, class Executor>
auto
basic_timeout_socket<Protocol, Executor>::
timer::
operator=(timer&&)
    -> timer&
{
    return *this;
}

template<class Protocol, class Executor>
void
basic_timeout_socket<Protocol, Executor>::
timer::
on_timeout()
{
    boost::asio::post(
        s_.ex_,
        [this]()
        {
            s_.expired_ = true;
            s_.sock_.cancel();
        });
}

//------------------------------------------------------------------------------

template<class Protocol, class Executor>
template<class ExecutionContext, class>
basic_timeout_socket<Protocol, Executor>::
basic_timeout_socket(ExecutionContext& ctx)
    : ex_(ctx.get_executor())
    , rd_timer_(*this)
    , wr_timer_(*this)
    , sock_(ctx)
{
}

template<class Protocol, class Executor>
template<class MutableBufferSequence, class ReadHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(ReadHandler,
    void(boost::system::error_code, std::size_t))
basic_timeout_socket<Protocol, Executor>::
async_read_some(
    MutableBufferSequence const& buffers,
    ReadHandler&& handler)
{
    static_assert(boost::asio::is_mutable_buffer_sequence<
        MutableBufferSequence>::value,
            "MutableBufferSequence requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        ReadHandler, void(error_code, std::size_t));
    async_op<BOOST_ASIO_HANDLER_TYPE(ReadHandler,
        void(error_code, std::size_t))>(buffers,
            std::forward<ReadHandler>(handler), *this,
                std::true_type{});
    return init.result.get();
}

template<class Protocol, class Executor>
template<class ConstBufferSequence, class WriteHandler>
BOOST_ASIO_INITFN_RESULT_TYPE(WriteHandler,
    void(boost::system::error_code, std::size_t))
basic_timeout_socket<Protocol, Executor>::
async_write_some(
    ConstBufferSequence const& buffers,
    WriteHandler&& handler)
{
    static_assert(boost::asio::is_const_buffer_sequence<
        ConstBufferSequence>::value,
            "ConstBufferSequence requirements not met");
    BOOST_BEAST_HANDLER_INIT(
        WriteHandler, void(error_code, std::size_t));
    async_op<BOOST_ASIO_HANDLER_TYPE(WriteHandler,
        void(error_code, std::size_t))>(buffers,
            std::forward<WriteHandler>(handler), *this,
                std::false_type{});
    return init.result.get();
}

} // beast
} // boost

#endif
