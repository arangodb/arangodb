//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_TIMEOUT_WORK_GUARD_HPP
#define BOOST_BEAST_CORE_DETAIL_TIMEOUT_WORK_GUARD_HPP

#include <boost/beast/experimental/core/detail/timeout_service.hpp>
#include <boost/assert.hpp>
#include <boost/core/exchange.hpp>

namespace boost {
namespace beast {
namespace detail {

class timeout_work_guard
{
    timeout_object* obj_;

public:
    timeout_work_guard(timeout_work_guard const&) = delete;
    timeout_work_guard& operator=(timeout_work_guard&&) = delete;
    timeout_work_guard& operator=(timeout_work_guard const&) = delete;

    ~timeout_work_guard()
    {
        reset();
    }

    timeout_work_guard(timeout_work_guard&& other)
        : obj_(boost::exchange(other.obj_, nullptr))
    {
    }

    explicit
    timeout_work_guard(timeout_object& obj)
        : obj_(&obj)
    {
        obj_->service().on_work_started(*obj_);
    }

    bool
    owns_work() const
    {
        return obj_ != nullptr;
    }

    void
    reset()
    {
        if(obj_)
            obj_->service().on_work_stopped(*obj_);
    }

    void
    complete()
    {
        BOOST_ASSERT(obj_ != nullptr);
        obj_->service().on_work_complete(*obj_);
        obj_ = nullptr;
    }
};

} // detail
} // beast
} // boost

#endif
