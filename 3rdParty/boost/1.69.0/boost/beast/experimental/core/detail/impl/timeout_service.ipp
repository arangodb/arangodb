//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_IMPL_TIMEOUT_SERVICE_IPP
#define BOOST_BEAST_CORE_DETAIL_IMPL_TIMEOUT_SERVICE_IPP

namespace boost {
namespace beast {
namespace detail {

//------------------------------------------------------------------------------

inline
timeout_object::
timeout_object(boost::asio::io_context& ioc)
    : svc_(boost::asio::use_service<timeout_service>(ioc))
{
}

//------------------------------------------------------------------------------

inline
timeout_service::
timeout_service(boost::asio::io_context& ctx)
    : service_base(ctx)
    , strand_(ctx.get_executor())
    , timer_(ctx)
{
}

inline
void
timeout_service::
on_work_started(timeout_object& obj)
{
    std::lock_guard<std::mutex> lock(m_);
    BOOST_VERIFY(++obj.outstanding_work_ == 1);
    insert(obj, *fresh_);
    if(++count_ == 1)
        do_async_wait();
}

inline
void
timeout_service::
on_work_complete(timeout_object& obj)
{
    std::lock_guard<std::mutex> lock(m_);
    remove(obj);
}

inline
void
timeout_service::
on_work_stopped(timeout_object& obj)
{
    std::lock_guard<std::mutex> lock(m_);
    BOOST_ASSERT(count_ > 0);
    BOOST_VERIFY(--obj.outstanding_work_ == 0);
    if(obj.list_ != nullptr)
        remove(obj);
    if(--count_ == 0)
        timer_.cancel();
}

inline
void
timeout_service::
set_option(std::chrono::seconds n)
{
    interval_ = n;
}

//------------------------------------------------------------------------------

// Precondition: caller holds the mutex
inline
void
timeout_service::
insert(timeout_object& obj, list_type& list)
{
    BOOST_ASSERT(obj.list_ == nullptr);
    list.push_back(&obj); // can throw
    obj.list_ = &list;
    obj.pos_ = list.size();
}

// Precondition: caller holds the mutex
inline
void
timeout_service::
remove(timeout_object& obj)
{
    BOOST_ASSERT(obj.list_ != nullptr);
    BOOST_ASSERT(
        obj.list_ == stale_ ||
        obj.list_ == fresh_);
    BOOST_ASSERT(obj.list_->size() > 0);
    auto& list = *obj.list_;
    auto const n = list.size() - 1;
    if(obj.pos_ != n)
    {
        auto other = list[n];
        list[obj.pos_] = other;
        other->pos_ = obj.pos_;
    }
    obj.list_ = nullptr;
    list.resize(n);
}

inline
void
timeout_service::
do_async_wait()
{
    timer_.expires_after(interval_);
    timer_.async_wait(
        boost::asio::bind_executor(
            strand_,
            [this](error_code ec)
            {
                this->on_timer(ec);
            }));
}

inline
void
timeout_service::
on_timer(error_code ec)
{
    if(ec == boost::asio::error::operation_aborted)
    {
        BOOST_ASSERT(fresh_->empty());
        BOOST_ASSERT(stale_->empty());
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_);
        if(! stale_->empty())
        {
            for(auto obj : *stale_)
            {
                obj->list_ = nullptr;
                obj->on_timeout();
            }
            stale_->clear();
        }
        std::swap(fresh_, stale_);
    }

    do_async_wait();
}

//------------------------------------------------------------------------------

inline
void
timeout_service::
shutdown() noexcept
{
    boost::asio::post(
        boost::asio::bind_executor(
            strand_,
            [this]()
            {
                timer_.cancel();
            }));
}

} // detail
} // beast
} // boost

#endif
