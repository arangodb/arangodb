
//          Copyright Oliver Kowalke 2015.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <vector>

#include <boost/fiber/all.hpp>

#include "barrier.hpp"
#include "bind/bind_processor.hpp"

using allocator_type = boost::fibers::fixedsize_stack;
using channel_type = boost::fibers::unbounded_channel< std::uint64_t >;
using clock_type = std::chrono::steady_clock;
using duration_type = clock_type::duration;
using lock_type = std::unique_lock< std::mutex >;
using time_point_type = clock_type::time_point;

static bool done = false;
static std::mutex mtx{};
static boost::fibers::condition_variable_any cnd{};

// microbenchmark
void skynet( allocator_type & salloc, channel_type & c, std::size_t num, std::size_t size, std::size_t div) {
    if ( 1 == size) {
        c.push( num);
    } else {
        channel_type rc;
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

void thread( unsigned int i, barrier * b) {
    bind_to_processor( i);
    boost::fibers::use_scheduling_algorithm< boost::fibers::algo::shared_work >();
    b->wait();
    lock_type lk( mtx);
    cnd.wait( lk, [](){ return done; });
    BOOST_ASSERT( done);
}

int main() {
    try {
        boost::fibers::use_scheduling_algorithm< boost::fibers::algo::shared_work >();
        unsigned int n = std::thread::hardware_concurrency();
        barrier b( n);
        bind_to_processor( n - 1);
        std::size_t stack_size{ 4048 };
        std::size_t size{ 100000 };
        std::size_t div{ 10 };
        std::vector< std::thread > threads;
        for ( unsigned int i = 1; i < n; ++i) {
            threads.push_back( std::thread( thread, i - 1, & b) );
        };
        allocator_type salloc{ stack_size };
        std::uint64_t result{ 0 };
        duration_type duration{ duration_type::zero() };
        channel_type rc;
        b.wait();
        time_point_type start{ clock_type::now() };
        skynet( salloc, rc, 0, size, div);
        result = rc.value_pop();
        duration = clock_type::now() - start;
        std::cout << "Result: " << result << " in " << duration.count() / 1000000 << " ms" << std::endl;
        lock_type lk( mtx);
        done = true;
        lk.unlock();
        cnd.notify_all();
        for ( std::thread & t : threads) {
            t.join();
        }
        std::cout << "done." << std::endl;
        return EXIT_SUCCESS;
    } catch ( std::exception const& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "unhandled exception" << std::endl;
    }
	return EXIT_FAILURE;
}
