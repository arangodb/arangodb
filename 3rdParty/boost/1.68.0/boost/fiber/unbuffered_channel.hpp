
//          Copyright Oliver Kowalke 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_FIBERS_UNBUFFERED_CHANNEL_H
#define BOOST_FIBERS_UNBUFFERED_CHANNEL_H

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <boost/config.hpp>

#include <boost/fiber/channel_op_status.hpp>
#include <boost/fiber/context.hpp>
#include <boost/fiber/detail/config.hpp>
#include <boost/fiber/detail/convert.hpp>
#include <boost/fiber/detail/spinlock.hpp>
#include <boost/fiber/exceptions.hpp>

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {

template< typename T >
class unbuffered_channel {
public:
    typedef typename std::remove_reference< T >::type   value_type;

private:
    typedef context::wait_queue_t   wait_queue_type;

    struct slot {
        value_type  value;
        context *   ctx;

        slot( value_type const& value_, context * ctx_) :
            value{ value_ },
            ctx{ ctx_ } {
        }

        slot( value_type && value_, context * ctx_) :
            value{ std::move( value_) },
            ctx{ ctx_ } {
        }
    };

    // shared cacheline
    std::atomic< slot * >       slot_{ nullptr };
    // shared cacheline
    std::atomic_bool            closed_{ false };
    mutable detail::spinlock    splk_producers_{};
    wait_queue_type             waiting_producers_{};
    mutable detail::spinlock    splk_consumers_{};
    wait_queue_type             waiting_consumers_{};
    char                        pad_[cacheline_length];

    bool is_empty_() {
        return nullptr == slot_.load( std::memory_order_acquire);
    }

    bool try_push_( slot * own_slot) {
        for (;;) {
            slot * s = slot_.load( std::memory_order_acquire);
            if ( nullptr == s) {
                if ( ! slot_.compare_exchange_strong( s, own_slot, std::memory_order_acq_rel) ) {
                    continue;
                }
                return true;
            } else {
                return false;
            }
        }
    }

    slot * try_pop_() {
        slot * nil_slot = nullptr;
        for (;;) {
            slot * s = slot_.load( std::memory_order_acquire);
            if ( nullptr != s) {
                if ( ! slot_.compare_exchange_strong( s, nil_slot, std::memory_order_acq_rel) ) {
                    continue;}
            }
            return s;
        }
    }

public:
    unbuffered_channel() {
    }

    ~unbuffered_channel() {
        close();
    }

    unbuffered_channel( unbuffered_channel const&) = delete;
    unbuffered_channel & operator=( unbuffered_channel const&) = delete;

    bool is_closed() const noexcept {
        return closed_.load( std::memory_order_acquire);
    }

    void close() noexcept {
        context * active_ctx = context::active();
        // notify all waiting producers
        closed_.store( true, std::memory_order_release);
        detail::spinlock_lock lk1{ splk_producers_ };
        while ( ! waiting_producers_.empty() ) {
            context * producer_ctx = & waiting_producers_.front();
            waiting_producers_.pop_front();
            std::intptr_t expected = reinterpret_cast< std::intptr_t >( this);
            if ( producer_ctx->twstatus.compare_exchange_strong( expected, static_cast< std::intptr_t >( -1), std::memory_order_acq_rel) ) {
                // notify context
                active_ctx->schedule( producer_ctx);
            } else if ( static_cast< std::intptr_t >( 0) == expected) {
                // no timed-wait op.
                // notify context
                active_ctx->schedule( producer_ctx);
            }
        }
        // notify all waiting consumers
        detail::spinlock_lock lk2{ splk_consumers_ };
        while ( ! waiting_consumers_.empty() ) {
            context * consumer_ctx = & waiting_consumers_.front();
            waiting_consumers_.pop_front();
            std::intptr_t expected = reinterpret_cast< std::intptr_t >( this);
            if ( consumer_ctx->twstatus.compare_exchange_strong( expected, static_cast< std::intptr_t >( -1), std::memory_order_acq_rel) ) {
                // notify context
                active_ctx->schedule( consumer_ctx);
            } else if ( static_cast< std::intptr_t >( 0) == expected) {
                // no timed-wait op.
                // notify context
                active_ctx->schedule( consumer_ctx);
            }
        }
    }

    channel_op_status push( value_type const& value) {
        context * active_ctx = context::active();
        slot s{ value, active_ctx };
        for (;;) {
            if ( BOOST_UNLIKELY( is_closed() ) ) {
                return channel_op_status::closed;
            }
            if ( try_push_( & s) ) {
                detail::spinlock_lock lk{ splk_consumers_ };
                // notify one waiting consumer
                while ( ! waiting_consumers_.empty() ) {
                    context * consumer_ctx = & waiting_consumers_.front();
                    waiting_consumers_.pop_front();
                    std::intptr_t expected = reinterpret_cast< std::intptr_t >( this);
                    if ( consumer_ctx->twstatus.compare_exchange_strong( expected, static_cast< std::intptr_t >( -1), std::memory_order_acq_rel) ) {
                        // notify context
                        active_ctx->schedule( consumer_ctx);
                        break;
                    } else if ( static_cast< std::intptr_t >( 0) == expected) {
                        // no timed-wait op.
                        // notify context
                        active_ctx->schedule( consumer_ctx);
                        break;
                    }
                }
                // suspend till value has been consumed
                active_ctx->suspend( lk);
                // resumed, value has been consumed
                return channel_op_status::success;
            } else {
                detail::spinlock_lock lk{ splk_producers_ };
                if ( BOOST_UNLIKELY( is_closed() ) ) {
                    return channel_op_status::closed;
                }
                if ( is_empty_() ) {
                    continue;
                }
                active_ctx->wait_link( waiting_producers_);
                active_ctx->twstatus.store( static_cast< std::intptr_t >( 0), std::memory_order_release);
                // suspend this producer
                active_ctx->suspend( lk);
                // resumed, slot mabye free
            }
        }
    }

    channel_op_status push( value_type && value) {
        context * active_ctx = context::active();
        slot s{ std::move( value), active_ctx };
        for (;;) {
            if ( BOOST_UNLIKELY( is_closed() ) ) {
                return channel_op_status::closed;
            }
            if ( try_push_( & s) ) {
                detail::spinlock_lock lk{ splk_consumers_ };
                // notify one waiting consumer
                while ( ! waiting_consumers_.empty() ) {
                    context * consumer_ctx = & waiting_consumers_.front();
                    waiting_consumers_.pop_front();
                    std::intptr_t expected = reinterpret_cast< std::intptr_t >( this);
                    if ( consumer_ctx->twstatus.compare_exchange_strong( expected, static_cast< std::intptr_t >( -1), std::memory_order_acq_rel) ) {
                        // notify context
                        active_ctx->schedule( consumer_ctx);
                        break;
                    } else if ( static_cast< std::intptr_t >( 0) == expected) {
                        // no timed-wait op.
                        // notify context
                        active_ctx->schedule( consumer_ctx);
                        break;
                    }
                }
                // suspend till value has been consumed
                active_ctx->suspend( lk);
                // resumed, value has been consumed
                return channel_op_status::success;
            } else {
                detail::spinlock_lock lk{ splk_producers_ };
                if ( BOOST_UNLIKELY( is_closed() ) ) {
                    return channel_op_status::closed;
                }
                if ( is_empty_() ) {
                    continue;
                }
                active_ctx->wait_link( waiting_producers_);
                active_ctx->twstatus.store( static_cast< std::intptr_t >( 0), std::memory_order_release);
                // suspend this producer
                active_ctx->suspend( lk);
                // resumed, slot mabye free
            }
        }
    }

    template< typename Rep, typename Period >
    channel_op_status push_wait_for( value_type const& value,
                                     std::chrono::duration< Rep, Period > const& timeout_duration) {
        return push_wait_until( value,
                                std::chrono::steady_clock::now() + timeout_duration);
    }

    template< typename Rep, typename Period >
    channel_op_status push_wait_for( value_type && value,
                                     std::chrono::duration< Rep, Period > const& timeout_duration) {
        return push_wait_until( std::forward< value_type >( value),
                                std::chrono::steady_clock::now() + timeout_duration);
    }

    template< typename Clock, typename Duration >
    channel_op_status push_wait_until( value_type const& value,
                                       std::chrono::time_point< Clock, Duration > const& timeout_time_) {
        context * active_ctx = context::active();
        slot s{ value, active_ctx };
        std::chrono::steady_clock::time_point timeout_time = detail::convert( timeout_time_);
        for (;;) {
            if ( BOOST_UNLIKELY( is_closed() ) ) {
                return channel_op_status::closed;
            }
            if ( try_push_( & s) ) {
                detail::spinlock_lock lk{ splk_consumers_ };
                // notify one waiting consumer
                while ( ! waiting_consumers_.empty() ) {
                    context * consumer_ctx = & waiting_consumers_.front();
                    waiting_consumers_.pop_front();
                    std::intptr_t expected = reinterpret_cast< std::intptr_t >( this);
                    if ( consumer_ctx->twstatus.compare_exchange_strong( expected, static_cast< std::intptr_t >( -1), std::memory_order_acq_rel) ) {
                        // notify context
                        active_ctx->schedule( consumer_ctx);
                        break;
                    } else if ( static_cast< std::intptr_t >( 0) == expected) {
                        // no timed-wait op.
                        // notify context
                        active_ctx->schedule( consumer_ctx);
                        break;
                    }
                }
                // suspend this producer
                active_ctx->twstatus.store( reinterpret_cast< std::intptr_t >( this), std::memory_order_release);
                if ( ! active_ctx->wait_until( timeout_time, lk) ) {
                    // clear slot
                    slot * nil_slot = nullptr, * own_slot = & s;
                    slot_.compare_exchange_strong( own_slot, nil_slot, std::memory_order_acq_rel);
                    // resumed, value has not been consumed
                    return channel_op_status::timeout;
                }
                // resumed, value has been consumed
                return channel_op_status::success;
            } else {
                detail::spinlock_lock lk{ splk_producers_ };
                if ( BOOST_UNLIKELY( is_closed() ) ) {
                    return channel_op_status::closed;
                }
                if ( is_empty_() ) {
                    continue;
                }
                active_ctx->wait_link( waiting_producers_);
                active_ctx->twstatus.store( reinterpret_cast< std::intptr_t >( this), std::memory_order_release);
                // suspend this producer
                if ( ! active_ctx->wait_until( timeout_time, lk) ) {
                    // relock local lk
                    lk.lock();
                    // remove from waiting-queue
                    waiting_producers_.remove( * active_ctx);
                    return channel_op_status::timeout;
                }
                // resumed, slot maybe free
            }
        }
    }

    template< typename Clock, typename Duration >
    channel_op_status push_wait_until( value_type && value,
                                       std::chrono::time_point< Clock, Duration > const& timeout_time_) {
        context * active_ctx = context::active();
        slot s{ std::move( value), active_ctx };
        std::chrono::steady_clock::time_point timeout_time = detail::convert( timeout_time_);
        for (;;) {
            if ( BOOST_UNLIKELY( is_closed() ) ) {
                return channel_op_status::closed;
            }
            if ( try_push_( & s) ) {
                detail::spinlock_lock lk{ splk_consumers_ };
                // notify one waiting consumer
                while ( ! waiting_consumers_.empty() ) {
                    context * consumer_ctx = & waiting_consumers_.front();
                    waiting_consumers_.pop_front();
                    std::intptr_t expected = reinterpret_cast< std::intptr_t >( this);
                    if ( consumer_ctx->twstatus.compare_exchange_strong( expected, static_cast< std::intptr_t >( -1), std::memory_order_acq_rel) ) {
                        // notify context
                        active_ctx->schedule( consumer_ctx);
                        break;
                    } else if ( static_cast< std::intptr_t >( 0) == expected) {
                        // no timed-wait op.
                        // notify context
                        active_ctx->schedule( consumer_ctx);
                        break;
                    }
                }
                // suspend this producer
                active_ctx->twstatus.store( reinterpret_cast< std::intptr_t >( this), std::memory_order_release);
                if ( ! active_ctx->wait_until( timeout_time, lk) ) {
                    // clear slot
                    slot * nil_slot = nullptr, * own_slot = & s;
                    slot_.compare_exchange_strong( own_slot, nil_slot, std::memory_order_acq_rel);
                    // resumed, value has not been consumed
                    return channel_op_status::timeout;
                }
                // resumed, value has been consumed
                return channel_op_status::success;
            } else {
                detail::spinlock_lock lk{ splk_producers_ };
                if ( BOOST_UNLIKELY( is_closed() ) ) {
                    return channel_op_status::closed;
                }
                if ( is_empty_() ) {
                    continue;
                }
                active_ctx->wait_link( waiting_producers_);
                active_ctx->twstatus.store( reinterpret_cast< std::intptr_t >( this), std::memory_order_release);
                // suspend this producer
                if ( ! active_ctx->wait_until( timeout_time, lk) ) {
                    // relock local lk
                    lk.lock();
                    // remove from waiting-queue
                    waiting_producers_.remove( * active_ctx);
                    return channel_op_status::timeout;
                }
                // resumed, slot maybe free
            }
        }
    }

    channel_op_status pop( value_type & value) {
        context * active_ctx = context::active();
        slot * s = nullptr;
        for (;;) {
            if ( nullptr != ( s = try_pop_() ) ) {
                {
                    detail::spinlock_lock lk{ splk_producers_ };
                    // notify one waiting producer
                    while ( ! waiting_producers_.empty() ) {
                        context * producer_ctx = & waiting_producers_.front();
                        waiting_producers_.pop_front();
                        std::intptr_t expected = reinterpret_cast< std::intptr_t >( this);
                        if ( producer_ctx->twstatus.compare_exchange_strong( expected, static_cast< std::intptr_t >( -1), std::memory_order_acq_rel) ) {
                            lk.unlock();
                            // notify context
                            active_ctx->schedule( producer_ctx);
                            break;
                        } else if ( static_cast< std::intptr_t >( 0) == expected) {
                            lk.unlock();
                            // no timed-wait op.
                            // notify context
                            active_ctx->schedule( producer_ctx);
                            break;
                        }
                    }
                }
                value = std::move( s->value);
                // notify context
                active_ctx->schedule( s->ctx);
                return channel_op_status::success;
            } else {
                detail::spinlock_lock lk{ splk_consumers_ };
                if ( BOOST_UNLIKELY( is_closed() ) ) {
                    return channel_op_status::closed;
                }
                if ( ! is_empty_() ) {
                    continue;
                }
                active_ctx->wait_link( waiting_consumers_);
                active_ctx->twstatus.store( static_cast< std::intptr_t >( 0), std::memory_order_release);
                // suspend this consumer
                active_ctx->suspend( lk);
                // resumed, slot mabye set
            }
        }
    }

    value_type value_pop() {
        context * active_ctx = context::active();
        slot * s = nullptr;
        for (;;) {
            if ( nullptr != ( s = try_pop_() ) ) {
                {
                    detail::spinlock_lock lk{ splk_producers_ };
                    // notify one waiting producer
                    while ( ! waiting_producers_.empty() ) {
                        context * producer_ctx = & waiting_producers_.front();
                        waiting_producers_.pop_front();
                        std::intptr_t expected = reinterpret_cast< std::intptr_t >( this);
                        if ( producer_ctx->twstatus.compare_exchange_strong( expected, static_cast< std::intptr_t >( -1), std::memory_order_acq_rel) ) {
                            lk.unlock();
                            // notify context
                            active_ctx->schedule( producer_ctx);
                            break;
                        } else if ( static_cast< std::intptr_t >( 0) == expected) {
                            lk.unlock();
                            // no timed-wait op.
                            // notify context
                            active_ctx->schedule( producer_ctx);
                            break;
                        }
                    }
                }
                // consume value
                value_type value = std::move( s->value);
                // notify context
                active_ctx->schedule( s->ctx);
                return std::move( value);
            } else {
                detail::spinlock_lock lk{ splk_consumers_ };
                if ( BOOST_UNLIKELY( is_closed() ) ) {
                    throw fiber_error{
                            std::make_error_code( std::errc::operation_not_permitted),
                            "boost fiber: channel is closed" };
                }
                if ( ! is_empty_() ) {
                    continue;
                }
                active_ctx->wait_link( waiting_consumers_);
                active_ctx->twstatus.store( static_cast< std::intptr_t >( 0), std::memory_order_release);
                // suspend this consumer
                active_ctx->suspend( lk);
                // resumed, slot mabye set
            }
        }
    }

    template< typename Rep, typename Period >
    channel_op_status pop_wait_for( value_type & value,
                                    std::chrono::duration< Rep, Period > const& timeout_duration) {
        return pop_wait_until( value,
                               std::chrono::steady_clock::now() + timeout_duration);
    }

    template< typename Clock, typename Duration >
    channel_op_status pop_wait_until( value_type & value,
                                      std::chrono::time_point< Clock, Duration > const& timeout_time_) {
        context * active_ctx = context::active();
        slot * s = nullptr;
        std::chrono::steady_clock::time_point timeout_time = detail::convert( timeout_time_);
        for (;;) {
            if ( nullptr != ( s = try_pop_() ) ) {
                {
                    detail::spinlock_lock lk{ splk_producers_ };
                    // notify one waiting producer
                    while ( ! waiting_producers_.empty() ) {
                        context * producer_ctx = & waiting_producers_.front();
                        waiting_producers_.pop_front();
                        std::intptr_t expected = reinterpret_cast< std::intptr_t >( this);
                        if ( producer_ctx->twstatus.compare_exchange_strong( expected, static_cast< std::intptr_t >( -1), std::memory_order_acq_rel) ) {
                            lk.unlock();
                            // notify context
                            active_ctx->schedule( producer_ctx);
                            break;
                        } else if ( static_cast< std::intptr_t >( 0) == expected) {
                            lk.unlock();
                            // no timed-wait op.
                            // notify context
                            active_ctx->schedule( producer_ctx);
                            break;
                        }
                    }
                }
                // consume value
                value = std::move( s->value);
                // notify context
                active_ctx->schedule( s->ctx);
                return channel_op_status::success;
            } else {
                detail::spinlock_lock lk{ splk_consumers_ };
                if ( BOOST_UNLIKELY( is_closed() ) ) {
                    return channel_op_status::closed;
                }
                if ( ! is_empty_() ) {
                    continue;
                }
                active_ctx->wait_link( waiting_consumers_);
                active_ctx->twstatus.store( reinterpret_cast< std::intptr_t >( this), std::memory_order_release);
                // suspend this consumer
                if ( ! active_ctx->wait_until( timeout_time, lk) ) {
                    // relock local lk
                    lk.lock();
                    // remove from waiting-queue
                    waiting_consumers_.remove( * active_ctx);
                    return channel_op_status::timeout;
                }
            }
        }
    }

    class iterator {
    private:
        typedef typename std::aligned_storage< sizeof( value_type), alignof( value_type) >::type  storage_type;

        unbuffered_channel  *   chan_{ nullptr };
        storage_type            storage_;

        void increment_() {
            BOOST_ASSERT( nullptr != chan_);
            try {
                ::new ( static_cast< void * >( std::addressof( storage_) ) ) value_type{ chan_->value_pop() };
            } catch ( fiber_error const&) {
                chan_ = nullptr;
            }
        }

    public:
        typedef std::input_iterator_tag                     iterator_category;
        typedef std::ptrdiff_t                              difference_type;
        typedef value_type                              *   pointer;
        typedef value_type                              &   reference;

        typedef pointer     pointer_t;
        typedef reference   reference_t;

        iterator() noexcept = default;

        explicit iterator( unbuffered_channel< T > * chan) noexcept :
            chan_{ chan } {
            increment_();
        }

        iterator( iterator const& other) noexcept :
            chan_{ other.chan_ } {
        }

        iterator & operator=( iterator const& other) noexcept {
            if ( this == & other) return * this;
            chan_ = other.chan_;
            return * this;
        }

        bool operator==( iterator const& other) const noexcept {
            return other.chan_ == chan_;
        }

        bool operator!=( iterator const& other) const noexcept {
            return other.chan_ != chan_;
        }

        iterator & operator++() {
            increment_();
            return * this;
        }

        iterator operator++( int) = delete;

        reference_t operator*() noexcept {
            return * reinterpret_cast< value_type * >( std::addressof( storage_) );
        }

        pointer_t operator->() noexcept {
            return reinterpret_cast< value_type * >( std::addressof( storage_) );
        }
    };

    friend class iterator;
};

template< typename T >
typename unbuffered_channel< T >::iterator
begin( unbuffered_channel< T > & chan) {
    return typename unbuffered_channel< T >::iterator( & chan);
}

template< typename T >
typename unbuffered_channel< T >::iterator
end( unbuffered_channel< T > &) {
    return typename unbuffered_channel< T >::iterator();
}

}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_FIBERS_UNBUFFERED_CHANNEL_H
