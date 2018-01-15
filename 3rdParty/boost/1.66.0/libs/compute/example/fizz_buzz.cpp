//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#include <iostream>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/accumulate.hpp>
#include <boost/compute/algorithm/exclusive_scan.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/utility/dim.hpp>
#include <boost/compute/utility/source.hpp>

namespace compute = boost::compute;

const char fizz_buzz_source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
    // returns the length of the string for the number 'n'. This is used
    // during the first pass when we calculate the amount of space needed
    // for each string in the fizz-buzz sequence.
    inline uint fizz_buzz_string_length(uint n)
    {
        if((n % 5 == 0) && (n % 3 == 0)){
            return sizeof("fizzbuzz");
        }
        else if(n % 5 == 0){
            return sizeof("fizz");
        }
        else if(n % 3 == 0){
            return sizeof("buzz");
        }
        else {
            uint digits = 0;
            while(n){
                n /= 10;
                digits++;
            }
            return digits + 1;
        }
    }

    // first-pass kernel which calculates the string length for each number
    // and writes it to the string_lengths array. these will then be passed
    // to exclusive_scan() to calculate the output offsets for each string.
    __kernel void fizz_buzz_allocate_strings(__global uint *string_lengths)
    {
        const uint i = get_global_id(0);
        const uint n = i + 1;

        string_lengths[i] = fizz_buzz_string_length(n);
    }

    // copy the string 's' with length 'n' to 'result' (just like strncpy())
    inline void copy_string(__constant const char *s, uint n, __global char *result)
    {
        while(n--){
            result[n] = s[n];
        }
    }

    // reverse the string [start, end).
    inline void reverse_string(__global char *start, __global char *end)
    {
        while(start < end){
          char tmp = *end;
          *end = *start;
          *start = tmp;
          start++;
          end--;
        }
    }

    // second-pass kernel which copies the fizz-buzz string for each number to
    // buffer using the previously calculated offsets.
    __kernel void fizz_buzz_copy_strings(__global const uint *offsets, __global char *buffer)
    {
        const uint i = get_global_id(0);
        const uint n = i + 1;
        const uint offset = offsets[i];

        if((n % 5 == 0) && (n % 3 == 0)){
            copy_string("fizzbuzz\n", 9, buffer + offset);
        }
        else if(n % 5 == 0){
            copy_string("fizz\n", 5, buffer + offset);
        }
        else if(n % 3 == 0){
            copy_string("buzz\n", 5, buffer + offset);
        }
        else {
            // convert number to string and write it to the output
            __global char *number = buffer + offset;
            uint n_ = n;
            while(n_){
                *number++ = (n_%10) + '0';
                n_ /= 10;
            }
            reverse_string(buffer + offset, number - 1);
            *number = '\n';
        }
    }
);

int main()
{
    using compute::dim;
    using compute::uint_;

    // fizz-buzz up to 100
    size_t n = 100;

    // get the default device
    compute::device device = compute::system::default_device();
    compute::context ctx(device);
    compute::command_queue queue(ctx, device);

    // compile the fizz-buzz program
    compute::program fizz_buzz_program =
        compute::program::create_with_source(fizz_buzz_source, ctx);
    fizz_buzz_program.build();

    // create a vector for the output string and computing offsets
    compute::vector<char> output(ctx);
    compute::vector<uint_> offsets(n, ctx);

    // run the allocate kernel to calculate string lengths
    compute::kernel allocate_kernel(fizz_buzz_program, "fizz_buzz_allocate_strings");
    allocate_kernel.set_arg(0, offsets);
    queue.enqueue_nd_range_kernel(allocate_kernel, dim(0), dim(n), dim(1));

    // allocate space for the output string
    output.resize(
        compute::accumulate(offsets.begin(), offsets.end(), 0, queue)
    );

    // scan string lengths for each number to calculate the output offsets
    compute::exclusive_scan(
        offsets.begin(), offsets.end(), offsets.begin(), queue
    );

    // run the copy kernel to fill the output buffer
    compute::kernel copy_kernel(fizz_buzz_program, "fizz_buzz_copy_strings");
    copy_kernel.set_arg(0, offsets);
    copy_kernel.set_arg(1, output);
    queue.enqueue_nd_range_kernel(copy_kernel, dim(0), dim(n), dim(1));

    // copy the string to the host and print it to stdout
    std::string str;
    str.resize(output.size());
    compute::copy(output.begin(), output.end(), str.begin(), queue);
    std::cout << str;

    return 0;
}
