
//          Copyright Oliver Kowalke 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_FIBERS_BUFFERED_CHANNEL_H
#define BOOST_FIBERS_BUFFERED_CHANNEL_H

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

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
class buffered_channel {
public:
    typedef typename std::remove_reference< T >::type   value_type;

private:
    typedef context::wait_queue_t                       wait_queue_type;
	typedef value_type                                  slot_type;

    mutable detail::spinlock   splk_{};
    wait_queue_type                                     waiting_producers_{};
    wait_queue_type                                     waiting_consumers_{};
	slot_type                                       *   slots_;
	std::size_t                                         pidx_{ 0 };
	std::size_t                                         cidx_{ 0 };
	std::size_t                                         capacity_;
    bool                                                closed_{ false };

	bool is_full_() const noexcept {
		return cidx_ == ((pidx_ + 1) % capacity_);
	}

	bool is_empty_() const noexcept {
		return cidx_ == pidx_;
	}

    bool is_closed_() const noexcept {
        return closed_;
    }

public:
    explicit buffered_channel( std::size_t capacity) :
            capacity_{ capacity } {
        if ( BOOST_UNLIKELY( 2 > capacity_ || 0 != ( capacity_ & (capacity_ - 1) ) ) ) { 
            throw fiber_error{ std::make_error_code( std::errc::invalid_argument),
                               "boost fiber: buffer capacity is invalid" };
        }
        slots_ = new slot_type[capacity_];
    }

    ~buffered_channel() {
        close();
        delete [] slots_;
    }

    buffered_channel( buffered_channel const&) = delete;
    buffered_channel & operator=( buffered_channel const&) = delete;

    bool is_closed() const noexcept {
        detail::spinlock_lock lk{ splk_ };
        return is_closed_();
    }

    void close() noexcept {
        context * active_ctx = context::active();
        detail::spinlock_lock lk{ splk_ };
        if ( ! closed_) {
            closed_ = true;
            // notify all waiting producers
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
    }

    channel_op_status try_push( value_type const& value) {
        context * active_ctx = context::active();
        detail::spinlock_lock lk{ splk_ };
        if ( BOOST_UNLIKELY( is_closed_() ) ) {
            return channel_op_status::closed;
        } else if ( is_full_() ) {
            return channel_op_status::full;
        } else {
            slots_[pidx_] = value;
            pidx_ = (pidx_ + 1) % capacity_;
            // notify one waiting consumer
            while ( ! waiting_consumers_.empty() ) {
                context * consumer_ctx = & waiting_consumers_.front();
                waiting_consumers_.pop_front();
                std::intptr_t expected = reinterpret_cast< std::intptr_t >( this);
                if ( consumer_ctx->twstatus.compare_exchange_strong( expected, static_cast< std::intptr_t >( -1), std::memory_order_acq_rel) ) {
                    lk.unlock();
                    // notify context
                    active_ctx->schedule( consumer_ctx);
                    break;
                } else if ( static_cast< std::intptr_t >( 0) == expected) {
                    lk.unlock();
                    // no timed-wait op.
                    // notify context
                    active_ctx->schedule( consumer_ctx);
                    break;
                }
            }
            return channel_op_status::success;
        }
    }

    channel_op_status try_push( value_type && value) {
        context * active_ctx = context::active();
        detail::spinlock_lock lk{ splk_ };
        if ( BOOST_UNLIKELY( is_closed_() ) ) {
            return channel_op_status::closed;
        } else if ( is_full_() ) {
            return channel_op_status::full;
        } else {
            slots_[pidx_] = std::move( value);
            pidx_ = (pidx_ + 1) % capacity_;
            // notify one waiting consumer
            while ( ! waiting_consumers_.empty() ) {
                context * consumer_ctx = & waiting_consumers_.front();
                waiting_consumers_.pop_front();
                std::intptr_t expected = reinterpret_cast< std::intptr_t >( this);
                if ( consumer_ctx->twstatus.compare_exchange_strong( expected, static_cast< std::intptr_t >( -1), std::memory_order_acq_rel) ) {
                    lk.unlock();
                    // notify context
                    active_ctx->schedule( consumer_ctx);
                    break;
                } else if ( static_cast< std::intptr_t >( 0) == expected) {
                    lk.unlock();
                    // no timed-wait op.
                    // notify context
                    active_ctx->schedule( consumer_ctx);
                    break;
                }
            }
            return channel_op_status::success;
        }
    }

    channel_op_status push( value_type const& value) {
        context * active_ctx = context::active();
        for (;;) {
            detail::spinlock_lock lk{ splk_ };
            if ( BOOST_UNLIKELY( is_closed_() ) ) {
                return channel_op_status::closed;
            } else if ( is_full_() ) {
                active_ctx->wait_link( waiting_producers_);
                active_ctx->twstatus.store( static_cast< std::intptr_t >( 0), std::memory_order_release);
                // suspend this producer
                active_ctx->suspend( lk);
            } else {
                slots_[pidx_] = value;
                pidx_ = (pidx_ + 1) % capacity_;
                // notify one waiting consumer
                while ( ! waiting_consumers_.empty() ) {
                    context * consumer_ctx = & waiting_consumers_.front();
                    waiting_consumers_.pop_front();
                    std::intptr_t expected = reinterpret_cast< std::intptr_t >( this);
                    if ( consumer_ctx->twstatus.compare_exchange_strong( expected, static_cast< std::intptr_t >( -1), std::memory_order_acq_rel) ) {
                        lk.unlock();
                        // notify context
                        active_ctx->schedule( consumer_ctx);
                        break;
                    } else if ( static_cast< std::intptr_t >( 0) == expected) {
                        lk.unlock();
                        // no timed-wait op.
                        // notify context
                        active_ctx->schedule( consumer_ctx);
                        break;
                    }
                }
                return channel_op_status::success;
            }
        }
    }

    channel_op_status push( value_type && value) {
        context * active_ctx = context::active();
        for (;;) {
            detail::spinlock_lock lk{ splk_ };
            if ( BOOST_UNLIKELY( is_closed_() ) ) {
                return channel_op_status::closed;
            } else if ( is_full_() ) {
                active_ctx->wait_link( waiting_producers_);
                active_ctx->twstatus.store( static_cast< std::intptr_t >( 0), std::memory_order_release);
                // suspend this producer
                active_ctx->suspend( lk);
            } else {
                slots_[pidx_] = std::move( value);
                pidx_ = (pidx_ + 1) % capacity_;
                // notify one waiting consumer
                while ( ! waiting_consumers_.empty() ) {
                    context * consumer_ctx = & waiting_consumers_.front();
                    waiting_consumers_.pop_front();
                    std::intptr_t expected = reinterpret_cast< std::intptr_t >( this);
                    if ( consumer_ctx->twstatus.compare_exchange_strong( expected, static_cast< std::intptr_t >( -1), std::memory_order_acq_rel) ) {
                        lk.unlock();
                        // notify context
                        active_ctx->schedule( consumer_ctx);
                        break;
                    } else if ( static_cast< std::intptr_t >( 0) == expected) {
                        lk.unlock();
                        // no timed-wait op.
                        // notify context
                        active_ctx->schedule( consumer_ctx);
                        break;
                    }
                }
                return channel_op_status::success;
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
        std::chrono::steady_clock::time_point timeout_time = detail::convert( timeout_time_);
        for (;;) {
            detail::spinlock_lock lk{ splk_ };
            if ( BOOST_UNLIKELY( is_closed_() ) ) {
                return channel_op_status::closed;
            } else if ( is_full_() ) {
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
            } else {
                slots_[pidx_] = value;
                pidx_ = (pidx_ + 1) % capacity_;
                // notify one waiting consumer
                while ( ! waiting_consumers_.empty() ) {
                    context * consumer_ctx = & waiting_consumers_.front();
                    waiting_consumers_.pop_front();
                    std::intptr_t expected = reinterpret_cast< std::intptr_t >( this);
                    if ( consumer_ctx->twstatus.compare_exchange_strong( expected, static_cast< std::intptr_t >( -1), std::memory_order_acq_rel) ) {
                        lk.unlock();
                        // notify context
                        active_ctx->schedule( consumer_ctx);
                        break;
                    } else if ( static_cast< std::intptr_t >( 0) == expected) {
                        lk.unlock();
                        // no timed-wait op.
                        // notify context
                        active_ctx->schedule( consumer_ctx);
                        break;
                    }
                }
                return channel_op_status::success;
            }
        }
    }

    template< typename Clock, typename Duration >
    channel_op_status push_wait_until( value_type && value,
                                       std::chrono::time_point< Clock, Duration > const& timeout_time_) {
        context * active_ctx = context::active();
        std::chrono::steady_clock::time_point timeout_time = detail::convert( timeout_time_);
        for (;;) {
            detail::spinlock_lock lk{ splk_ };
            if ( BOOST_UNLIKELY( is_closed_() ) ) {
                return channel_op_status::closed;
            } else if ( is_full_() ) {
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
            } else {
                slots_[pidx_] = std::move( value);
                pidx_ = (pidx_ + 1) % capacity_;
                // notify one waiting consumer
                while ( ! waiting_consumers_.empty() ) {
                    context * consumer_ctx = & waiting_consumers_.front();
                    waiting_consumers_.pop_front();
                    std::intptr_t expected = reinterpret_cast< std::intptr_t >( this);
                    if ( consumer_ctx->twstatus.compare_exchange_strong( expected, static_cast< std::intptr_t >( -1), std::memory_order_acq_rel) ) {
                        lk.unlock();
                        // notify context
                        active_ctx->schedule( consumer_ctx);
                        break;
                    } else if ( static_cast< std::intptr_t >( 0) == expected) {
                        lk.unlock();
                        // no timed-wait op.
                        // notify context
                        active_ctx->schedule( consumer_ctx);
                        break;
                    }
                }
                return channel_op_status::success;
            }
        }
    }

    channel_op_status try_pop( value_type & value) {
        context * active_ctx = context::active();
        detail::spinlock_lock lk{ splk_ };
        if ( is_empty_() ) {
            return is_closed_()
                ? channel_op_status::closed
                : channel_op_status::empty;
        } else {
            value = std::move( slots_[cidx_]);
            cidx_ = (cidx_ + 1) % capacity_;
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
            return channel_op_status::success;
        }
    }

    channel_op_status pop( value_type & value) {
        context * active_ctx = context::active();
        for (;;) {
            detail::spinlock_lock lk{ splk_ };
            if ( is_empty_() ) {
                if ( BOOST_UNLIKELY( is_closed_() ) ) {
                    return channel_op_status::closed;
                } else {
                    active_ctx->wait_link( waiting_consumers_);
                    active_ctx->twstatus.store( static_cast< std::intptr_t >( 0), std::memory_order_release);
                    // suspend this consumer
                    active_ctx->suspend( lk);
                }
            } else {
                value = std::move( slots_[cidx_]);
                cidx_ = (cidx_ + 1) % capacity_;
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
                return channel_op_status::success;
            }
        }
    }

    value_type value_pop() {
        context * active_ctx = context::active();
        for (;;) {
            detail::spinlock_lock lk{ splk_ };
            if ( is_empty_() ) {
                if ( BOOST_UNLIKELY( is_closed_() ) ) {
                    throw fiber_error{
                        std::make_error_code( std::errc::operation_not_permitted),
                        "boost fiber: channel is closed" };
                } else {
                    active_ctx->wait_link( waiting_consumers_);
                    active_ctx->twstatus.store( static_cast< std::intptr_t >( 0), std::memory_order_release);
                    // suspend this consumer
                    active_ctx->suspend( lk);
                }
            } else {
                value_type value = std::move( slots_[cidx_]);
                cidx_ = (cidx_ + 1) % capacity_;
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
                return std::move( value);
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
        std::chrono::steady_clock::time_point timeout_time = detail::convert( timeout_time_);
        for (;;) {
            detail::spinlock_lock lk{ splk_ };
            if ( is_empty_() ) {
                if ( BOOST_UNLIKELY( is_closed_() ) ) {
                    return channel_op_status::closed;
                } else {
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
            } else {
                value = std::move( slots_[cidx_]);
                cidx_ = (cidx_ + 1) % capacity_;
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
                return channel_op_status::success;
            }
        }
    }

    class iterator {
    private:
        typedef typename std::aligned_storage< sizeof( value_type), alignof( value_type) >::type  storage_type;

        buffered_channel    *   chan_{ nullptr };
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

        explicit iterator( buffered_channel< T > * chan) noexcept :
            chan_{ chan } {
            increment_();
        }

        iterator( iterator const& other) noexcept :
            chan_{ other.chan_ } {
        }

        iterator & operator=( iterator const& other) noexcept {
            if ( BOOST_LIKELY( this != & other) ) {
                chan_ = other.chan_;
            }
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
typename buffered_channel< T >::iterator
begin( buffered_channel< T > & chan) {
    return typename buffered_channel< T >::iterator( & chan);
}

template< typename T >
typename buffered_channel< T >::iterator
end( buffered_channel< T > &) {
    return typename buffered_channel< T >::iterator();
}

}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif

#endif // BOOST_FIBERS_BUFFERED_CHANNEL_H
