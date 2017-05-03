//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestProgram
#include <boost/test/unit_test.hpp>

// disable the automatic kernel compilation debug messages. this allows the
// test for program to check that compilation error exceptions are properly
// thrown when invalid kernel code is passed to program::build().
#undef BOOST_COMPUTE_DEBUG_KERNEL_COMPILATION

#include <boost/compute/kernel.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/program.hpp>
#include <boost/compute/utility/source.hpp>

#include "quirks.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

const char source[] =
    "__kernel void foo(__global float *x, const uint n) { }\n"
    "__kernel void bar(__global int *x, __global int *y) { }\n";


BOOST_AUTO_TEST_CASE(get_program_info)
{
    // create program
    boost::compute::program program =
        boost::compute::program::create_with_source(source, context);

    // build program
    program.build();

    // check program info
#ifndef BOOST_COMPUTE_USE_OFFLINE_CACHE
    BOOST_CHECK(program.source().empty() == false);
#endif
    BOOST_CHECK(program.get_context() == context);
}

BOOST_AUTO_TEST_CASE(program_source)
{
    // create program from source
    boost::compute::program program =
        boost::compute::program::create_with_source(source, context);

    BOOST_CHECK_EQUAL(std::string(source), program.source());
}

BOOST_AUTO_TEST_CASE(program_multiple_sources)
{
    std::vector<std::string> sources;
    sources.push_back("__kernel void foo(__global int* x) { }\n");
    sources.push_back("__kernel void bar(__global float* y) { }\n");

    // create program from sources
    boost::compute::program program =
        boost::compute::program::create_with_source(sources, context);
    program.build();

    boost::compute::kernel foo = program.create_kernel("foo");
    boost::compute::kernel bar = program.create_kernel("bar");
}

BOOST_AUTO_TEST_CASE(program_source_no_file)
{
    // create program from a non-existant source file
    // and verifies it throws.
    BOOST_CHECK_THROW(boost::compute::program program =
                      boost::compute::program::create_with_source_file
                      (std::string(), context),
                      std::ios_base::failure);
}

BOOST_AUTO_TEST_CASE(create_kernel)
{
    boost::compute::program program =
        boost::compute::program::create_with_source(source, context);
    program.build();

    boost::compute::kernel foo = program.create_kernel("foo");
    boost::compute::kernel bar = program.create_kernel("bar");

    // try to create a kernel that doesn't exist
    BOOST_CHECK_THROW(program.create_kernel("baz"), boost::compute::opencl_error);
}

BOOST_AUTO_TEST_CASE(create_with_binary)
{
    // create program from source
    boost::compute::program source_program =
        boost::compute::program::create_with_source(source, context);
    source_program.build();

    // create kernels in source program
    boost::compute::kernel source_foo_kernel = source_program.create_kernel("foo");
    boost::compute::kernel source_bar_kernel = source_program.create_kernel("bar");

    // check source kernels
    BOOST_CHECK_EQUAL(source_foo_kernel.name(), std::string("foo"));
    BOOST_CHECK_EQUAL(source_bar_kernel.name(), std::string("bar"));

    // get binary
    std::vector<unsigned char> binary = source_program.binary();

    // create program from binary
    boost::compute::program binary_program =
        boost::compute::program::create_with_binary(binary, context);
    binary_program.build();

    // create kernels in binary program
    boost::compute::kernel binary_foo_kernel = binary_program.create_kernel("foo");
    boost::compute::kernel binary_bar_kernel = binary_program.create_kernel("bar");

    // check binary kernels
    BOOST_CHECK_EQUAL(binary_foo_kernel.name(), std::string("foo"));
    BOOST_CHECK_EQUAL(binary_bar_kernel.name(), std::string("bar"));
}

BOOST_AUTO_TEST_CASE(create_with_source_doctest)
{
//! [create_with_source]
std::string source = "__kernel void foo(__global int *data) { }";

boost::compute::program foo_program =
    boost::compute::program::create_with_source(source, context);
//! [create_with_source]

    foo_program.build();
}

#ifdef CL_VERSION_1_2
BOOST_AUTO_TEST_CASE(compile_and_link)
{
    REQUIRES_OPENCL_VERSION(1,2);

    if(!supports_compile_program(device) || !supports_link_program(device)) {
        return;
    }

    // create the library program
    const char library_source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        // for some reason the apple opencl compilers complains if a prototype
        // for the square() function is not available, so we add it here
        T square(T);

        // generic square function definition
        T square(T x) { return x * x; }
    );

    compute::program library_program =
        compute::program::create_with_source(library_source, context);

    library_program.compile("-DT=int");

    // create the kernel program
    const char kernel_source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        // forward declare square function
        extern int square(int);

        // square kernel definition
        __kernel void square_kernel(__global int *x)
        {
            x[0] = square(x[0]);
        }
    );

    compute::program square_program =
        compute::program::create_with_source(kernel_source, context);

    square_program.compile();

    // link the programs
    std::vector<compute::program> programs;
    programs.push_back(library_program);
    programs.push_back(square_program);

    compute::program linked_program =
        compute::program::link(programs, context);

    // create the square kernel
    compute::kernel square_kernel =
        linked_program.create_kernel("square_kernel");
    BOOST_CHECK_EQUAL(square_kernel.name(), "square_kernel");
}
#endif // CL_VERSION_1_2

BOOST_AUTO_TEST_CASE(build_log)
{
    const char invalid_source[] =
        "__kernel void foo(__global int *input) { !@#$%^&*() }";

    compute::program invalid_program =
        compute::program::create_with_source(invalid_source, context);

    try {
        invalid_program.build();

        // should not get here
        BOOST_CHECK(false);
    }
    catch(compute::opencl_error&){
        std::string log = invalid_program.build_log();
        BOOST_CHECK(!log.empty());
    }
}

BOOST_AUTO_TEST_SUITE_END()
