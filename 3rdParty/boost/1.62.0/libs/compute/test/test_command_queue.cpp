//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestCommandQueue
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <boost/compute/kernel.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/program.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/fill.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/utility/dim.hpp>
#include <boost/compute/utility/source.hpp>
#include <boost/compute/detail/diagnostic.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;
namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(get_context)
{
    BOOST_VERIFY(queue.get_context() == context);
    BOOST_VERIFY(queue.get_info<CL_QUEUE_CONTEXT>() == context.get());
}

BOOST_AUTO_TEST_CASE(get_device)
{
    BOOST_VERIFY(queue.get_info<CL_QUEUE_DEVICE>() == device.get());
}

BOOST_AUTO_TEST_CASE(equality_operator)
{
    compute::command_queue queue1(context, device);
    BOOST_CHECK(queue1 == queue1);

    compute::command_queue queue2 = queue1;
    BOOST_CHECK(queue1 == queue2);

    compute::command_queue queue3(context, device);
    BOOST_CHECK(queue1 != queue3);
}

BOOST_AUTO_TEST_CASE(event_profiling)
{
    bc::command_queue queue(context, device, bc::command_queue::enable_profiling);

    int data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    bc::buffer buffer(context, sizeof(data));

    bc::event event =
        queue.enqueue_write_buffer_async(buffer,
                                         0,
                                         sizeof(data),
                                         static_cast<const void *>(data));
    queue.finish();

    event.get_profiling_info<cl_ulong>(bc::event::profiling_command_queued);
    event.get_profiling_info<cl_ulong>(bc::event::profiling_command_submit);
    event.get_profiling_info<cl_ulong>(bc::event::profiling_command_start);
    event.get_profiling_info<cl_ulong>(bc::event::profiling_command_end);
}

BOOST_AUTO_TEST_CASE(kernel_profiling)
{
    // create queue with profiling enabled
    boost::compute::command_queue queue(
        context, device, boost::compute::command_queue::enable_profiling
    );

    // input data
    int data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    boost::compute::buffer buffer(context, sizeof(data));

    // copy input data to device
    queue.enqueue_write_buffer(buffer, 0, sizeof(data), data);

    // setup kernel
    const char source[] =
        "__kernel void iscal(__global int *buffer, int alpha)\n"
        "{\n"
        "    buffer[get_global_id(0)] *= alpha;\n"
        "}\n";

    boost::compute::program program =
        boost::compute::program::create_with_source(source, context);
    program.build();

    boost::compute::kernel kernel(program, "iscal");
    kernel.set_arg(0, buffer);
    kernel.set_arg(1, 2);

    // execute kernel
    size_t global_work_offset = 0;
    size_t global_work_size = 8;

    boost::compute::event event =
        queue.enqueue_nd_range_kernel(kernel,
                                      size_t(1),
                                      &global_work_offset,
                                      &global_work_size,
                                      0);

    // wait until kernel is finished
    event.wait();

    // check profiling information
    event.get_profiling_info<cl_ulong>(bc::event::profiling_command_queued);
    event.get_profiling_info<cl_ulong>(bc::event::profiling_command_submit);
    event.get_profiling_info<cl_ulong>(bc::event::profiling_command_start);
    event.get_profiling_info<cl_ulong>(bc::event::profiling_command_end);

    // read results back to host
    queue.enqueue_read_buffer(buffer, 0, sizeof(data), data);

    // check results
    BOOST_CHECK_EQUAL(data[0], 2);
    BOOST_CHECK_EQUAL(data[1], 4);
    BOOST_CHECK_EQUAL(data[2], 6);
    BOOST_CHECK_EQUAL(data[3], 8);
    BOOST_CHECK_EQUAL(data[4], 10);
    BOOST_CHECK_EQUAL(data[5], 12);
    BOOST_CHECK_EQUAL(data[6], 14);
    BOOST_CHECK_EQUAL(data[7], 16);
}

BOOST_AUTO_TEST_CASE(construct_from_cl_command_queue)
{
    // create cl_command_queue
    cl_command_queue cl_queue;
#ifdef CL_VERSION_2_0
    if (device.check_version(2, 0)){ // runtime check
        cl_queue =
            clCreateCommandQueueWithProperties(context, device.id(), 0, 0);
    } else
#endif // CL_VERSION_2_0
    {
        // Suppress deprecated declarations warning
        BOOST_COMPUTE_DISABLE_DEPRECATED_DECLARATIONS();
        cl_queue =
            clCreateCommandQueue(context, device.id(), 0, 0);
        BOOST_COMPUTE_ENABLE_DEPRECATED_DECLARATIONS();
    }
    BOOST_VERIFY(cl_queue);

    // create boost::compute::command_queue
    boost::compute::command_queue queue(cl_queue);

    // check queue
    BOOST_CHECK(queue.get_context() == context);
    BOOST_CHECK(cl_command_queue(queue) == cl_queue);

    // cleanup cl_command_queue
    clReleaseCommandQueue(cl_queue);
}

#ifdef CL_VERSION_1_1
BOOST_AUTO_TEST_CASE(write_buffer_rect)
{
    REQUIRES_OPENCL_VERSION(1, 1);

    // skip this test on AMD GPUs due to a buggy implementation
    // of the clEnqueueWriteBufferRect() function
    if(device.vendor() == "Advanced Micro Devices, Inc." &&
       device.type() & boost::compute::device::gpu){
        std::cerr << "skipping write_buffer_rect test on AMD GPU" << std::endl;
        return;
    }

    int data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    boost::compute::buffer buffer(context, 8 * sizeof(int));

    // copy every other value to the buffer
    size_t buffer_origin[] = { 0, 0, 0 };
    size_t host_origin[] = { 0, 0, 0 };
    size_t region[] = { sizeof(int), sizeof(int), 1 };

    queue.enqueue_write_buffer_rect(
        buffer,
        buffer_origin,
        host_origin,
        region,
        sizeof(int),
        0,
        2 * sizeof(int),
        0,
        data
    );

    // check output values
    int output[4];
    queue.enqueue_read_buffer(buffer, 0, 4 * sizeof(int), output);
    BOOST_CHECK_EQUAL(output[0], 1);
    BOOST_CHECK_EQUAL(output[1], 3);
    BOOST_CHECK_EQUAL(output[2], 5);
    BOOST_CHECK_EQUAL(output[3], 7);
}
#endif // CL_VERSION_1_1

static bool nullary_kernel_executed = false;

static void nullary_kernel()
{
    nullary_kernel_executed = true;
}

BOOST_AUTO_TEST_CASE(native_kernel)
{
    cl_device_exec_capabilities exec_capabilities =
        device.get_info<CL_DEVICE_EXECUTION_CAPABILITIES>();
    if(!(exec_capabilities & CL_EXEC_NATIVE_KERNEL)){
        std::cerr << "skipping native_kernel test: "
                  << "device does not support CL_EXEC_NATIVE_KERNEL"
                  << std::endl;
        return;
    }

    compute::vector<int> vector(1000, context);
    compute::fill(vector.begin(), vector.end(), 42, queue);
    BOOST_CHECK_EQUAL(nullary_kernel_executed, false);
    queue.enqueue_native_kernel(&nullary_kernel);
    queue.finish();
    BOOST_CHECK_EQUAL(nullary_kernel_executed, true);
}

BOOST_AUTO_TEST_CASE(copy_with_wait_list)
{
    int data1[] = { 1, 3, 5, 7 };
    int data2[] = { 2, 4, 6, 8 };

    compute::buffer buf1(context, 4 * sizeof(int));
    compute::buffer buf2(context, 4 * sizeof(int));

    compute::event write_event1 =
        queue.enqueue_write_buffer_async(buf1, 0, buf1.size(), data1);

    compute::event write_event2 =
        queue.enqueue_write_buffer_async(buf2, 0, buf2.size(), data2);

    compute::event read_event1 =
        queue.enqueue_read_buffer_async(buf1, 0, buf1.size(), data2, write_event1);

    compute::event read_event2 =
        queue.enqueue_read_buffer_async(buf2, 0, buf2.size(), data1, write_event2);

    read_event1.wait();
    read_event2.wait();

    CHECK_HOST_RANGE_EQUAL(int, 4, data1, (2, 4, 6, 8));
    CHECK_HOST_RANGE_EQUAL(int, 4, data2, (1, 3, 5, 7));
}

BOOST_AUTO_TEST_CASE(enqueue_kernel_with_extents)
{
    using boost::compute::dim;
    using boost::compute::uint_;

    const char source[] = BOOST_COMPUTE_STRINGIZE_SOURCE(
        __kernel void foo(__global int *output1, __global int *output2)
        {
            output1[get_global_id(0)] = get_local_id(0);
            output2[get_global_id(1)] = get_local_id(1);
        }
    );

    compute::kernel kernel =
        compute::kernel::create_with_source(source, "foo", context);

    compute::vector<uint_> output1(4, context);
    compute::vector<uint_> output2(4, context);

    kernel.set_arg(0, output1);
    kernel.set_arg(1, output2);

    queue.enqueue_nd_range_kernel(kernel, dim(0, 0), dim(4, 4), dim(1, 1));

    CHECK_RANGE_EQUAL(int, 4, output1, (0, 0, 0, 0));
    CHECK_RANGE_EQUAL(int, 4, output2, (0, 0, 0, 0));

    // Maximum number of work-items that can be specified in each
    // dimension of the work-group to clEnqueueNDRangeKernel.
    std::vector<size_t> max_work_item_sizes =
        device.get_info<CL_DEVICE_MAX_WORK_ITEM_SIZES>();

    if(max_work_item_sizes[0] < size_t(2)) {
        return;
    }

    queue.enqueue_nd_range_kernel(kernel, dim(0, 0), dim(4, 4), dim(2, 1));

    CHECK_RANGE_EQUAL(int, 4, output1, (0, 1, 0, 1));
    CHECK_RANGE_EQUAL(int, 4, output2, (0, 0, 0, 0));

    if(max_work_item_sizes[1] < size_t(2)) {
        return;
    }

    queue.enqueue_nd_range_kernel(kernel, dim(0, 0), dim(4, 4), dim(2, 2));

    CHECK_RANGE_EQUAL(int, 4, output1, (0, 1, 0, 1));
    CHECK_RANGE_EQUAL(int, 4, output2, (0, 1, 0, 1));
}

BOOST_AUTO_TEST_SUITE_END()
