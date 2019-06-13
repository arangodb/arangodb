#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <numeric>
#include <thread>
#include <vector>

#include "buffered_channel.hpp"

using channel_type = buffered_channel< std::uint64_t >;
using clock_type = std::chrono::steady_clock;
using duration_type = clock_type::duration;
using time_point_type = clock_type::time_point;

// microbenchmark
void skynet( channel_type & c, std::size_t num, std::size_t size, std::size_t div) {
    if ( 1 == size) {
        c.push( num);
    } else {
        channel_type rc{ 16 };
        for ( std::size_t i = 0; i < div; ++i) {
            auto sub_num = num + i * size / div;
            std::thread{ skynet, std::ref( rc), sub_num, size / div, div }.detach();
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
        std::size_t size{ 10000 };
        std::size_t div{ 10 };
        std::uint64_t result{ 0 };
        duration_type duration{ duration_type::zero() };
        channel_type rc{ 2 };
        time_point_type start{ clock_type::now() };
        skynet( rc, 0, size, div);
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
