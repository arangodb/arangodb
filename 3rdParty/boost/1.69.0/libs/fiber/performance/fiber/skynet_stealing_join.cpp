
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
#include <boost/predef.h>

#include "barrier.hpp"

using clock_type = std::chrono::steady_clock;
using duration_type = clock_type::duration;
using time_point_type = clock_type::time_point;
using channel_type = boost::fibers::buffered_channel< std::uint64_t >;
using allocator_type = boost::fibers::fixedsize_stack;
using lock_type = std::unique_lock< std::mutex >;

static bool done = false;
static std::mutex mtx{};
static boost::fibers::condition_variable_any cnd{};

// microbenchmark
void skynet( allocator_type & salloc, channel_type & c, std::size_t num, std::size_t size, std::size_t div) {
    if ( 1 == size) {
        c.push( num);
    } else {
        channel_type rc{ 16 };
        std::vector< boost::fibers::fiber > fibers;
        for ( std::size_t i = 0; i < div; ++i) {
            auto sub_num = num + i * size / div;
            fibers.emplace_back( boost::fibers::launch::dispatch,
                                 std::allocator_arg, salloc,
                                 skynet,
                                 std::ref( salloc), std::ref( rc), sub_num, size / div, div);
        }
        std::uint64_t sum{ 0 };
        for ( std::size_t i = 0; i < div; ++i) {
            sum += rc.value_pop();
        }
        c.push( sum);
        for ( auto & f : fibers) {
            f.join();
        }
    }
}

void thread( std::uint32_t thread_count) {
    // thread registers itself at work-stealing scheduler
    boost::fibers::use_scheduling_algorithm< boost::fibers::algo::work_stealing >( thread_count);
    lock_type lk( mtx);
    cnd.wait( lk, [](){ return done; });
    BOOST_ASSERT( done);
}

int main() {
    try {
        // count of logical cpus
        std::uint32_t thread_count = std::thread::hardware_concurrency();
        std::size_t size{ 1000000 };
        std::size_t div{ 10 };
        allocator_type salloc{ 2*allocator_type::traits_type::page_size() };
        std::uint64_t result{ 0 };
        channel_type rc{ 2 };
        std::vector< std::thread > threads;
        for ( std::uint32_t i = 1 /* count main-thread */; i < thread_count; ++i) {
            // spawn thread
            threads.emplace_back( thread, thread_count);
        }
        // main-thread registers itself at work-stealing scheduler
        boost::fibers::use_scheduling_algorithm< boost::fibers::algo::work_stealing >( thread_count);
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
