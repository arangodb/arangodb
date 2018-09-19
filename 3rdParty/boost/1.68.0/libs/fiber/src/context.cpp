
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "boost/fiber/context.hpp"

#include <cstdlib>
#include <mutex>
#include <new>

#include "boost/fiber/exceptions.hpp"
#include "boost/fiber/scheduler.hpp"

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {

class main_context final : public context {
public:
    main_context() noexcept :
        context{ 1, type::main_context, launch::post } {
    }
};

class dispatcher_context final : public context {
private:
    boost::context::fiber
    run_( boost::context::fiber && c) {
#if (defined(BOOST_USE_UCONTEXT)||defined(BOOST_USE_WINFIB))
        std::move( c).resume();
#endif
		// execute scheduler::dispatch()
		return get_scheduler()->dispatch();
    }

public:
    dispatcher_context( boost::context::preallocated const& palloc, default_stack const& salloc) :
        context{ 0, type::dispatcher_context, launch::post } {
        c_ = boost::context::fiber{ std::allocator_arg, palloc, salloc,
                                    std::bind( & dispatcher_context::run_, this, std::placeholders::_1) };
#if (defined(BOOST_USE_UCONTEXT)||defined(BOOST_USE_WINFIB))
        c_ = std::move( c_).resume();
#endif
    }
};

static intrusive_ptr< context > make_dispatcher_context() {
    default_stack salloc; // use default satck-size
    auto sctx = salloc.allocate();
    // reserve space for control structure
    void * storage = reinterpret_cast< void * >(
            ( reinterpret_cast< uintptr_t >( sctx.sp) - static_cast< uintptr_t >( sizeof( dispatcher_context) ) )
            & ~ static_cast< uintptr_t >( 0xff) );
    void * stack_bottom = reinterpret_cast< void * >(
            reinterpret_cast< uintptr_t >( sctx.sp) - static_cast< uintptr_t >( sctx.size) );
    const std::size_t size = reinterpret_cast< uintptr_t >( storage) - reinterpret_cast< uintptr_t >( stack_bottom);
    // placement new of context on top of fiber's stack
    return intrusive_ptr< context >{
        new ( storage) dispatcher_context{
                boost::context::preallocated{ storage, size, sctx }, salloc } };
}

// schwarz counter
struct context_initializer {
    static thread_local context *   active_;
    static thread_local std::size_t counter_;

    context_initializer() {
        if ( 0 == counter_++) {
            // main fiber context of this thread
            context * main_ctx = new main_context{};
            // scheduler of this thread
            scheduler * sched = new scheduler{};
            // attach main context to scheduler
            sched->attach_main_context( main_ctx);
            // create and attach dispatcher context to scheduler
            sched->attach_dispatcher_context( make_dispatcher_context() );
            // make main context to active context
            active_ = main_ctx;
        }
    }

    ~context_initializer() {
        if ( 0 == --counter_) {
            context * main_ctx = active_;
            BOOST_ASSERT( main_ctx->is_context( type::main_context) );
            scheduler * sched = main_ctx->get_scheduler();
            delete sched;
            delete main_ctx;
        }
    }
};

// zero-initialization
thread_local context * context_initializer::active_{ nullptr };
thread_local std::size_t context_initializer::counter_{ 0 };

context *
context::active() noexcept {
    // initialized the first time control passes; per thread
    thread_local static context_initializer ctx_initializer;
    return context_initializer::active_;
}

void
context::reset_active() noexcept {
    context_initializer::active_ = nullptr;
}

context::~context() {
    // protect for concurrent access
    std::unique_lock< detail::spinlock > lk{ splk_ };
    BOOST_ASSERT( ! ready_is_linked() );
    BOOST_ASSERT( ! remote_ready_is_linked() );
    BOOST_ASSERT( ! sleep_is_linked() );
    BOOST_ASSERT( ! wait_is_linked() );
    if ( is_context( type::dispatcher_context) ) {
        // dispatcher-context is resumed by main-context
        // while the scheduler is deconstructed
#ifdef BOOST_DISABLE_ASSERTS
        wait_queue_.pop_front();
#else
        context * ctx = & wait_queue_.front();
        wait_queue_.pop_front();
        BOOST_ASSERT( ctx->is_context( type::main_context) );
        BOOST_ASSERT( nullptr == active() );
#endif
    }
    BOOST_ASSERT( wait_queue_.empty() );
    delete properties_;
}

context::id
context::get_id() const noexcept {
    return id{ const_cast< context * >( this) };
}

void
context::resume() noexcept {
    context * prev = this;
    // context_initializer::active_ will point to `this`
    // prev will point to previous active context
    std::swap( context_initializer::active_, prev);
    // pass pointer to the context that resumes `this`
    std::move( c_).resume_with([prev](boost::context::fiber && c){
                prev->c_ = std::move( c);
                return boost::context::fiber{};
            });
}

void
context::resume( detail::spinlock_lock & lk) noexcept {
    context * prev = this;
    // context_initializer::active_ will point to `this`
    // prev will point to previous active context
    std::swap( context_initializer::active_, prev);
    // pass pointer to the context that resumes `this`
    std::move( c_).resume_with([prev,&lk](boost::context::fiber && c){
                prev->c_ = std::move( c);
                lk.unlock();
                return boost::context::fiber{};
            });
}

void
context::resume( context * ready_ctx) noexcept {
    context * prev = this;
    // context_initializer::active_ will point to `this`
    // prev will point to previous active context
    std::swap( context_initializer::active_, prev);
    // pass pointer to the context that resumes `this`
    std::move( c_).resume_with([prev,ready_ctx](boost::context::fiber && c){
                prev->c_ = std::move( c);
                context::active()->schedule( ready_ctx);
                return boost::context::fiber{};
            });
}

void
context::suspend() noexcept {
    get_scheduler()->suspend();
}

void
context::suspend( detail::spinlock_lock & lk) noexcept {
    get_scheduler()->suspend( lk);
}

void
context::join() {
    // get active context
    context * active_ctx = context::active();
    // protect for concurrent access
    std::unique_lock< detail::spinlock > lk{ splk_ };
    // wait for context which is not terminated
    if ( ! terminated_) {
        // push active context to wait-queue, member
        // of the context which has to be joined by
        // the active context
        active_ctx->wait_link( wait_queue_);
        // suspend active context
        active_ctx->get_scheduler()->suspend( lk);
        // active context resumed
        BOOST_ASSERT( context::active() == active_ctx);
    }
}

void
context::yield() noexcept {
    // yield active context
    get_scheduler()->yield( context::active() );
}

boost::context::fiber
context::suspend_with_cc() noexcept {
    context * prev = this;
    // context_initializer::active_ will point to `this`
    // prev will point to previous active context
    std::swap( context_initializer::active_, prev);
    // pass pointer to the context that resumes `this`
    return std::move( c_).resume_with([prev](boost::context::fiber && c){
                prev->c_ = std::move( c);
                return boost::context::fiber{};
            });
}

boost::context::fiber
context::terminate() noexcept {
    // protect for concurrent access
    std::unique_lock< detail::spinlock > lk{ splk_ };
    // mark as terminated
    terminated_ = true;
    // notify all waiting fibers
    while ( ! wait_queue_.empty() ) {
        context * ctx = & wait_queue_.front();
        // remove fiber from wait-queue
        wait_queue_.pop_front();
        // notify scheduler
        schedule( ctx);
    }
    BOOST_ASSERT( wait_queue_.empty() );
    // release fiber-specific-data
    for ( fss_data_t::value_type & data : fss_data_) {
        data.second.do_cleanup();
    }
    fss_data_.clear();
    // switch to another context
    return get_scheduler()->terminate( lk, this);
}

bool
context::wait_until( std::chrono::steady_clock::time_point const& tp) noexcept {
    BOOST_ASSERT( nullptr != get_scheduler() );
    BOOST_ASSERT( this == active() );
    return get_scheduler()->wait_until( this, tp);
}

bool
context::wait_until( std::chrono::steady_clock::time_point const& tp,
                     detail::spinlock_lock & lk) noexcept {
    BOOST_ASSERT( nullptr != get_scheduler() );
    BOOST_ASSERT( this == active() );
    return get_scheduler()->wait_until( this, tp, lk);
}

void
context::schedule( context * ctx) noexcept {
    //BOOST_ASSERT( nullptr != ctx);
    BOOST_ASSERT( this != ctx);
    BOOST_ASSERT( nullptr != get_scheduler() );
    BOOST_ASSERT( nullptr != ctx->get_scheduler() );
#if ! defined(BOOST_FIBERS_NO_ATOMICS)
    // FIXME: comparing scheduler address' must be synchronized?
    //        what if ctx is migrated between threads
    //        (other scheduler assigned)
    if ( scheduler_ == ctx->get_scheduler() ) {
        // local
        get_scheduler()->schedule( ctx);
    } else {
        // remote
        ctx->get_scheduler()->schedule_from_remote( ctx);
    }
#else
    BOOST_ASSERT( get_scheduler() == ctx->get_scheduler() );
    get_scheduler()->schedule( ctx);
#endif
}

void *
context::get_fss_data( void const * vp) const {
    uintptr_t key = reinterpret_cast< uintptr_t >( vp);
    fss_data_t::const_iterator i = fss_data_.find( key);
    return fss_data_.end() != i ? i->second.vp : nullptr;
}

void
context::set_fss_data( void const * vp,
                       detail::fss_cleanup_function::ptr_t const& cleanup_fn,
                       void * data,
                       bool cleanup_existing) {
    BOOST_ASSERT( cleanup_fn);
    uintptr_t key = reinterpret_cast< uintptr_t >( vp);
    fss_data_t::iterator i = fss_data_.find( key);
    if ( fss_data_.end() != i) {
        if( cleanup_existing) {
            i->second.do_cleanup();
        }
        if ( nullptr != data) {
            fss_data_.insert(
                    i,
                    std::make_pair(
                        key,
                        fss_data{ data, cleanup_fn } ) );
        } else {
            fss_data_.erase( i);
        }
    } else {
        fss_data_.insert(
            std::make_pair(
                key,
                fss_data{ data, cleanup_fn } ) );
    }
}

void
context::set_properties( fiber_properties * props) noexcept {
    delete properties_;
    properties_ = props;
}

bool
context::worker_is_linked() const noexcept {
    return worker_hook_.is_linked();
}

bool
context::ready_is_linked() const noexcept {
    return ready_hook_.is_linked();
}

bool
context::remote_ready_is_linked() const noexcept {
    return remote_ready_hook_.is_linked();
}

bool
context::sleep_is_linked() const noexcept {
    return sleep_hook_.is_linked();
}

bool
context::terminated_is_linked() const noexcept {
    return terminated_hook_.is_linked();
}

bool
context::wait_is_linked() const noexcept {
    return wait_hook_.is_linked();
}

void
context::worker_unlink() noexcept {
    BOOST_ASSERT( worker_is_linked() );
    worker_hook_.unlink();
}

void
context::ready_unlink() noexcept {
    BOOST_ASSERT( ready_is_linked() );
    ready_hook_.unlink();
}

void
context::sleep_unlink() noexcept {
    BOOST_ASSERT( sleep_is_linked() );
    sleep_hook_.unlink();
}

void
context::wait_unlink() noexcept {
    BOOST_ASSERT( wait_is_linked() );
    wait_hook_.unlink();
}

void
context::detach() noexcept {
    BOOST_ASSERT( context::active() != this);
    get_scheduler()->detach_worker_context( this);
}

void
context::attach( context * ctx) noexcept {
    BOOST_ASSERT( nullptr != ctx);
    get_scheduler()->attach_worker_context( ctx);
}

}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif
