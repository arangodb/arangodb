
#include "boost/fiber/waker.hpp"
#include "boost/fiber/context.hpp"

namespace boost {
namespace fibers {

bool
waker::wake() const noexcept {
    BOOST_ASSERT(epoch_ > 0);
    BOOST_ASSERT(ctx_ != nullptr);

    return ctx_->wake(epoch_);
}

void
wait_queue::suspend_and_wait( detail::spinlock_lock & lk, context * active_ctx) {
    waker_with_hook w{ active_ctx->create_waker() };
    slist_.push_back(w);
    // suspend this fiber
    active_ctx->suspend( lk);
    BOOST_ASSERT( ! w.is_linked() );
}

bool
wait_queue::suspend_and_wait_until( detail::spinlock_lock & lk,
                                context * active_ctx,
                                std::chrono::steady_clock::time_point const& timeout_time) {
    waker_with_hook w{ active_ctx->create_waker() };
    slist_.push_back(w);
    // suspend this fiber
    if ( ! active_ctx->wait_until( timeout_time, lk, waker(w)) ) {
        // relock local lk
        lk.lock();
        // remove from waiting-queue
        if ( w.is_linked()) {
            slist_.remove( w);
        }
        lk.unlock();
        return false;
    }
    return true;
}

void
wait_queue::notify_one() {
    while ( ! slist_.empty() ) {
        waker & w = slist_.front();
        slist_.pop_front();
        if ( w.wake()) {
            break;
        }
    }
}

void
wait_queue::notify_all() {
    while ( ! slist_.empty() ) {
        waker & w = slist_.front();
        slist_.pop_front();
        w.wake();
    }
}

bool
wait_queue::empty() const {
    return slist_.empty();
}

}
}
