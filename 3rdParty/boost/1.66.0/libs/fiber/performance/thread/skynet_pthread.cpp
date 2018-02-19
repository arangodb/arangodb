#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <vector>

extern "C" {
#include <pthread.h>
#include <signal.h>
}

#include "buffered_channel.hpp"

using channel_type = buffered_channel< std::uint64_t >;
using clock_type = std::chrono::steady_clock;
using duration_type = clock_type::duration;
using time_point_type = clock_type::time_point;

struct thread_args {
    channel_type    &   c;
    std::size_t         num;
    std::size_t         size;
    std::size_t         div;
};

// microbenchmark
void * skynet( void * vargs) {
    thread_args * args = static_cast< thread_args * >( vargs);
    if ( 1 == args->size) {
        args->c.push( args->num);
    } else {
        channel_type rc{ 16 };
        for ( std::size_t i = 0; i < args->div; ++i) {
            auto sub_num = args->num + i * args->size / args->div;
            auto sub_size = args->size / args->div;
            auto size = 8 * 1024;
            pthread_attr_t tattr;
            if ( 0 != ::pthread_attr_init( & tattr) ) {
                std::runtime_error("pthread_attr_init() failed");
            }
            if ( 0 != ::pthread_attr_setstacksize( & tattr, size) ) {
                std::runtime_error("pthread_attr_setstacksize() failed");
            }
            thread_args * targs = new thread_args{ rc, sub_num, sub_size, args->div }; 
            pthread_t tid; 
            if ( 0 != ::pthread_create( & tid, & tattr, & skynet, targs) ) {
                std::runtime_error("pthread_create() failed");
            }
            if ( 0 != ::pthread_detach( tid) ) {
                std::runtime_error("pthread_detach() failed");
            }
        }
        std::uint64_t sum{ 0 };
        for ( std::size_t i = 0; i < args->div; ++i) {
            sum += rc.value_pop();
        }
        args->c.push( sum);
    }
    delete args;
    return nullptr;
}

int main() {
    try {
        std::size_t size{ 10000 };
        std::size_t div{ 10 };
        std::uint64_t result{ 0 };
        duration_type duration{ duration_type::zero() };
        channel_type rc{ 2 };
        thread_args * targs = new thread_args{ rc, 0, size, div }; 
        time_point_type start{ clock_type::now() };
        skynet( targs);
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
