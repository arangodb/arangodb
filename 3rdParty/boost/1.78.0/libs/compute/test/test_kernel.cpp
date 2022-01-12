//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestKernel
#include <boost/test/unit_test.hpp>

#include <boost/compute/buffer.hpp>
#include <boost/compute/kernel.hpp>
#include <boost/compute/types.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/utility/source.hpp>

#include "context_setup.hpp"
#include "check_macros.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(name)
{
    compute::kernel foo = compute::kernel::create_with_source(
        "__kernel void foo(int x) { }", "foo", context
    );
    BOOST_CHECK_EQUAL(foo.name(), "foo");

    compute::kernel bar = compute::kernel::create_with_source(
        "__kernel void bar(float x) { }", "bar", context
    );
    BOOST_CHECK_EQUAL(bar.name(), "bar");
}

BOOST_AUTO_TEST_CASE(arity)
{
    compute::kernel foo = compute::kernel::create_with_source(
        "__kernel void foo(int x) { }", "foo", context
    );
    BOOST_CHECK_EQUAL(foo.arity(), size_t(1));

    compute::kernel bar = compute::kernel::create_with_source(
        "__kernel void bar(float x, float y) { }", "bar", context
    );
    BOOST_CHECK_EQUAL(bar.arity(), size_t(2));

    compute::kernel baz = compute::kernel::create_with_source(
        "__kernel void baz(char x, char y, char z) { }", "baz", context
    );
    BOOST_CHECK_EQUAL(baz.arity(), size_t(3));
}

BOOST_AUTO_TEST_CASE(set_buffer_arg)
{
    const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        __kernel void foo(__global int *x, __global int *y)
        {
            x[get_global_id(0)] = -y[get_global_id(0)];
        }
    );

    compute::kernel foo =
        compute::kernel::create_with_source(source, "foo", context);

    compute::buffer x(context, 16);
    compute::buffer y(context, 16);

    foo.set_arg(0, x);
    foo.set_arg(1, y.get());
}

BOOST_AUTO_TEST_CASE(get_work_group_info)
{
    const char source[] =
    "__kernel void sum(__global const float *input,\n"
    "                  __global float *output)\n"
    "{\n"
    "    __local float scratch[16];\n"
    "    const uint gid = get_global_id(0);\n"
    "    const uint lid = get_local_id(0);\n"
    "    if(lid < 16)\n"
    "        scratch[lid] = input[gid];\n"
    "}\n";

    compute::program program =
        compute::program::create_with_source(source, context);

    program.build();

    compute::kernel kernel = program.create_kernel("sum");

    using compute::ulong_;

    // get local memory size
    kernel.get_work_group_info<ulong_>(device, CL_KERNEL_LOCAL_MEM_SIZE);

    // check work group size
    size_t work_group_size =
        kernel.get_work_group_info<size_t>(device, CL_KERNEL_WORK_GROUP_SIZE);
    BOOST_CHECK(work_group_size >= 1);
}

#ifndef BOOST_COMPUTE_NO_VARIADIC_TEMPLATES
BOOST_AUTO_TEST_CASE(kernel_set_args)
{
    compute::kernel k = compute::kernel::create_with_source(
        "__kernel void test(int x, float y, char z) { }", "test", context
    );

    k.set_args(4, 2.4f, 'a');
}
#endif // BOOST_COMPUTE_NO_VARIADIC_TEMPLATES

// Originally failed to compile on macOS (several types are resolved differently)
BOOST_AUTO_TEST_CASE(kernel_set_args_mac)
{
    compute::kernel k = compute::kernel::create_with_source(
        "__kernel void test(unsigned int a, unsigned long b) { }", "test", context
    );

    compute::uint_  a;
    compute::ulong_ b;

    k.set_arg(0, a);
    k.set_arg(1, b);
}


#ifdef BOOST_COMPUTE_CL_VERSION_1_2
BOOST_AUTO_TEST_CASE(get_arg_info)
{
    REQUIRES_OPENCL_VERSION(1, 2);

    const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        __kernel void sum_kernel(__global const int *input,
                                 const uint size,
                                 __global int *result)
        {
            int sum = 0;
            for(uint i = 0; i < size; i++){
                sum += input[i];
            }
            *result = sum;
        }
    );

    compute::program program =
        compute::program::create_with_source(source, context);

    program.build("-cl-kernel-arg-info");

    compute::kernel kernel = program.create_kernel("sum_kernel");

    BOOST_CHECK_EQUAL(kernel.get_info<CL_KERNEL_NUM_ARGS>(), compute::uint_(3));

    BOOST_CHECK_EQUAL(kernel.get_arg_info<std::string>(0, CL_KERNEL_ARG_TYPE_NAME), "int*");
    BOOST_CHECK_EQUAL(kernel.get_arg_info<std::string>(0, CL_KERNEL_ARG_NAME), "input");
    BOOST_CHECK_EQUAL(kernel.get_arg_info<std::string>(1, CL_KERNEL_ARG_TYPE_NAME), "uint");
    BOOST_CHECK_EQUAL(kernel.get_arg_info<std::string>(1, CL_KERNEL_ARG_NAME), "size");
    BOOST_CHECK_EQUAL(kernel.get_arg_info<std::string>(2, CL_KERNEL_ARG_TYPE_NAME), "int*");
    BOOST_CHECK_EQUAL(kernel.get_arg_info<std::string>(2, CL_KERNEL_ARG_NAME), "result");

    BOOST_CHECK_EQUAL(kernel.get_arg_info<CL_KERNEL_ARG_TYPE_NAME>(0), "int*");
    BOOST_CHECK_EQUAL(kernel.get_arg_info<CL_KERNEL_ARG_NAME>(0), "input");
    BOOST_CHECK_EQUAL(kernel.get_arg_info<CL_KERNEL_ARG_TYPE_NAME>(1), "uint");
    BOOST_CHECK_EQUAL(kernel.get_arg_info<CL_KERNEL_ARG_NAME>(1), "size");
    BOOST_CHECK_EQUAL(kernel.get_arg_info<CL_KERNEL_ARG_TYPE_NAME>(2), "int*");
    BOOST_CHECK_EQUAL(kernel.get_arg_info<CL_KERNEL_ARG_NAME>(2), "result");
}
#endif // BOOST_COMPUTE_CL_VERSION_1_2

#ifdef BOOST_COMPUTE_CL_VERSION_2_0
#ifndef CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE_KHR
    #define CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE_KHR CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE
#endif
#ifndef CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE_KHR
    #define CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE_KHR CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE
#endif
BOOST_AUTO_TEST_CASE(get_sub_group_info_ext)
{
    compute::kernel k = compute::kernel::create_with_source(
        "__kernel void test(float x) { }", "test", context
    );

    // get_sub_group_info(const device&, cl_kernel_sub_group_info, const std::vector<size_t>)
    std::vector<size_t> local_work_size(2, size_t(64));
    boost::optional<size_t> count = k.get_sub_group_info<size_t>(
        device,
        CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE_KHR,
        local_work_size
    );

    #ifdef BOOST_COMPUTE_CL_VERSION_2_1
    if(device.check_version(2, 1))
    {
        BOOST_CHECK(count);
    }
    else
    #endif // BOOST_COMPUTE_CL_VERSION_2_1
    if(device.check_version(2, 0) && device.supports_extension("cl_khr_subgroups"))
    {
        // for device with cl_khr_subgroups it should return some value
        BOOST_CHECK(count);
    }
    else
    {
        // for device without cl_khr_subgroups ext it should return null optional
        BOOST_CHECK(count == boost::none);
    }

    // get_sub_group_info(const device&, cl_kernel_sub_group_info, const size_t, const void *)
    count = k.get_sub_group_info<size_t>(
        device,
        CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE_KHR,
        2 * sizeof(size_t),
        &local_work_size[0]
    );

    #ifdef BOOST_COMPUTE_CL_VERSION_2_1
    if(device.check_version(2, 1))
    {
        BOOST_CHECK(count);
    }
    else
    #endif // BOOST_COMPUTE_CL_VERSION_2_1
    if(device.check_version(2, 0) && device.supports_extension("cl_khr_subgroups"))
    {
        // for device with cl_khr_subgroups it should return some value
        BOOST_CHECK(count);
    }
    else
    {
        // for device without cl_khr_subgroups ext it should return null optional
        BOOST_CHECK(count == boost::none);
    }
}
#endif // BOOST_COMPUTE_CL_VERSION_2_0

#ifdef BOOST_COMPUTE_CL_VERSION_2_1
BOOST_AUTO_TEST_CASE(get_sub_group_info_core)
{
    compute::kernel k = compute::kernel::create_with_source(
        "__kernel void test(float x) { }", "test", context
    );

    // get_sub_group_info(const device&, cl_kernel_sub_group_info, const size_t)
    boost::optional<std::vector<size_t>> local_size =
        k.get_sub_group_info<std::vector<size_t> >(
            device,
            CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT,
            size_t(1)
        );

    if(device.check_version(2, 1))
    {
        // for 2.1 devices it should return some value
        BOOST_CHECK(local_size);
        BOOST_CHECK(local_size.value().size() == 3);
    }
    else
    {
        // for 1.x and 2.0 devices it should return null optional,
        // because CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT is not
        // supported by cl_khr_subgroups (2.0 ext)
        BOOST_CHECK(local_size == boost::none);
    }

    // get_sub_group_info(const device&, cl_kernel_sub_group_info, const size_t)
    boost::optional<size_t> local_size_simple =
        k.get_sub_group_info<size_t>(
            device,
            CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT,
            size_t(1)
        );

    if(device.check_version(2, 1))
    {
        // for 2.1 devices it should return some value
        BOOST_CHECK(local_size_simple);
    }
    else
    {
        // for 1.x and 2.0 devices it should return null optional,
        // because CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT is not
        // supported by cl_khr_subgroups (2.0 ext)
        BOOST_CHECK(local_size_simple == boost::none);
    }

    // get_sub_group_info(const device&, cl_kernel_sub_group_info)
    boost::optional<size_t> max =
        k.get_sub_group_info<size_t>(
            device,
            CL_KERNEL_MAX_NUM_SUB_GROUPS
        );

    if(device.check_version(2, 1))
    {
        // for 2.1 devices it should return some value
        BOOST_CHECK(max);
    }
    else
    {
        // for 1.x and 2.0 devices it should return null optional,
        // because CL_KERNEL_MAX_NUM_SUB_GROUPS is not
        // supported by cl_khr_subgroups (2.0 ext)
        BOOST_CHECK(max == boost::none);
    }
}
#endif // BOOST_COMPUTE_CL_VERSION_2_1

#ifdef BOOST_COMPUTE_CL_VERSION_2_1
BOOST_AUTO_TEST_CASE(clone_kernel)
{
    REQUIRES_OPENCL_PLATFORM_VERSION(2, 1);

    compute::kernel k1 = compute::kernel::create_with_source(
        "__kernel void test(__global int * x) { x[get_global_id(0)] = get_global_id(0); }",
        "test", context
    );

    compute::buffer x(context, 5 * sizeof(compute::int_));
    k1.set_arg(0, x);

    // Clone k1 kernel
    compute::kernel k2 = k1.clone();
    // After clone k2 0th argument (__global float * x) should be set,
    // so we should be able to enqueue k2 kernel without problems
    queue.enqueue_1d_range_kernel(k2, 0, x.size() / sizeof(compute::int_), 0).wait();
}
#endif // BOOST_COMPUTE_CL_VERSION_2_1

BOOST_AUTO_TEST_SUITE_END()
