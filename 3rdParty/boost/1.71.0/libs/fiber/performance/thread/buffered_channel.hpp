
//          Copyright Oliver Kowalke 2016.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// based on Dmitry Vyukov's MPMC queue
// (http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue)

#ifndef BUFFERED_CHANNEL_H
#define BUFFERED_CHANNEL_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/fiber/detail/config.hpp>

enum class channel_op_status {
    success = 0,
    empty,
    full,
    closed,
    timeout
};

template< typename T >
class buffered_channel {
public:
    typedef T   value_type;

private:
    typedef typename std::aligned_storage< sizeof( T), alignof( T) >::type  storage_type;

    struct alignas(cache_alignment) slot {
        std::atomic< std::size_t >  cycle{ 0 };
        storage_type                storage{};

        slot() = default;
    };

    // procuder cacheline
    alignas(cache_alignment) std::atomic< std::size_t >     producer_idx_{ 0 };
    // consumer cacheline
    alignas(cache_alignment) std::atomic< std::size_t >     consumer_idx_{ 0 };
    // shared write cacheline
    alignas(cache_alignment) std::atomic_bool               closed_{ false };
    mutable std::mutex                                      mtx_{};
    std::condition_variable                                 not_full_cnd_{};
    std::condition_variable                                 not_empty_cnd_{};
    // shared read cacheline
    alignas(cache_alignment) slot                        *  slots_{ nullptr };
    std::size_t                                             capacity_;
    char                                                    pad_[cacheline_length];
    std::size_t                                             waiting_consumer_{ 0 };

    bool is_full_() {
        std::size_t idx{ producer_idx_.load( std::memory_order_relaxed) };
        return 0 > static_cast< std::intptr_t >( slots_[idx & (capacity_ - 1)].cycle.load( std::memory_order_acquire) ) - static_cast< std::intptr_t >( idx);
    }

    bool is_empty_() {
        std::size_t idx{ consumer_idx_.load( std::memory_order_relaxed) };
        return 0 > static_cast< std::intptr_t >( slots_[idx & (capacity_ - 1)].cycle.load( std::memory_order_acquire) ) - static_cast< std::intptr_t >( idx + 1);
    }

    template< typename ValueType >
    channel_op_status try_push_( ValueType && value) {
        slot * s{ nullptr };
        std::size_t idx{ producer_idx_.load( std::memory_order_relaxed) };
        for (;;) {
            s = & slots_[idx & (capacity_ - 1)];
            std::size_t cycle{ s->cycle.load( std::memory_order_acquire) };
            std::intptr_t diff{ static_cast< std::intptr_t >( cycle) - static_cast< std::intptr_t >( idx) };
            if ( 0 == diff) {
                if ( producer_idx_.compare_exchange_weak( idx, idx + 1, std::memory_order_relaxed) ) {
                    break;
                }
            } else if ( 0 > diff) {
                return channel_op_status::full;
            } else {
                idx = producer_idx_.load( std::memory_order_relaxed);
            }
        }
        ::new ( static_cast< void * >( std::addressof( s->storage) ) ) value_type( std::forward< ValueType >( value) );
        s->cycle.store( idx + 1, std::memory_order_release);
        return channel_op_status::success;
    }

    channel_op_status try_value_pop_( slot *& s, std::size_t & idx) {
        idx = consumer_idx_.load( std::memory_order_relaxed);
        for (;;) {
            s = & slots_[idx & (capacity_ - 1)];
            std::size_t cycle = s->cycle.load( std::memory_order_acquire);
            std::intptr_t diff{ static_cast< std::intptr_t >( cycle) - static_cast< std::intptr_t >( idx + 1) };
            if ( 0 == diff) {
                if ( consumer_idx_.compare_exchange_weak( idx, idx + 1, std::memory_order_relaxed) ) {
                    break;
                }
            } else if ( 0 > diff) {
                return channel_op_status::empty;
            } else {
                idx = consumer_idx_.load( std::memory_order_relaxed);
            }
        }
        // incrementing the slot cycle must be deferred till the value has been consumed
        // slot cycle tells procuders that the cell can be re-used (store new value)
        return channel_op_status::success;
    }

    channel_op_status try_pop_( value_type & value) {
        slot * s{ nullptr };
        std::size_t idx{ 0 };
        channel_op_status status{ try_value_pop_( s, idx) };
        if ( channel_op_status::success == status) {
            value = std::move( * reinterpret_cast< value_type * >( std::addressof( s->storage) ) );
            s->cycle.store( idx + capacity_, std::memory_order_release);
        }
        return status;
    }

public:
    explicit buffered_channel( std::size_t capacity) :
        capacity_{ capacity } {
        if ( 0 == capacity_ || 0 != ( capacity_ & (capacity_ - 1) ) ) { 
            throw std::runtime_error{ "boost fiber: buffer capacity is invalid" };
        }
        slots_ = new slot[capacity_]();
        for ( std::size_t i = 0; i < capacity_; ++i) {
            slots_[i].cycle.store( i, std::memory_order_relaxed);
        }
    }

    ~buffered_channel() {
        close();
        for (;;) {
            slot * s{ nullptr };
            std::size_t idx{ 0 };
            if ( channel_op_status::success == try_value_pop_( s, idx) ) {
                reinterpret_cast< value_type * >( std::addressof( s->storage) )->~value_type();
                s->cycle.store( idx + capacity_, std::memory_order_release);
            } else {
                break;
            }
        }
        delete [] slots_;
    }

    buffered_channel( buffered_channel const&) = delete;
    buffered_channel & operator=( buffered_channel const&) = delete;

    bool is_closed() const noexcept {
        return closed_.load( std::memory_order_acquire);
    }

    void close() noexcept {
        std::unique_lock< std::mutex > lk{ mtx_ };
        closed_.store( true, std::memory_order_release);
        not_full_cnd_.notify_all();
        not_empty_cnd_.notify_all();
    }

    channel_op_status push( value_type const& value) {
        for (;;) {
            if ( is_closed() ) {
                return channel_op_status::closed;
            }
            channel_op_status status{ try_push_( value) };
            if ( channel_op_status::success == status) {
                std::unique_lock< std::mutex > lk{ mtx_ };
                if ( 0 < waiting_consumer_) {
                    not_empty_cnd_.notify_one();
                }
                return status;
            } else if ( channel_op_status::full == status) {
                std::unique_lock< std::mutex > lk{ mtx_ };
                if ( is_closed() ) {
                    return channel_op_status::closed;
                }
                if ( ! is_full_() ) {
                    continue;
                }
                not_full_cnd_.wait( lk, [this]{ return is_closed() || ! is_full_(); });
            } else {
                BOOST_ASSERT( channel_op_status::closed == status);
                return status;
            }
        }
    }

    value_type value_pop() {
        for (;;) {
            slot * s{ nullptr };
            std::size_t idx{ 0 };
            channel_op_status status{ try_value_pop_( s, idx) };
            if ( channel_op_status::success == status) {
                value_type value{ std::move( * reinterpret_cast< value_type * >( std::addressof( s->storage) ) ) };
                s->cycle.store( idx + capacity_, std::memory_order_release);
                not_full_cnd_.notify_one();
                return std::move( value);
            } else if ( channel_op_status::empty == status) {
                std::unique_lock< std::mutex > lk{ mtx_ };
                ++waiting_consumer_;
                if ( is_closed() ) {
                    throw std::runtime_error{ "boost fiber: channel is closed" };
                }
                if ( ! is_empty_() ) {
                    continue;
                }
                not_empty_cnd_.wait( lk, [this](){ return is_closed() || ! is_empty_(); });
                --waiting_consumer_;
            } else {
                BOOST_ASSERT( channel_op_status::closed == status);
                throw std::runtime_error{ "boost fiber: channel is closed" };
            }
        }
    }
};

#endif // BUFFERED_CHANNEL_H
