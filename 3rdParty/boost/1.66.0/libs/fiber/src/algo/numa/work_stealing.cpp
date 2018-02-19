
//          Copyright Oliver Kowalke 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/fiber/algo/numa/work_stealing.hpp"

#include <cmath>
#include <random>

#include <boost/assert.hpp>
#include <boost/context/detail/prefetch.hpp>

#include "boost/fiber/type.hpp"

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {
namespace algo {
namespace numa {

std::vector< intrusive_ptr< work_stealing > > work_stealing::schedulers_{};

std::vector< std::uint32_t > get_local_cpus( std::uint32_t node_id, std::vector< boost::fibers::numa::node > const& topo) {
    for ( auto & node : topo) {
        if ( node_id == node.id) {
            // store IDs of logical cpus that belong to this local NUMA node
            return std::vector< std::uint32_t >{ node.logical_cpus.begin(), node.logical_cpus.end() };
        }
    }
    return std::vector< std::uint32_t >{};
}

std::vector< std::uint32_t > get_remote_cpus( std::uint32_t node_id, std::vector< boost::fibers::numa::node > const& topo) {
    std::vector< std::uint32_t > remote_cpus;
    for ( auto & node : topo) {
        if ( node_id != node.id) {
            // store IDs of logical cpus that belong to a remote NUMA node
            // no ordering regarding to the NUMA distance
            remote_cpus.insert( remote_cpus.end(), node.logical_cpus.begin(), node.logical_cpus.end() );
        }
    }
    return remote_cpus;
}

void
work_stealing::init_( std::vector< boost::fibers::numa::node > const& topo,
                      std::vector< intrusive_ptr< work_stealing > > & schedulers) {
    std::uint32_t max_cpu_id = 0;
    for ( auto & node : topo) {
        max_cpu_id = (std::max)( max_cpu_id, * node.logical_cpus.rbegin() );
    }
    // resize array of schedulers to max. CPU ID, initilized with nullptr
    // CPU ID acts as the index in the scheduler array
    // if a logical cpus is offline, schedulers_ will contain a nullptr
    // logical cpus index starts at `0`
    std::vector< intrusive_ptr< work_stealing > >{ max_cpu_id + 1, nullptr }.swap( schedulers);
}

work_stealing::work_stealing(
    std::uint32_t cpu_id,
    std::uint32_t node_id,
    std::vector< boost::fibers::numa::node > const& topo,
    bool suspend) :
        cpu_id_{ cpu_id },
        local_cpus_{ get_local_cpus( node_id, topo) },
        remote_cpus_{ get_remote_cpus( node_id, topo) },
        suspend_{ suspend } {
    // pin current thread to logical cpu
    boost::fibers::numa::pin_thread( cpu_id_);
    // initialize the array of schedulers
    static std::once_flag flag;
    std::call_once( flag, & work_stealing::init_, topo, std::ref( schedulers_) );
    // register pointer of this scheduler
    schedulers_[cpu_id_] = this;
}

void
work_stealing::awakened( context * ctx) noexcept {
    if ( ! ctx->is_context( type::pinned_context) ) {
        ctx->detach();
    }
    rqueue_.push( ctx);
}

context *
work_stealing::pick_next() noexcept {
    context * victim = rqueue_.pop();
    if ( nullptr != victim) {
        boost::context::detail::prefetch_range( victim, sizeof( context) );
        if ( ! victim->is_context( type::pinned_context) ) {
            context::active()->attach( victim);
        }
    } else {
        std::uint32_t cpu_id = 0;
        std::size_t count = 0, size = local_cpus_.size();
        static thread_local std::minstd_rand generator{ std::random_device{}() };
        std::uniform_int_distribution< std::uint32_t > local_distribution{
            0, static_cast< std::uint32_t >( local_cpus_.size() - 1) };
        std::uniform_int_distribution< std::uint32_t > remote_distribution{
            0, static_cast< std::uint32_t >( remote_cpus_.size() - 1) };
        do {
            do {
                ++count;
                // random selection of one logical cpu
                // that belongs to the local NUMA node
                cpu_id = local_cpus_[local_distribution( generator)];
                // prevent stealing from own scheduler
            } while ( cpu_id == cpu_id_);
            // steal context from other scheduler
            // schedulers_[cpu_id] should never contain a nullptr
            BOOST_ASSERT( nullptr != schedulers_[cpu_id]);
            victim = schedulers_[cpu_id]->steal();
        } while ( nullptr == victim && count < size);
        if ( nullptr != victim) {
            boost::context::detail::prefetch_range( victim, sizeof( context) );
            BOOST_ASSERT( ! victim->is_context( type::pinned_context) );
            context::active()->attach( victim);
        } else if ( ! remote_cpus_.empty() ) {
            cpu_id = 0;
            count = 0;
            size = remote_cpus_.size();
            do {
                ++count;
                // random selection of one logical cpu
                // that belongs to a remote NUMA node
                cpu_id = remote_cpus_[remote_distribution( generator)];
                // remote cpu ID should never be equal to local cpu ID
                BOOST_ASSERT( cpu_id != cpu_id_);
                // schedulers_[cpu_id] should never contain a nullptr
                BOOST_ASSERT( nullptr != schedulers_[cpu_id]);
                // steal context from other scheduler
                victim = schedulers_[cpu_id]->steal();
            } while ( nullptr == victim && count < size);
            if ( nullptr != victim) {
                boost::context::detail::prefetch_range( victim, sizeof( context) );
                BOOST_ASSERT( ! victim->is_context( type::pinned_context) );
                // move memory from remote NUMA-node to
                // memory of local NUMA-node
                context::active()->attach( victim);
            }
        }
    }
    return victim;
}

void
work_stealing::suspend_until( std::chrono::steady_clock::time_point const& time_point) noexcept {
    if ( suspend_) {
        if ( (std::chrono::steady_clock::time_point::max)() == time_point) {
            std::unique_lock< std::mutex > lk{ mtx_ };
            cnd_.wait( lk, [this](){ return flag_; });
            flag_ = false;
        } else {
            std::unique_lock< std::mutex > lk{ mtx_ };
            cnd_.wait_until( lk, time_point, [this](){ return flag_; });
            flag_ = false;
        }
    }
}

void
work_stealing::notify() noexcept {
    if ( suspend_) {
        std::unique_lock< std::mutex > lk{ mtx_ };
        flag_ = true;
        lk.unlock();
        cnd_.notify_all();
    }
}

}}}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif
