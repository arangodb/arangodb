
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "boost/fiber/scheduler.hpp"

#include <chrono>
#include <mutex>

#include <boost/assert.hpp>

#include "boost/fiber/algo/round_robin.hpp"
#include "boost/fiber/context.hpp"
#include "boost/fiber/exceptions.hpp"

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {

void
scheduler::release_terminated_() noexcept {
    while ( ! terminated_queue_.empty() ) {
        context * ctx = & terminated_queue_.front();
        terminated_queue_.pop_front();
        BOOST_ASSERT( ctx->is_context( type::worker_context) );
        BOOST_ASSERT( ! ctx->is_context( type::pinned_context) );
        BOOST_ASSERT( this == ctx->get_scheduler() );
        BOOST_ASSERT( ctx->is_resumable() );
        BOOST_ASSERT( ! ctx->worker_is_linked() );
        BOOST_ASSERT( ! ctx->ready_is_linked() );
#if ! defined(BOOST_FIBERS_NO_ATOMICS)
        BOOST_ASSERT( ! ctx->remote_ready_is_linked() );
#endif
        BOOST_ASSERT( ! ctx->sleep_is_linked() );
        BOOST_ASSERT( ctx->wait_queue_.empty() );
        BOOST_ASSERT( ctx->terminated_);
        // if last reference, e.g. fiber::join() or fiber::detach()
        // have been already called, this will call ~context(),
        // the context is automatically removeid from worker-queue
        intrusive_ptr_release( ctx);
    }
}

#if ! defined(BOOST_FIBERS_NO_ATOMICS)
void
scheduler::remote_ready2ready_() noexcept {
    remote_ready_queue_type tmp;
    detail::spinlock_lock lk{ remote_ready_splk_ };
    remote_ready_queue_.swap( tmp);
    lk.unlock();
    // get context from remote ready-queue
    while ( ! tmp.empty() ) {
        context * ctx = & tmp.front();
        tmp.pop_front();
        // store context in local queues
        schedule( ctx);
    }
}
#endif

void
scheduler::sleep2ready_() noexcept {
    // move context which the deadline has reached
    // to ready-queue
    // sleep-queue is sorted (ascending)
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    sleep_queue_type::iterator e = sleep_queue_.end();
    for ( sleep_queue_type::iterator i = sleep_queue_.begin(); i != e;) {
        context * ctx = & ( * i);
        // dispatcher context must never be pushed to sleep-queue
        BOOST_ASSERT( ! ctx->is_context( type::dispatcher_context) );
        BOOST_ASSERT( main_ctx_ == ctx || ctx->worker_is_linked() );
        BOOST_ASSERT( ! ctx->ready_is_linked() );
        // remote_ready_hook_ can be linked in that point in case when the ctx
        // has been signaled concurrently when sleep2ready_ is called. In that
        // case sleep_waker_.wake() is just no-op, because sleep_waker_ is
        // outdated
        BOOST_ASSERT( ! ctx->terminated_is_linked() );
        // set fiber to state_ready if deadline was reached
        if ( ctx->tp_ <= now) {
            // remove context from sleep-queue
            i = sleep_queue_.erase( i);
            // reset sleep-tp
            ctx->tp_ = (std::chrono::steady_clock::time_point::max)();
            ctx->sleep_waker_.wake();
        } else {
            break; // first context with now < deadline
        }
    }
}

scheduler::scheduler() noexcept :
    algo_{ new algo::round_robin() } {
}

scheduler::~scheduler() {
    BOOST_ASSERT( nullptr != main_ctx_);
    BOOST_ASSERT( nullptr != dispatcher_ctx_.get() );
    BOOST_ASSERT( context::active() == main_ctx_);
    // signal dispatcher-context termination
    shutdown_ = true;
    // resume pending fibers
    // by resuming dispatcher-context
    context::active()->suspend();
    // no context' in worker-queue
    BOOST_ASSERT( worker_queue_.empty() );
    BOOST_ASSERT( terminated_queue_.empty() );
    BOOST_ASSERT( sleep_queue_.empty() );
    // set active context to nullptr
    context::reset_active();
    // deallocate dispatcher-context
    BOOST_ASSERT( ! dispatcher_ctx_->ready_is_linked() );
    dispatcher_ctx_.reset();
    // set main-context to nullptr
    main_ctx_ = nullptr;
}

boost::context::fiber
scheduler::dispatch() noexcept {
    BOOST_ASSERT( context::active() == dispatcher_ctx_);
    for (;;) {
        if ( shutdown_) {
            // notify sched-algorithm about termination
            algo_->notify();
            if ( worker_queue_.empty() ) {
                break;
            }
        }
        // release terminated context'
        release_terminated_();
#if ! defined(BOOST_FIBERS_NO_ATOMICS)
        // get context' from remote ready-queue
        remote_ready2ready_();
#endif
        // get sleeping context'
        // must be called after remote_ready2ready_()
        sleep2ready_();
        // get next ready context
        context * ctx = algo_->pick_next();
        if ( nullptr != ctx) {
            BOOST_ASSERT( ctx->is_resumable() );
            BOOST_ASSERT( ! ctx->ready_is_linked() );
#if ! defined(BOOST_FIBERS_NO_ATOMICS)
            BOOST_ASSERT( ! ctx->remote_ready_is_linked() );
#endif
            BOOST_ASSERT( ! ctx->sleep_is_linked() );
            BOOST_ASSERT( ! ctx->terminated_is_linked() );
            // push dispatcher-context to ready-queue
            // so that ready-queue never becomes empty
            ctx->resume( dispatcher_ctx_.get() );
            BOOST_ASSERT( context::active() == dispatcher_ctx_.get() );
        } else {
            // no ready context, wait till signaled
            // set deadline to highest value
            std::chrono::steady_clock::time_point suspend_time =
                    (std::chrono::steady_clock::time_point::max)();
            // get lowest deadline from sleep-queue
            sleep_queue_type::iterator i = sleep_queue_.begin();
            if ( sleep_queue_.end() != i) {
                suspend_time = i->tp_;
            }
            // no ready context, wait till signaled
            algo_->suspend_until( suspend_time);
        }
    }
    // release termianted context'
    release_terminated_();
    // return to main-context
    return main_ctx_->suspend_with_cc();
}

void
scheduler::schedule( context * ctx) noexcept {
    BOOST_ASSERT( nullptr != ctx);
    BOOST_ASSERT( ! ctx->ready_is_linked() );
#if ! defined(BOOST_FIBERS_NO_ATOMICS)
    BOOST_ASSERT( ! ctx->remote_ready_is_linked() );
#endif
    BOOST_ASSERT( ! ctx->terminated_is_linked() );
    // remove context ctx from sleep-queue
    // (might happen if blocked in timed_mutex::try_lock_until())
    if ( ctx->sleep_is_linked() ) {
        // unlink it from sleep-queue
        ctx->sleep_unlink();
    }
    // push new context to ready-queue
    algo_->awakened( ctx);
}

#if ! defined(BOOST_FIBERS_NO_ATOMICS)
void
scheduler::schedule_from_remote( context * ctx) noexcept {
    BOOST_ASSERT( nullptr != ctx);
    // another thread might signal the main-context of this thread
    BOOST_ASSERT( ! ctx->is_context( type::dispatcher_context) );
    BOOST_ASSERT( this == ctx->get_scheduler() );
    BOOST_ASSERT( ! ctx->ready_is_linked() );
    BOOST_ASSERT( ! ctx->remote_ready_is_linked() );
    BOOST_ASSERT( ! ctx->terminated_is_linked() );
    // protect for concurrent access
    detail::spinlock_lock lk{ remote_ready_splk_ };
    BOOST_ASSERT( ! shutdown_);
    BOOST_ASSERT( nullptr != main_ctx_);
    BOOST_ASSERT( nullptr != dispatcher_ctx_.get() );
    // push new context to remote ready-queue
    ctx->remote_ready_link( remote_ready_queue_);
    lk.unlock();
    // notify scheduler
    algo_->notify();
}
#endif

boost::context::fiber
scheduler::terminate( detail::spinlock_lock & lk, context * ctx) noexcept {
    BOOST_ASSERT( nullptr != ctx);
    BOOST_ASSERT( context::active() == ctx);
    BOOST_ASSERT( this == ctx->get_scheduler() );
    BOOST_ASSERT( ctx->is_context( type::worker_context) );
    BOOST_ASSERT( ! ctx->is_context( type::pinned_context) );
    BOOST_ASSERT( ! ctx->ready_is_linked() );
#if ! defined(BOOST_FIBERS_NO_ATOMICS)
    BOOST_ASSERT( ! ctx->remote_ready_is_linked() );
#endif
    BOOST_ASSERT( ! ctx->sleep_is_linked() );
    BOOST_ASSERT( ! ctx->terminated_is_linked() );
    BOOST_ASSERT( ctx->wait_queue_.empty() );
    // store the terminated fiber in the terminated-queue
    // the dispatcher-context will call
    ctx->terminated_link( terminated_queue_);
    // remove from the worker-queue
    ctx->worker_unlink();
    // release lock
    lk.unlock();
    // resume another fiber
    return algo_->pick_next()->suspend_with_cc();
}

void
scheduler::yield( context * ctx) noexcept {
    BOOST_ASSERT( nullptr != ctx);
    BOOST_ASSERT( context::active() == ctx);
    BOOST_ASSERT( ctx->is_context( type::worker_context) || ctx->is_context( type::main_context) );
    BOOST_ASSERT( ! ctx->ready_is_linked() );
#if ! defined(BOOST_FIBERS_NO_ATOMICS)
    BOOST_ASSERT( ! ctx->remote_ready_is_linked() );
#endif
    BOOST_ASSERT( ! ctx->sleep_is_linked() );
    BOOST_ASSERT( ! ctx->terminated_is_linked() );
    // resume another fiber
    algo_->pick_next()->resume( ctx);
}

bool
scheduler::wait_until( context * ctx,
                       std::chrono::steady_clock::time_point const& sleep_tp) noexcept {
    BOOST_ASSERT( nullptr != ctx);
    BOOST_ASSERT( context::active() == ctx);
    BOOST_ASSERT( ctx->is_context( type::worker_context) || ctx->is_context( type::main_context) );
    BOOST_ASSERT( ! ctx->ready_is_linked() );
#if ! defined(BOOST_FIBERS_NO_ATOMICS)
    BOOST_ASSERT( ! ctx->remote_ready_is_linked() );
#endif
    BOOST_ASSERT( ! ctx->sleep_is_linked() );
    BOOST_ASSERT( ! ctx->terminated_is_linked() );
    ctx->sleep_waker_ = ctx->create_waker();
    ctx->tp_ = sleep_tp;
    ctx->sleep_link( sleep_queue_);
    // resume another context
    algo_->pick_next()->resume();
    // context has been resumed
    // check if deadline has reached
    return std::chrono::steady_clock::now() < sleep_tp;
}

bool
scheduler::wait_until( context * ctx,
                       std::chrono::steady_clock::time_point const& sleep_tp,
                       detail::spinlock_lock & lk,
                       waker && w) noexcept {
    BOOST_ASSERT( nullptr != ctx);
    BOOST_ASSERT( context::active() == ctx);
    BOOST_ASSERT( ctx->is_context( type::worker_context) || ctx->is_context( type::main_context) );
    BOOST_ASSERT( ! ctx->ready_is_linked() );
#if ! defined(BOOST_FIBERS_NO_ATOMICS)
    BOOST_ASSERT( ! ctx->remote_ready_is_linked() );
#endif
    BOOST_ASSERT( ! ctx->sleep_is_linked() );
    BOOST_ASSERT( ! ctx->terminated_is_linked() );
    // push active context to sleep-queue
    ctx->sleep_waker_ = std::move( w);
    ctx->tp_ = sleep_tp;
    ctx->sleep_link( sleep_queue_);
    // resume another context
    algo_->pick_next()->resume( lk);
    // context has been resumed
    // check if deadline has reached
    return std::chrono::steady_clock::now() < sleep_tp;
}

void
scheduler::suspend() noexcept {
    // resume another context
    algo_->pick_next()->resume();
}

void
scheduler::suspend( detail::spinlock_lock & lk) noexcept {
    // resume another context
    algo_->pick_next()->resume( lk);
}

bool
scheduler::has_ready_fibers() const noexcept {
    return algo_->has_ready_fibers();
}

void
scheduler::set_algo( algo::algorithm::ptr_t algo) noexcept {
    // move remaining context in current scheduler to new one
    while ( algo_->has_ready_fibers() ) {
        algo->awakened( algo_->pick_next() );
    }
    algo_ = std::move( algo);
}

void
scheduler::attach_main_context( context * ctx) noexcept {
    BOOST_ASSERT( nullptr != ctx);
    // main-context represents the execution context created
    // by the system, e.g. main()- or thread-context
    // should not be in worker-queue
    main_ctx_ = ctx;
    main_ctx_->scheduler_ = this;
}

void
scheduler::attach_dispatcher_context( intrusive_ptr< context > ctx) noexcept {
    BOOST_ASSERT( ctx);
    // dispatcher context has to handle
    //    - remote ready context'
    //    - sleeping context'
    //    - extern event-loops
    //    - suspending the thread if ready-queue is empty (waiting on external event)
    // should not be in worker-queue
    dispatcher_ctx_.swap( ctx);
    // add dispatcher-context to ready-queue
    // so it is the first element in the ready-queue
    // if the main context tries to suspend the first time
    // the dispatcher-context is resumed and
    // scheduler::dispatch() is executed
    dispatcher_ctx_->scheduler_ = this;
    algo_->awakened( dispatcher_ctx_.get() );
}

void
scheduler::attach_worker_context( context * ctx) noexcept {
    BOOST_ASSERT( nullptr != ctx);
    BOOST_ASSERT( nullptr == ctx->get_scheduler() );
    BOOST_ASSERT( ! ctx->ready_is_linked() );
#if ! defined(BOOST_FIBERS_NO_ATOMICS)
    BOOST_ASSERT( ! ctx->remote_ready_is_linked() );
#endif
    BOOST_ASSERT( ! ctx->sleep_is_linked() );
    BOOST_ASSERT( ! ctx->terminated_is_linked() );
    BOOST_ASSERT( ! ctx->worker_is_linked() );
    ctx->worker_link( worker_queue_);
    ctx->scheduler_ = this;
    // an attached context must belong at least to worker-queue
}

void
scheduler::detach_worker_context( context * ctx) noexcept {
    BOOST_ASSERT( nullptr != ctx);
    BOOST_ASSERT( ! ctx->ready_is_linked() );
#if ! defined(BOOST_FIBERS_NO_ATOMICS)
    BOOST_ASSERT( ! ctx->remote_ready_is_linked() );
#endif
    BOOST_ASSERT( ! ctx->sleep_is_linked() );
    BOOST_ASSERT( ! ctx->terminated_is_linked() );
    BOOST_ASSERT( ctx->worker_is_linked() );
    BOOST_ASSERT( ! ctx->is_context( type::pinned_context) );
    ctx->worker_unlink();
    BOOST_ASSERT( ! ctx->worker_is_linked() );
    ctx->scheduler_ = nullptr;
    // a detached context must not belong to any queue
}

}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif
