
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_FIBERS_CONTEXT_H
#define BOOST_FIBERS_CONTEXT_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <tuple>
#include <type_traits>

#include <boost/assert.hpp>
#include <boost/config.hpp>
#if defined(BOOST_NO_CXX17_STD_APPLY)
#include <boost/context/detail/apply.hpp>
#endif
#include <boost/context/fiber.hpp>
#include <boost/context/stack_context.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/parent_from_member.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/intrusive/slist.hpp>

#include <boost/fiber/detail/config.hpp>
#include <boost/fiber/detail/data.hpp>
#include <boost/fiber/detail/decay_copy.hpp>
#include <boost/fiber/detail/fss.hpp>
#include <boost/fiber/detail/spinlock.hpp>
#include <boost/fiber/exceptions.hpp>
#include <boost/fiber/fixedsize_stack.hpp>
#include <boost/fiber/policy.hpp>
#include <boost/fiber/properties.hpp>
#include <boost/fiber/segmented_stack.hpp>
#include <boost/fiber/type.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable:4251)
#endif

namespace boost {
namespace fibers {

class context;
class fiber;
class scheduler;

namespace detail {

struct wait_tag;
typedef intrusive::list_member_hook<
    intrusive::tag< wait_tag >,
    intrusive::link_mode<
        intrusive::auto_unlink
    >
>                                 wait_hook;
// declaration of the functor that converts between
// the context class and the wait-hook
struct wait_functor {
    // required types
    typedef wait_hook               hook_type;
    typedef hook_type           *   hook_ptr;
    typedef const hook_type     *   const_hook_ptr;
    typedef context                 value_type;
    typedef value_type          *   pointer;
    typedef const value_type    *   const_pointer;

    // required static functions
    static hook_ptr to_hook_ptr( value_type &value);
    static const_hook_ptr to_hook_ptr( value_type const& value);
    static pointer to_value_ptr( hook_ptr n);
    static const_pointer to_value_ptr( const_hook_ptr n);
};

struct ready_tag;
typedef intrusive::list_member_hook<
    intrusive::tag< ready_tag >,
    intrusive::link_mode<
        intrusive::auto_unlink
    >
>                                       ready_hook;

struct sleep_tag;
typedef intrusive::set_member_hook<
    intrusive::tag< sleep_tag >,
    intrusive::link_mode<
        intrusive::auto_unlink
    >
>                                       sleep_hook;

struct worker_tag;
typedef intrusive::list_member_hook<
    intrusive::tag< worker_tag >,
    intrusive::link_mode<
        intrusive::auto_unlink
    >
>                                       worker_hook;

struct terminated_tag;
typedef intrusive::slist_member_hook<
    intrusive::tag< terminated_tag >,
    intrusive::link_mode<
        intrusive::safe_link
    >
>                                       terminated_hook;

struct remote_ready_tag;
typedef intrusive::slist_member_hook<
    intrusive::tag< remote_ready_tag >,
    intrusive::link_mode<
        intrusive::safe_link
    >
>                                       remote_ready_hook;

}

class BOOST_FIBERS_DECL context {
public:
    typedef intrusive::list<
                context,
                intrusive::function_hook< detail::wait_functor >,
                intrusive::constant_time_size< false >
            >   wait_queue_t;

private:
    friend class dispatcher_context;
    friend class main_context;
    template< typename Fn, typename ... Arg > friend class worker_context;
    friend class scheduler;

    struct fss_data {
        void                                *   vp{ nullptr };
        detail::fss_cleanup_function::ptr_t     cleanup_function{};

        fss_data() noexcept {
        }

        fss_data( void * vp_,
                  detail::fss_cleanup_function::ptr_t const& fn) noexcept :
            vp( vp_),
            cleanup_function( fn) {
            BOOST_ASSERT( cleanup_function);
        }

        void do_cleanup() {
            ( * cleanup_function)( vp);
        }
    };

    typedef std::map< uintptr_t, fss_data >             fss_data_t;

#if ! defined(BOOST_FIBERS_NO_ATOMICS)
    std::atomic< std::size_t >                          use_count_;
#else
    std::size_t                                         use_count_;
#endif
#if ! defined(BOOST_FIBERS_NO_ATOMICS)
    detail::remote_ready_hook                           remote_ready_hook_{};
#endif
    detail::spinlock                                    splk_{};
    bool                                                terminated_{ false };
    wait_queue_t                                        wait_queue_{};
public:
    detail::wait_hook                                   wait_hook_{};
#if ! defined(BOOST_FIBERS_NO_ATOMICS)
    std::atomic< std::intptr_t >                        twstatus{ 0 };
#endif
private:
    scheduler                                       *   scheduler_{ nullptr };
    fss_data_t                                          fss_data_{};
    detail::sleep_hook                                  sleep_hook_{};
    detail::ready_hook                                  ready_hook_{};
    detail::terminated_hook                             terminated_hook_{};
    detail::worker_hook                                 worker_hook_{};
    fiber_properties                                *   properties_{ nullptr };
    boost::context::fiber                               c_{};
    std::chrono::steady_clock::time_point               tp_;
    type                                                type_;
    launch                                              policy_;

    context( std::size_t initial_count, type t, launch policy) noexcept :
        use_count_{ initial_count },
        tp_{ (std::chrono::steady_clock::time_point::max)() },
        type_{ t },
        policy_{ policy } {
    }

public:
    class id {
    private:
        context  *   impl_{ nullptr };

    public:
        id() = default;

        explicit id( context * impl) noexcept :
            impl_{ impl } {
        }

        bool operator==( id const& other) const noexcept {
            return impl_ == other.impl_;
        }

        bool operator!=( id const& other) const noexcept {
            return impl_ != other.impl_;
        }

        bool operator<( id const& other) const noexcept {
            return impl_ < other.impl_;
        }

        bool operator>( id const& other) const noexcept {
            return other.impl_ < impl_;
        }

        bool operator<=( id const& other) const noexcept {
            return ! ( * this > other);
        }

        bool operator>=( id const& other) const noexcept {
            return ! ( * this < other);
        }

        template< typename charT, class traitsT >
        friend std::basic_ostream< charT, traitsT > &
        operator<<( std::basic_ostream< charT, traitsT > & os, id const& other) {
            if ( nullptr != other.impl_) {
                return os << other.impl_;
            } else {
                return os << "{not-valid}";
            }
        }

        explicit operator bool() const noexcept {
            return nullptr != impl_;
        }

        bool operator!() const noexcept {
            return nullptr == impl_;
        }
    };

    static context * active() noexcept;

    static void reset_active() noexcept;

    context( context const&) = delete;
    context( context &&) = delete;
    context & operator=( context const&) = delete;
    context & operator=( context &&) = delete;

    friend bool
    operator==( context const& lhs, context const& rhs) noexcept {
        return & lhs == & rhs;
    }

    virtual ~context();

    scheduler * get_scheduler() const noexcept {
        return scheduler_;
    }

    id get_id() const noexcept;

    bool is_resumable() const noexcept {
        if ( c_) return true;
        else return false;
    }

    void resume() noexcept;
    void resume( detail::spinlock_lock &) noexcept;
    void resume( context *) noexcept;

    void suspend() noexcept;
    void suspend( detail::spinlock_lock &) noexcept;

    boost::context::fiber suspend_with_cc() noexcept;
    boost::context::fiber terminate() noexcept;

    void join();

    void yield() noexcept;

    bool wait_until( std::chrono::steady_clock::time_point const&) noexcept;
    bool wait_until( std::chrono::steady_clock::time_point const&,
                     detail::spinlock_lock &) noexcept;

    void schedule( context *) noexcept;

    bool is_context( type t) const noexcept {
        return type::none != ( type_ & t);
    }

    void * get_fss_data( void const * vp) const;

    void set_fss_data(
        void const * vp,
        detail::fss_cleanup_function::ptr_t const& cleanup_fn,
        void * data,
        bool cleanup_existing);

    void set_properties( fiber_properties * props) noexcept;

    fiber_properties * get_properties() const noexcept {
        return properties_;
    }

    launch get_policy() const noexcept {
        return policy_;
    }

    bool worker_is_linked() const noexcept;

    bool ready_is_linked() const noexcept;

    bool remote_ready_is_linked() const noexcept;

    bool sleep_is_linked() const noexcept;

    bool terminated_is_linked() const noexcept;

    bool wait_is_linked() const noexcept;

    template< typename List >
    void worker_link( List & lst) noexcept {
        static_assert( std::is_same< typename List::value_traits::hook_type, detail::worker_hook >::value, "not a worker-queue");
        BOOST_ASSERT( ! worker_is_linked() );
        lst.push_back( * this);
    }

    template< typename List >
    void ready_link( List & lst) noexcept {
        static_assert( std::is_same< typename List::value_traits::hook_type, detail::ready_hook >::value, "not a ready-queue");
        BOOST_ASSERT( ! ready_is_linked() );
        lst.push_back( * this);
    }

    template< typename List >
    void remote_ready_link( List & lst) noexcept {
        static_assert( std::is_same< typename List::value_traits::hook_type, detail::remote_ready_hook >::value, "not a remote-ready-queue");
        BOOST_ASSERT( ! remote_ready_is_linked() );
        lst.push_back( * this);
    }

    template< typename Set >
    void sleep_link( Set & set) noexcept {
        static_assert( std::is_same< typename Set::value_traits::hook_type,detail::sleep_hook >::value, "not a sleep-queue");
        BOOST_ASSERT( ! sleep_is_linked() );
        set.insert( * this);
    }

    template< typename List >
    void terminated_link( List & lst) noexcept {
        static_assert( std::is_same< typename List::value_traits::hook_type, detail::terminated_hook >::value, "not a terminated-queue");
        BOOST_ASSERT( ! terminated_is_linked() );
        lst.push_back( * this);
    }

    template< typename List >
    void wait_link( List & lst) noexcept {
        static_assert( std::is_same< typename List::value_traits::hook_type, detail::wait_hook >::value, "not a wait-queue");
        BOOST_ASSERT( ! wait_is_linked() );
        lst.push_back( * this);
    }

    void worker_unlink() noexcept;

    void ready_unlink() noexcept;

    void sleep_unlink() noexcept;

    void wait_unlink() noexcept;

    void detach() noexcept;

    void attach( context *) noexcept;

    friend void intrusive_ptr_add_ref( context * ctx) noexcept {
        BOOST_ASSERT( nullptr != ctx);
        ctx->use_count_.fetch_add( 1, std::memory_order_relaxed);
    }

    friend void intrusive_ptr_release( context * ctx) noexcept {
        BOOST_ASSERT( nullptr != ctx);
        if ( 1 == ctx->use_count_.fetch_sub( 1, std::memory_order_release) ) {
            std::atomic_thread_fence( std::memory_order_acquire);
            boost::context::fiber c = std::move( ctx->c_);
            // destruct context
            ctx->~context();
            // deallocated stack
            std::move( c).resume();
        }
    }
};

inline
bool operator<( context const& l, context const& r) noexcept {
    return l.get_id() < r.get_id();
}

template< typename Fn, typename ... Arg >
class worker_context final : public context {
private:
    typename std::decay< Fn >::type                     fn_;
    std::tuple< Arg ... >                               arg_;

    boost::context::fiber
    run_( boost::context::fiber && c) {
        {
            // fn and tpl must be destroyed before calling terminate()
            auto fn = std::move( fn_);
            auto arg = std::move( arg_);
#if (defined(BOOST_USE_UCONTEXT)||defined(BOOST_USE_WINFIB))
            std::move( c).resume();
#endif
#if defined(BOOST_NO_CXX17_STD_APPLY)
           boost::context::detail::apply( std::move( fn), std::move( arg) );
#else
           std::apply( std::move( fn), std::move( arg) );
#endif
        }
        // terminate context
        return terminate();
    }

public:
    template< typename StackAlloc >
    worker_context( launch policy,
                    boost::context::preallocated const& palloc, StackAlloc && salloc,
                    Fn && fn, Arg ... arg) :
            context{ 1, type::worker_context, policy },
            fn_( std::forward< Fn >( fn) ),
            arg_( std::forward< Arg >( arg) ... ) {
        c_ = boost::context::fiber{ std::allocator_arg, palloc, std::forward< StackAlloc >( salloc),
                                    std::bind( & worker_context::run_, this, std::placeholders::_1) };
#if (defined(BOOST_USE_UCONTEXT)||defined(BOOST_USE_WINFIB))
        c_ = std::move( c_).resume();
#endif
    }
};


template< typename StackAlloc, typename Fn, typename ... Arg >
static intrusive_ptr< context > make_worker_context( launch policy,
                                                     StackAlloc && salloc,
                                                     Fn && fn, Arg ... arg) {
    typedef worker_context< Fn, Arg ... >   context_t;

    auto sctx = salloc.allocate();
    // reserve space for control structure
    void * storage = reinterpret_cast< void * >(
            ( reinterpret_cast< uintptr_t >( sctx.sp) - static_cast< uintptr_t >( sizeof( context_t) ) )
            & ~ static_cast< uintptr_t >( 0xff) );
    void * stack_bottom = reinterpret_cast< void * >(
            reinterpret_cast< uintptr_t >( sctx.sp) - static_cast< uintptr_t >( sctx.size) );
    const std::size_t size = reinterpret_cast< uintptr_t >( storage) - reinterpret_cast< uintptr_t >( stack_bottom);
    // placement new of context on top of fiber's stack
    return intrusive_ptr< context >{ 
            new ( storage) context_t{
                policy,
                boost::context::preallocated{ storage, size, sctx },
                std::forward< StackAlloc >( salloc),
                std::forward< Fn >( fn),
                std::forward< Arg >( arg) ... } };
}

namespace detail {

inline
wait_functor::hook_ptr wait_functor::to_hook_ptr( wait_functor::value_type & value) {
    return & value.wait_hook_;
}

inline
wait_functor::const_hook_ptr wait_functor::to_hook_ptr( wait_functor::value_type const& value) {
    return & value.wait_hook_;
}

inline
wait_functor::pointer wait_functor::to_value_ptr( wait_functor::hook_ptr n) {
    return intrusive::get_parent_from_member< context >( n, & context::wait_hook_);
}

inline
wait_functor::const_pointer wait_functor::to_value_ptr( wait_functor::const_hook_ptr n) {
    return intrusive::get_parent_from_member< context >( n, & context::wait_hook_);
}

}}}

#ifdef _MSC_VER
# pragma warning(pop)
#endif

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_FIBERS_CONTEXT_H
