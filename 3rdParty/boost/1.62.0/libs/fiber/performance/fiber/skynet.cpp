#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <numeric>
#include <vector>

#include <boost/fiber/all.hpp>
#include <boost/pool/pool.hpp>
#include <boost/pool/pool_alloc.hpp>

#include "bind/bind_processor.hpp"

using clock_type = std::chrono::steady_clock;
using duration_type = clock_type::duration;
using time_point_type = clock_type::time_point;
using channel_type = boost::fibers::unbounded_channel< std::uint64_t >;
using allocator_type = boost::fibers::pooled_fixedsize_stack;

void prepare( allocator_type & salloc) {
    auto sctx = salloc.allocate();
    salloc.deallocate( sctx);
};

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

int main() {
    try {
        bind_to_processor( 0);
        std::size_t stack_size{ 4048 };
        std::size_t size{ 100000 };
        std::size_t div{ 10 };
        allocator_type salloc{ stack_size, size };
        prepare( salloc);
        std::uint64_t result{ 0 };
        duration_type duration{ duration_type::zero() };
        channel_type rc;
        time_point_type start{ clock_type::now() };
        skynet( salloc, rc, 0, size, div);
        result = rc.value_pop();
        duration = clock_type::now() - start;
        std::cout << "Result: " << result << " in " << duration.count() / 1000000 << " ms" << std::endl;
        std::cout << "done." << std::endl;
        return EXIT_SUCCESS;
    } catch ( std::exception const& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "unhandled exception" << std::endl;
    }
	return EXIT_FAILURE;
}
