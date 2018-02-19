
//          Copyright Oliver Kowalke 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// based on https://github.com/atemerev/skynet from Alexander Temerev 

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <queue>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <random>
#include <sstream>
#include <vector>

#include <boost/fiber/all.hpp>
#include <boost/fiber/numa/topology.hpp>
#include <boost/predef.h>

#include "../barrier.hpp"

using clock_type = std::chrono::steady_clock;
using duration_type = clock_type::duration;
using time_point_type = clock_type::time_point;
using channel_type = boost::fibers::buffered_channel< std::uint64_t >;
using allocator_type = boost::fibers::fixedsize_stack;
using lock_type = std::unique_lock< std::mutex >;

static bool done = false;
static std::mutex mtx{};
static boost::fibers::condition_variable_any cnd{};

std::uint32_t hardware_concurrency( std::vector< boost::fibers::numa::node > const& topo) {
    std::uint32_t cpus = 0;
    for ( auto & node : topo) {
        cpus += node.logical_cpus.size();
    }
    return cpus;
}

// microbenchmark
void skynet( allocator_type & salloc, channel_type & c, std::size_t num, std::size_t size, std::size_t div) {
    if ( 1 == size) {
        c.push( num);
    } else {
        channel_type rc{ 16 };
        for ( std::size_t i = 0; i < div; ++i) {
            auto sub_num = num + i * size / div;
            boost::fibers::fiber{ boost::fibers::launch::dispatch,
                              std::allocator_arg, salloc,
                              skynet,
                              std::ref( salloc), std::ref( rc), sub_num, size / div, div }.detach();
        }
        std::uint64_t sum{ 0 };
        for ( std::size_t i = 0; i < div; ++i) {
            sum += rc.value_pop();
        }
        c.push( sum);
    }
}

void thread( std::uint32_t cpu_id, std::uint32_t node_id, std::vector< boost::fibers::numa::node > const& topo, barrier * b) {
    boost::fibers::use_scheduling_algorithm< boost::fibers::algo::numa::work_stealing >( cpu_id, node_id, topo);
    b->wait();
    lock_type lk( mtx);
    cnd.wait( lk, [](){ return done; });
    BOOST_ASSERT( done);
}

int main() {
    try {
        std::vector< boost::fibers::numa::node > topo = boost::fibers::numa::topology();
        auto node = topo[0];
        auto main_cpu_id = * node.logical_cpus.begin();
        boost::fibers::use_scheduling_algorithm< boost::fibers::algo::numa::work_stealing >( main_cpu_id, node.id, topo);
        barrier b{ hardware_concurrency( topo) };
        std::size_t size{ 1000000 };
        std::size_t div{ 10 };
        // Windows 10 and FreeBSD require a fiber stack of 8kb
        // otherwise the stack gets exhausted
        // stack requirements must be checked for other OS too
#if BOOST_OS_WINDOWS || BOOST_OS_BSD
        allocator_type salloc{ 2*allocator_type::traits_type::page_size() };
#else
        allocator_type salloc{ allocator_type::traits_type::page_size() };
#endif
        std::uint64_t result{ 0 };
        channel_type rc{ 2 };
        std::vector< std::thread > threads;
        for ( auto & node : topo) {
            for ( std::uint32_t cpu_id : node.logical_cpus) {
                // exclude main-thread
                if ( main_cpu_id != cpu_id) {
                    threads.emplace_back( thread, cpu_id, node.id, std::cref( topo), & b);
                }
            }
        }
        b.wait();
        time_point_type start{ clock_type::now() };
        skynet( salloc, rc, 0, size, div);
        result = rc.value_pop();
        if ( 499999500000 != result) {
            throw std::runtime_error("invalid result");
        }
        auto duration = clock_type::now() - start;
        lock_type lk( mtx);
        done = true;
        lk.unlock();
        cnd.notify_all();
        for ( std::thread & t : threads) {
            t.join();
        }
        std::cout << "duration: " << duration.count() / 1000000 << " ms" << std::endl;
        return EXIT_SUCCESS;
    } catch ( std::exception const& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "unhandled exception" << std::endl;
    }
	return EXIT_FAILURE;
}
