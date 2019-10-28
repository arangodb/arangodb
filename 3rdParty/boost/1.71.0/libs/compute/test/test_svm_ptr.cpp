//---------------------------------------------------------------------------//
// Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestSvmPtr
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <boost/compute/core.hpp>
#include <boost/compute/svm.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/utility/source.hpp>

#include "quirks.hpp"
#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(empty)
{
}

#ifdef BOOST_COMPUTE_CL_VERSION_2_0
BOOST_AUTO_TEST_CASE(alloc)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    compute::svm_ptr<cl_int> ptr = compute::svm_alloc<cl_int>(context, 8);
    compute::svm_free(context, ptr);
}

BOOST_AUTO_TEST_CASE(svmmemcpy)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    if(bug_in_svmmemcpy(device)){
        std::cerr << "skipping svmmemcpy test case" << std::endl;
        return;
    }

    cl_int input[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    cl_int output[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    compute::svm_ptr<cl_int> ptr = compute::svm_alloc<cl_int>(context, 8);
    compute::svm_ptr<cl_int> ptr2 = compute::svm_alloc<cl_int>(context, 8);

    // copying from and to host mem
    queue.enqueue_svm_memcpy(ptr.get(), input, 8 * sizeof(cl_int));
    queue.enqueue_svm_memcpy(output, ptr.get(), 8 * sizeof(cl_int));
    queue.finish();

    CHECK_HOST_RANGE_EQUAL(cl_int, 8, output, (1, 2, 3, 4, 5, 6, 7, 8));

    // copying between svm mem
    queue.enqueue_svm_memcpy(ptr2.get(), ptr.get(), 8 * sizeof(cl_int));
    queue.enqueue_svm_memcpy(output, ptr2.get(), 8 * sizeof(cl_int));
    queue.finish();

    CHECK_HOST_RANGE_EQUAL(cl_int, 8, output, (1, 2, 3, 4, 5, 6, 7, 8));

    compute::svm_free(context, ptr);
    compute::svm_free(context, ptr2);
}

BOOST_AUTO_TEST_CASE(sum_svm_kernel)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        __kernel void sum_svm_mem(__global const int *ptr, __global int *result)
        {
            int sum = 0;
            for(uint i = 0; i < 8; i++){
                sum += ptr[i];
            }
            *result = sum;
        }
    );

    compute::program program =
        compute::program::build_with_source(source, context, "-cl-std=CL2.0");

    compute::kernel sum_svm_mem_kernel = program.create_kernel("sum_svm_mem");

    cl_int data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    compute::svm_ptr<cl_int> ptr = compute::svm_alloc<cl_int>(context, 8);
    queue.enqueue_svm_map(ptr.get(), 8 * sizeof(cl_int), CL_MAP_WRITE);
    for(size_t i = 0; i < 8; i ++) {
        static_cast<cl_int*>(ptr.get())[i] = data[i];
    }
    queue.enqueue_svm_unmap(ptr.get());

    compute::vector<cl_int> result(1, context);

    sum_svm_mem_kernel.set_arg(0, ptr);
    sum_svm_mem_kernel.set_arg(1, result);
    queue.enqueue_task(sum_svm_mem_kernel);

    queue.finish();
    BOOST_CHECK_EQUAL(result[0], (36));

    compute::svm_free(context, ptr);
}
#endif // BOOST_COMPUTE_CL_VERSION_2_0

#ifdef BOOST_COMPUTE_CL_VERSION_2_1
BOOST_AUTO_TEST_CASE(migrate)
{
    REQUIRES_OPENCL_VERSION(2, 1);

    compute::svm_ptr<cl_int> ptr =
        compute::svm_alloc<cl_int>(context, 8);

    // Migrate to device
    std::vector<const void*> ptrs(1, ptr.get());
    std::vector<size_t> sizes(1, 8 * sizeof(cl_int));
    queue.enqueue_svm_migrate_memory(ptrs, sizes).wait();

    // Set on device
    const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        __kernel void foo(__global int *ptr)
        {
            for(int i = 0; i < 8; i++){
                ptr[i] = i;
            }
        }
    );
    compute::program program =
        compute::program::build_with_source(source, context, "-cl-std=CL2.0");
    compute::kernel foo_kernel = program.create_kernel("foo");
    foo_kernel.set_arg(0, ptr);
    queue.enqueue_task(foo_kernel).wait();

    // Migrate to host
    queue.enqueue_svm_migrate_memory(
        ptr.get(), 0, boost::compute::command_queue::migrate_to_host
    ).wait();

    // Check
    CHECK_HOST_RANGE_EQUAL(
        cl_int, 8,
        static_cast<cl_int*>(ptr.get()),
        (0, 1, 2, 3, 4, 5, 6, 7)
    );
    compute::svm_free(context, ptr);
}
#endif // BOOST_COMPUTE_CL_VERSION_2_1

BOOST_AUTO_TEST_SUITE_END()
