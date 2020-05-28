
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)


#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <tuple>

#include <cuda.h>

#include <boost/assert.hpp>
#include <boost/bind.hpp>
#include <boost/intrusive_ptr.hpp>

#include <boost/fiber/all.hpp>
#include <boost/fiber/cuda/waitfor.hpp>

__global__
void vector_add( int * a, int * b, int * c, int size) {
    int idx = threadIdx.x + blockIdx.x * blockDim.x;
    if ( idx < size) {
        c[idx] = a[idx] + b[idx];
    }
}

int main() {
    try {
        bool done = false;
        boost::fibers::fiber f1( [&done]{
                std::cout << "f1: entered" << std::endl;
                try {
                    cudaStream_t stream0, stream1;
                    cudaStreamCreate( & stream0);
                    cudaStreamCreate( & stream1);
                    int size = 1024 * 1024;
                    int full_size = 20 * size;
                    int * host_a, * host_b, * host_c;
                    cudaHostAlloc( & host_a, full_size * sizeof( int), cudaHostAllocDefault);
                    cudaHostAlloc( & host_b, full_size * sizeof( int), cudaHostAllocDefault);
                    cudaHostAlloc( & host_c, full_size * sizeof( int), cudaHostAllocDefault);
                    int * dev_a0, * dev_b0, * dev_c0;
                    int * dev_a1, * dev_b1, * dev_c1;
                    cudaMalloc( & dev_a0, size * sizeof( int) );
                    cudaMalloc( & dev_b0, size * sizeof( int) );
                    cudaMalloc( & dev_c0, size * sizeof( int) );
                    cudaMalloc( & dev_a1, size * sizeof( int) );
                    cudaMalloc( & dev_b1, size * sizeof( int) );
                    cudaMalloc( & dev_c1, size * sizeof( int) );
                    std::minstd_rand generator;
                    std::uniform_int_distribution<> distribution(1, 6);
                    for ( int i = 0; i < full_size; ++i) {
                        host_a[i] = distribution( generator);
                        host_b[i] = distribution( generator);
                    }
                    for ( int i = 0; i < full_size; i += 2 * size) {
                        cudaMemcpyAsync( dev_a0, host_a + i, size * sizeof( int), cudaMemcpyHostToDevice, stream0);
                        cudaMemcpyAsync( dev_a1, host_a + i + size, size * sizeof( int), cudaMemcpyHostToDevice, stream1);
                        cudaMemcpyAsync( dev_b0, host_b + i, size * sizeof( int), cudaMemcpyHostToDevice, stream0);
                        cudaMemcpyAsync( dev_b1, host_b + i + size, size * sizeof( int), cudaMemcpyHostToDevice, stream1);
                        vector_add<<< size / 256, 256, 0, stream0 >>>( dev_a0, dev_b0, dev_c0, size);
                        vector_add<<< size / 256, 256, 0, stream1 >>>( dev_a1, dev_b1, dev_c1, size);
                        cudaMemcpyAsync( host_c + i, dev_c0, size * sizeof( int), cudaMemcpyDeviceToHost, stream0);
                        cudaMemcpyAsync( host_c + i + size, dev_c1, size * sizeof( int), cudaMemcpyDeviceToHost, stream1);
                    }
                    auto results = boost::fibers::cuda::waitfor_all( stream0, stream1);
                    for ( auto & result : results) {
                        BOOST_ASSERT( stream0 == std::get< 0 >( result) || stream1 == std::get< 0 >( result) );
                        BOOST_ASSERT( cudaSuccess == std::get< 1 >( result) );
                    }
                    std::cout << "f1: GPU computation finished" << std::endl;
                    cudaFreeHost( host_a);
                    cudaFreeHost( host_b);
                    cudaFreeHost( host_c);
                    cudaFree( dev_a0);
                    cudaFree( dev_b0);
                    cudaFree( dev_c0);
                    cudaFree( dev_a1);
                    cudaFree( dev_b1);
                    cudaFree( dev_c1);
                    cudaStreamDestroy( stream0);
                    cudaStreamDestroy( stream1);
                    done = true;
                } catch ( std::exception const& ex) {
                    std::cerr << "exception: " << ex.what() << std::endl;
                }
                std::cout << "f1: leaving" << std::endl;
        });
        boost::fibers::fiber f2([&done]{
            std::cout << "f2: entered" << std::endl;
            while ( ! done) {
                std::cout << "f2: sleeping" << std::endl;
                boost::this_fiber::sleep_for( std::chrono::milliseconds( 1 ) );
            }
            std::cout << "f2: leaving" << std::endl;
        });
        f1.join();
        f2.join();
        std::cout << "done." << std::endl;
        return EXIT_SUCCESS;
    } catch ( std::exception const& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "unhandled exception" << std::endl;
    }
	return EXIT_FAILURE;
}
