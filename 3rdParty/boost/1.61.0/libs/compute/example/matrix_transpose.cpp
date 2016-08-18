//---------------------------------------------------------------------------//
// Copyright (c) 2014 Benoit Dequidt <benoit.dequidt@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <iostream>
#include <cstdlib>

#include <boost/program_options.hpp>

#include <boost/compute/core.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/type_traits/type_name.hpp>
#include <boost/compute/utility/source.hpp>

namespace compute = boost::compute;
namespace po = boost::program_options;

using compute::uint_;

const uint_ TILE_DIM = 32;
const uint_ BLOCK_ROWS = 8;

// generate a copy kernel program
compute::kernel make_copy_kernel(const compute::context& context)
{
    // source for the copy_kernel program
    const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        __kernel void copy_kernel(__global const float *src, __global float *dst)
        {
            uint x = get_group_id(0) * TILE_DIM + get_local_id(0);
            uint y = get_group_id(1) * TILE_DIM + get_local_id(1);

            uint width = get_num_groups(0) * TILE_DIM;

            for(uint i = 0 ; i < TILE_DIM ; i+= BLOCK_ROWS){
                dst[(y+i)*width +x] = src[(y+i)*width + x];
            }
        }
    );

    // setup compilation flags for the copy program
    std::stringstream options;
    options << "-DTILE_DIM=" << TILE_DIM << " -DBLOCK_ROWS=" << BLOCK_ROWS;

    // create and build the copy program
    compute::program program =
        compute::program::build_with_source(source, context, options.str());

    // create and return the copy kernel
    return program.create_kernel("copy_kernel");
}

// generate a naive transpose kernel
compute::kernel make_naive_transpose_kernel(const compute::context& context)
{
    // source for the naive_transpose kernel
    const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        __kernel void naive_transpose(__global const float *src, __global float *dst)
        {
            uint x = get_group_id(0) * TILE_DIM + get_local_id(0);
            uint y = get_group_id(1) * TILE_DIM + get_local_id(1);

            uint width = get_num_groups(0) * TILE_DIM;

            for(uint i = 0 ; i < TILE_DIM; i+= BLOCK_ROWS){
                dst[x*width + y+i] = src[(y+i)*width + x];
            }
        }
    );

    // setup compilation flags for the naive_transpose program
    std::stringstream options;
    options << "-DTILE_DIM=" << TILE_DIM << " -DBLOCK_ROWS=" << BLOCK_ROWS;

    // create and build the naive_transpose program
    compute::program program =
        compute::program::build_with_source(source, context, options.str());

    // create and return the naive_transpose kernel
    return program.create_kernel("naive_transpose");
}

// generates a coalesced transpose kernel
compute::kernel make_coalesced_transpose_kernel(const compute::context& context)
{
    // source for the coalesced_transpose kernel
    const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        __kernel void coalesced_transpose(__global const float *src, __global float *dst)
        {
            __local float tile[TILE_DIM][TILE_DIM];

            // compute indexes
            uint x = get_group_id(0) * TILE_DIM + get_local_id(0);
            uint y = get_group_id(1) * TILE_DIM + get_local_id(1);

            uint width = get_num_groups(0) * TILE_DIM;

            // load inside local memory
            for(uint i = 0 ; i < TILE_DIM; i+= BLOCK_ROWS){
                tile[get_local_id(1)+i][get_local_id(0)] = src[(y+i)*width + x];
            }

            barrier(CLK_LOCAL_MEM_FENCE);

            // transpose indexes
            x = get_group_id(1) * TILE_DIM + get_local_id(0);
            y = get_group_id(0) * TILE_DIM + get_local_id(1);

            // write output from local memory
            for(uint i = 0 ; i < TILE_DIM ; i+=BLOCK_ROWS){
                dst[(y+i)*width + x] = tile[get_local_id(0)][get_local_id(1)+i];
            }
        }
    );

    // setup compilation flags for the coalesced_transpose program
    std::stringstream options;
    options << "-DTILE_DIM=" << TILE_DIM << " -DBLOCK_ROWS=" << BLOCK_ROWS;

    // create and build the coalesced_transpose program
    compute::program program =
        compute::program::build_with_source(source, context, options.str());

    // create and return coalesced_transpose kernel
    return program.create_kernel("coalesced_transpose");
}

// generate a coalesced withtout bank conflicts kernel
compute::kernel make_coalesced_no_bank_conflicts_kernel(const compute::context& context)
{
    const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        __kernel void coalesced_no_bank_conflicts(__global const float *src, __global float *dst)
        {
            // TILE_DIM+1 is here to avoid bank conflicts in local memory
            __local float tile[TILE_DIM][TILE_DIM+1];

            // compute indexes
            uint x = get_group_id(0) * TILE_DIM + get_local_id(0);
            uint y = get_group_id(1) * TILE_DIM + get_local_id(1);

            uint width = get_num_groups(0) * TILE_DIM;

            // load inside local memory
            for(uint i = 0 ; i < TILE_DIM; i+= BLOCK_ROWS){
                tile[get_local_id(1)+i][get_local_id(0)] = src[(y+i)*width + x];
            }

            barrier(CLK_LOCAL_MEM_FENCE);

            // transpose indexes
            x = get_group_id(1) * TILE_DIM + get_local_id(0);
            y = get_group_id(0) * TILE_DIM + get_local_id(1);

            // write output from local memory
            for(uint i = 0 ; i < TILE_DIM ; i+=BLOCK_ROWS){
                dst[(y+i)*width + x] = tile[get_local_id(0)][get_local_id(1)+i];
            }
        }
    );

    // setup compilation flags for the coalesced_no_bank_conflicts program
    std::stringstream options;
    options << "-DTILE_DIM=" << TILE_DIM << " -DBLOCK_ROWS=" << BLOCK_ROWS;

    // create and build the coalesced_no_bank_conflicts program
    compute::program program =
        compute::program::build_with_source(source, context, options.str());

    // create and return the coalesced_no_bank_conflicts kernel
    return program.create_kernel("coalesced_no_bank_conflicts");
}

// compare 'expectedResult' to 'transposedMatrix'. prints an error message if not equal.
bool check_transposition(const std::vector<float>& expectedResult,
                         uint_ size,
                         const std::vector<float>& transposedMatrix)
{
    for(uint_ i = 0 ; i < size ; ++i){
        if(expectedResult[i] != transposedMatrix[i]){
            std::cout << "idx = " << i << " , expected " << expectedResult[i]
                      << " , got " << transposedMatrix[i] << std::endl;
            std::cout << "FAILED" << std::endl;
            return false;
        }
    }
    return true;
}

// generate a matrix inside 'in' and do the tranposition inside 'out'
void generate_matrix(std::vector<float>& in, std::vector<float>& out, uint_ rows, uint_ cols)
{
    // generate a matrix
    for(uint_ i = 0 ; i < rows ; ++i){
        for(uint_ j = 0 ; j < cols ; ++j){
            in[i*cols + j] = i*cols + j;
        }
    }

    // store transposed result
    for(uint_ j = 0; j < cols ; ++j){
        for(uint_ i = 0 ; i < rows ; ++i){
            out[j*rows + i] = in[i*cols + j];
        }
    }
}

// neccessary for 64-bit integer on win32
#ifdef _WIN32
#define uint64_t unsigned __int64
#endif

int main(int argc, char *argv[])
{
    // setup command line arguments
    po::options_description options("options");
    options.add_options()
        ("help", "show usage instructions")
        ("rows", po::value<uint_>()->default_value(4096), "number of matrix rows")
        ("cols", po::value<uint_>()->default_value(4096), "number of matrix columns")
    ;

    // parse command line
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, options), vm);
    po::notify(vm);

    // check command line arguments
    if(vm.count("help")){
        std::cout << options << std::endl;
        return 0;
    }

    // get number rows and columns for the matrix
    const uint_ rows = vm["rows"].as<uint_>();
    const uint_ cols = vm["cols"].as<uint_>();

    // get the default device
    compute::device device = compute::system::default_device();

    // print out device name and matrix information
    std::cout << "Device: " << device.name() << std::endl;
    std::cout << "Matrix Size: " << rows << "x" << cols << std::endl;
    std::cout << "Grid Size: " << rows/TILE_DIM << "x" << cols/TILE_DIM << " blocks" << std::endl;
    std::cout << "Local Size: " << TILE_DIM << "x" << BLOCK_ROWS << " threads" << std::endl;
    std::cout << std::endl;

    const size_t global_work_size[2] = {rows, cols*BLOCK_ROWS/TILE_DIM};
    const size_t local_work_size[2] = {TILE_DIM, BLOCK_ROWS};

    // setup input data on the host
    const uint_ size = rows * cols;
    std::vector<float> h_input(size);
    std::vector<float> h_output(size);
    std::vector<float> expectedResult(size);
    generate_matrix(h_input, expectedResult, rows, cols);

    // create a context for the device
    compute::context context(device);

    // device vectors
    compute::vector<float> d_input(size, context);
    compute::vector<float> d_output(size, context);

    // command_queue with profiling
    compute::command_queue queue(context, device, compute::command_queue::enable_profiling);

    // copy input data
    compute::copy(h_input.begin(), h_input.end(), d_input.begin(), queue);

    // simple copy kernel
    std::cout << "Testing copy_kernel:" << std::endl;
    compute::kernel kernel = make_copy_kernel(context);
    kernel.set_arg(0, d_input);
    kernel.set_arg(1, d_output);

    compute::event start;
    start = queue.enqueue_nd_range_kernel(kernel, 2, 0, global_work_size, local_work_size);
    queue.finish();
    uint64_t elapsed = start.duration<boost::chrono::nanoseconds>().count();

    std::cout << "  Elapsed: " << elapsed << " ns" << std::endl;
    std::cout << "  BandWidth: " << 2*rows*cols*sizeof(float) / elapsed << " GB/s" << std::endl;
    compute::copy(d_output.begin(), d_output.end(), h_output.begin(), queue);

    check_transposition(h_input, rows*cols, h_output);
    std::cout << std::endl;

    // naive_transpose kernel
    std::cout << "Testing naive_transpose:" << std::endl;
    kernel = make_naive_transpose_kernel(context);
    kernel.set_arg(0, d_input);
    kernel.set_arg(1, d_output);

    start = queue.enqueue_nd_range_kernel(kernel, 2, 0, global_work_size, local_work_size);
    queue.finish();
    elapsed = start.duration<boost::chrono::nanoseconds>().count();
    std::cout << "  Elapsed: " << elapsed << " ns" << std::endl;
    std::cout << "  BandWidth: " << 2*rows*cols*sizeof(float) / elapsed << " GB/s" << std::endl;
    compute::copy(d_output.begin(), d_output.end(), h_output.begin(), queue);

    check_transposition(expectedResult, rows*cols, h_output);
    std::cout << std::endl;

    // coalesced_transpose kernel
    std::cout << "Testing coalesced_transpose:" << std::endl;
    kernel = make_coalesced_transpose_kernel(context);
    kernel.set_arg(0, d_input);
    kernel.set_arg(1, d_output);

    start = queue.enqueue_nd_range_kernel(kernel, 2, 0, global_work_size, local_work_size);
    queue.finish();
    elapsed = start.duration<boost::chrono::nanoseconds>().count();
    std::cout << "  Elapsed: " << elapsed << " ns" << std::endl;
    std::cout << "  BandWidth: " << 2*rows*cols*sizeof(float) / elapsed << " GB/s" << std::endl;

    compute::copy(d_output.begin(), d_output.end(), h_output.begin(), queue);

    check_transposition(expectedResult, rows*cols, h_output);
    std::cout << std::endl;

    // coalesced_no_bank_conflicts kernel
    std::cout << "Testing coalesced_no_bank_conflicts:" << std::endl;

    kernel = make_coalesced_no_bank_conflicts_kernel(context);
    kernel.set_arg(0, d_input);
    kernel.set_arg(1, d_output);

    start = queue.enqueue_nd_range_kernel(kernel, 2, 0, global_work_size, local_work_size);
    queue.finish();
    elapsed = start.duration<boost::chrono::nanoseconds>().count();
    std::cout << "  Elapsed: " << elapsed << " ns" << std::endl;
    std::cout << "  BandWidth: " << 2*rows*cols*sizeof(float) / elapsed << " GB/s" << std::endl;

    compute::copy(d_output.begin(), d_output.end(), h_output.begin(), queue);

    check_transposition(expectedResult, rows*cols, h_output);
    std::cout << std::endl;

    return 0;
}
