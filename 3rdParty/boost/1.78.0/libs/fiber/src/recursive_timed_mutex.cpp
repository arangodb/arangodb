
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "boost/fiber/recursive_timed_mutex.hpp"

#include <algorithm>
#include <functional>

#include "boost/fiber/exceptions.hpp"
#include "boost/fiber/scheduler.hpp"

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {

bool
recursive_timed_mutex::try_lock_until_( std::chrono::steady_clock::time_point const& timeout_time) noexcept {
    while ( true) {
        if ( std::chrono::steady_clock::now() > timeout_time) {
            return false;
        }
        context * active_ctx = context::active();
        // store this fiber in order to be notified later
        detail::spinlock_lock lk{ wait_queue_splk_ };
        if ( active_ctx == owner_) {
            ++count_;
            return true;
        }
        if ( nullptr == owner_) {
            owner_ = active_ctx;
            count_ = 1;
            return true;
        }
        if ( ! wait_queue_.suspend_and_wait_until( lk, active_ctx, timeout_time)) {
            return false;
        }
    }
}

void
recursive_timed_mutex::lock() {
    while ( true) {
        context * active_ctx = context::active();
        // store this fiber in order to be notified later
        detail::spinlock_lock lk{ wait_queue_splk_ };
        if ( active_ctx == owner_) {
            ++count_;
            return;
        }
        if ( nullptr == owner_) {
            owner_ = active_ctx;
            count_ = 1;
            return;
        }
        wait_queue_.suspend_and_wait( lk, active_ctx);
    }
}

bool
recursive_timed_mutex::try_lock() noexcept {
    context * active_ctx = context::active();
    detail::spinlock_lock lk{ wait_queue_splk_ };
    if ( nullptr == owner_) {
        owner_ = active_ctx;
        count_ = 1;
    } else if ( active_ctx == owner_) {
        ++count_;
    }
    lk.unlock();
    // let other fiber release the lock
    active_ctx->yield();
    return active_ctx == owner_;
}

void
recursive_timed_mutex::unlock() {
    context * active_ctx = context::active();
    detail::spinlock_lock lk{ wait_queue_splk_ };
    if ( BOOST_UNLIKELY( active_ctx != owner_) ) {
        throw lock_error{
                std::make_error_code( std::errc::operation_not_permitted),
                "boost fiber: no  privilege to perform the operation" };
    }
    if ( 0 == --count_) {
        owner_ = nullptr;
        wait_queue_.notify_one();
    }
}

}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif
