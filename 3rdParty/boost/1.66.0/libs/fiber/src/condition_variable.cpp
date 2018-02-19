
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "boost/fiber/condition_variable.hpp"

#include "boost/fiber/context.hpp"

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {

void
condition_variable_any::notify_one() noexcept {
    context * active_ctx = context::active();
    // get one context' from wait-queue
    detail::spinlock_lock lk{ wait_queue_splk_ };
    while ( ! wait_queue_.empty() ) {
        context * ctx = & wait_queue_.front();
        wait_queue_.pop_front();
        std::intptr_t expected = reinterpret_cast< std::intptr_t >( this);
        if ( ctx->twstatus.compare_exchange_strong( expected, static_cast< std::intptr_t >( -1), std::memory_order_acq_rel) ) {
            // notify before timeout
            intrusive_ptr_release( ctx);
            // notify context
            active_ctx->schedule( ctx);
            break;
        } else if ( static_cast< std::intptr_t >( 0) == expected) {
            // no timed-wait op.
            // notify context
            active_ctx->schedule( ctx);
            break;
        } else {
            // timed-wait op.
            // expected == -1: notify after timeout, same timed-wait op.
            // expected == <any>: notify after timeout, another timed-wait op. was already started
            intrusive_ptr_release( ctx);
            // re-schedule next
        }
    }
}

void
condition_variable_any::notify_all() noexcept {
    context * active_ctx = context::active();
    // get all context' from wait-queue
    detail::spinlock_lock lk{ wait_queue_splk_ };
    // notify all context'
    while ( ! wait_queue_.empty() ) {
        context * ctx = & wait_queue_.front();
        wait_queue_.pop_front();
        std::intptr_t expected = reinterpret_cast< std::intptr_t >( this);
        if ( ctx->twstatus.compare_exchange_strong( expected, static_cast< std::intptr_t >( -1), std::memory_order_acq_rel) ) {
            // notify before timeout
            intrusive_ptr_release( ctx);
            // notify context
            active_ctx->schedule( ctx);
        } else if ( static_cast< std::intptr_t >( 0) == expected) {
            // no timed-wait op.
            // notify context
            active_ctx->schedule( ctx);
        } else {
            // timed-wait op.
            // expected == -1: notify after timeout, same timed-wait op.
            // expected == <any>: notify after timeout, another timed-wait op. was already started
            intrusive_ptr_release( ctx);
            // re-schedule next
        }
    }
}

}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif
