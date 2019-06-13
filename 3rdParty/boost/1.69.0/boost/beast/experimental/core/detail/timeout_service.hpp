//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_TIMEOUT_SERVICE_HPP
#define BOOST_BEAST_CORE_DETAIL_TIMEOUT_SERVICE_HPP

#include <boost/beast/core/error.hpp>
#include <boost/beast/experimental/core/detail/service_base.hpp>
#include <boost/assert.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <utility>
#include <vector>

namespace boost {
namespace beast {
namespace detail {

//------------------------------------------------------------------------------

class timeout_service;

class timeout_object
{
    friend class timeout_service;

    using list_type = std::vector<timeout_object*>;

    timeout_service& svc_;
    std::size_t pos_;
    list_type* list_ = nullptr;
    char outstanding_work_ = 0;

public:
    timeout_object() = delete;
    timeout_object(timeout_object&&) = delete;
    timeout_object(timeout_object const&) = delete;
    timeout_object& operator=(timeout_object&&) = delete;
    timeout_object& operator=(timeout_object const&) = delete;

    // VFALCO should be execution_context
    explicit
    timeout_object(boost::asio::io_context& ioc);

    timeout_service&
    service() const
    {
        return svc_;
    }

    virtual void on_timeout() = 0;
};

//------------------------------------------------------------------------------

class timeout_service
    : public service_base<timeout_service>
{
public:
    using key_type = timeout_service;

    // VFALCO Should be execution_context
    explicit
    timeout_service(boost::asio::io_context& ctx);

    void
    on_work_started(timeout_object& obj);

    void
    on_work_complete(timeout_object& obj);

    void
    on_work_stopped(timeout_object& obj);

    void
    set_option(std::chrono::seconds n);

private:
    friend class timeout_object;

    using list_type = std::vector<timeout_object*>;

    void insert(timeout_object& obj, list_type& list);
    void remove(timeout_object& obj);
    void do_async_wait();
    void on_timer(error_code ec);

    virtual void shutdown() noexcept override;

    boost::asio::strand<
        boost::asio::io_context::executor_type> strand_;

    std::mutex m_;
    list_type list_[2];
    list_type* fresh_ = &list_[0];
    list_type* stale_ = &list_[1];
    std::size_t count_ = 0;
    boost::asio::steady_timer timer_;
    std::chrono::seconds interval_{30ul};
};

//------------------------------------------------------------------------------

} // detail
} // beast
} // boost

#include <boost/beast/experimental/core/detail/impl/timeout_service.ipp>

#endif
