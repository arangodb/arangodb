//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestCopy
#include <boost/test/unit_test.hpp>

#include <list>
#include <vector>
#include <string>
#include <sstream>
#include <iterator>
#include <iostream>

#include <boost/compute/svm.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/functional.hpp>
#include <boost/compute/command_queue.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/copy_n.hpp>
#include <boost/compute/algorithm/fill.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/async/future.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/detail/device_ptr.hpp>
#include <boost/compute/iterator/detail/swizzle_iterator.hpp>

#include "quirks.hpp"
#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;
namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(copy_on_device)
{
    float data[] = { 6.1f, 10.2f, 19.3f, 25.4f };
    bc::vector<float> a(4, context);
    bc::copy(data, data + 4, a.begin(), queue);
    CHECK_RANGE_EQUAL(float, 4, a, (6.1f, 10.2f, 19.3f, 25.4f));

    bc::vector<float> b(4, context);
    bc::fill(b.begin(), b.end(), 0, queue);
    CHECK_RANGE_EQUAL(float, 4, b, (0.0f, 0.0f, 0.0f, 0.0f));

    bc::copy(a.begin(), a.end(), b.begin(), queue);
    CHECK_RANGE_EQUAL(float, 4, b, (6.1f, 10.2f, 19.3f, 25.4f));

    bc::vector<float> c(context);
    bc::copy(c.begin(), c.end(), b.begin(), queue);
    CHECK_RANGE_EQUAL(float, 4, b, (6.1f, 10.2f, 19.3f, 25.4f));
}

BOOST_AUTO_TEST_CASE(copy_on_device_device_ptr)
{
    float data[] = { 6.1f, 10.2f, 19.3f, 25.4f };
    bc::vector<float> a(4, context);
    bc::copy(data, data + 4, a.begin(), queue);
    CHECK_RANGE_EQUAL(float, 4, a, (6.1f, 10.2f, 19.3f, 25.4f));

    bc::vector<float> b(4, context);
    bc::detail::device_ptr<float> b_ptr(b.get_buffer(), size_t(0));

    // buffer_iterator -> device_ptr
    bc::copy(a.begin(), a.end(), b_ptr, queue);
    CHECK_RANGE_EQUAL(float, 4, b, (6.1f, 10.2f, 19.3f, 25.4f));

    bc::vector<float> c(4, context);
    bc::fill(c.begin(), c.end(), 0.0f, queue);
    bc::detail::device_ptr<float> c_ptr(c.get_buffer(), size_t(2));

    // device_ptr -> device_ptr
    bc::copy(b_ptr, b_ptr + 2, c_ptr, queue);
    CHECK_RANGE_EQUAL(float, 4, c, (0.0f, 0.0f, 6.1f, 10.2f));

    // device_ptr -> buffer_iterator
    bc::copy(c_ptr, c_ptr + 2, a.begin() + 2, queue);
    CHECK_RANGE_EQUAL(float, 4, a, (6.1f, 10.2f, 6.1f, 10.2f));
}

BOOST_AUTO_TEST_CASE(copy_on_host)
{
    int data[] = { 2, 4, 6, 8 };
    std::vector<int> vector(4);
    compute::copy(data, data + 4, vector.begin(), queue);
    CHECK_RANGE_EQUAL(int, 4, vector, (2, 4, 6, 8));
}

BOOST_AUTO_TEST_CASE(copy)
{
    int data[] = { 1, 2, 5, 6 };
    bc::vector<int> vector(4, context);
    bc::copy(data, data + 4, vector.begin(), queue);
    CHECK_RANGE_EQUAL(int, 4, vector, (1, 2, 5, 6));

    std::vector<int> host_vector(4);
    bc::copy(vector.begin(), vector.end(), host_vector.begin(), queue);
    BOOST_CHECK_EQUAL(host_vector[0], 1);
    BOOST_CHECK_EQUAL(host_vector[1], 2);
    BOOST_CHECK_EQUAL(host_vector[2], 5);
    BOOST_CHECK_EQUAL(host_vector[3], 6);
}

BOOST_AUTO_TEST_CASE(empty_copy)
{
    int data[] = { 1, 2, 5, 6 };
    bc::vector<int> a(4, context);
    bc::vector<int> b(context);
    std::vector<int> c;

    bc::copy(data, data + 4, a.begin(), queue);
    CHECK_RANGE_EQUAL(int, 4, a, (1, 2, 5, 6));

    bc::copy(b.begin(), b.end(), a.begin(), queue);
    CHECK_RANGE_EQUAL(int, 4, a, (1, 2, 5, 6));

    bc::copy(c.begin(), c.end(), a.begin(), queue);
    CHECK_RANGE_EQUAL(int, 4, a, (1, 2, 5, 6));

    bc::future<bc::vector<int>::iterator> future =
        bc::copy_async(c.begin(), c.end(), a.begin(), queue);
    if(future.valid())
        future.wait();
    CHECK_RANGE_EQUAL(int, 4, a, (1, 2, 5, 6));
}

// Test copying from a std::list to a bc::vector. This differs from
// the test copying from std::vector because std::list has non-contigous
// storage for its data values.
BOOST_AUTO_TEST_CASE(copy_from_host_list)
{
    int data[] = { -4, 12, 9, 0 };
    std::list<int> host_list(data, data + 4);

    bc::vector<int> vector(4, context);
    bc::copy(host_list.begin(), host_list.end(), vector.begin(), queue);
    CHECK_RANGE_EQUAL(int, 4, vector, (-4, 12, 9, 0));
}

BOOST_AUTO_TEST_CASE(copy_n_int)
{
    int data[] = { 1, 2, 3, 4, 5 };
    bc::vector<int> a(data, data + 5, queue);

    bc::vector<int> b(5, context);
    bc::fill(b.begin(), b.end(), 0, queue);
    bc::copy_n(a.begin(), 3, b.begin(), queue);
    CHECK_RANGE_EQUAL(int, 5, b, (1, 2, 3, 0, 0));

    bc::copy_n(b.begin(), 4, a.begin(), queue);
    CHECK_RANGE_EQUAL(int, 5, a, (1, 2, 3, 0, 5));
}

BOOST_AUTO_TEST_CASE(copy_swizzle_iterator)
{
    using bc::int2_;
    using bc::int4_;

    int data[] = { 1, 2, 3, 4,
                   5, 6, 7, 8,
                   9, 1, 2, 3,
                   4, 5, 6, 7 };

    bc::vector<int4_> input(reinterpret_cast<int4_*>(data),
                            reinterpret_cast<int4_*>(data) + 4,
                            queue);
    BOOST_CHECK_EQUAL(input.size(), size_t(4));
    CHECK_RANGE_EQUAL(int4_, 4, input,
        (int4_(1, 2, 3, 4),
         int4_(5, 6, 7, 8),
         int4_(9, 1, 2, 3),
         int4_(4, 5, 6, 7))
    );

    bc::vector<int4_> output4(4, context);
    bc::copy(
        bc::detail::make_swizzle_iterator<4>(input.begin(), "wzyx"),
        bc::detail::make_swizzle_iterator<4>(input.end(), "wzyx"),
        output4.begin(),
        queue
    );
    CHECK_RANGE_EQUAL(int4_, 4, output4,
        (int4_(4, 3, 2, 1),
         int4_(8, 7, 6, 5),
         int4_(3, 2, 1, 9),
         int4_(7, 6, 5, 4))
    );

    bc::vector<int2_> output2(4, context);
    bc::copy(
        bc::detail::make_swizzle_iterator<2>(input.begin(), "xz"),
        bc::detail::make_swizzle_iterator<2>(input.end(), "xz"),
        output2.begin(),
        queue
    );
    CHECK_RANGE_EQUAL(int2_, 4, output2,
        (int2_(1, 3),
         int2_(5, 7),
         int2_(9, 2),
         int2_(4, 6))
    );

    bc::vector<int> output1(4, context);
    bc::copy(
        bc::detail::make_swizzle_iterator<1>(input.begin(), "y"),
        bc::detail::make_swizzle_iterator<1>(input.end(), "y"),
        output1.begin(),
        queue
    );
    CHECK_RANGE_EQUAL(int, 4, output1, (2, 6, 1, 5));
}

BOOST_AUTO_TEST_CASE(copy_int_async)
{
    // setup host data
    int host_data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    typedef int* host_iterator;

    // setup device data
    bc::vector<int> device_data(8, context);
    typedef bc::vector<int>::iterator device_iterator;

    // copy data to device
    bc::future<device_iterator> host_to_device_future =
        bc::copy_async(host_data, host_data + 8, device_data.begin(), queue);

    // wait for copy to complete
    host_to_device_future.wait();

    // check results
    CHECK_RANGE_EQUAL(int, 8, device_data, (1, 2, 3, 4, 5, 6, 7, 8));
    BOOST_VERIFY(host_to_device_future.get() == device_data.end());

    // fill host data with zeros
    std::fill(host_data, host_data + 8, int(0));

    // copy data back to host
    bc::future<host_iterator> device_to_host_future =
        bc::copy_async(device_data.begin(), device_data.end(), host_data, queue);

    // wait for copy to complete
    device_to_host_future.wait();

    // check results
    BOOST_CHECK_EQUAL(host_data[0], int(1));
    BOOST_CHECK_EQUAL(host_data[1], int(2));
    BOOST_CHECK_EQUAL(host_data[2], int(3));
    BOOST_CHECK_EQUAL(host_data[3], int(4));
    BOOST_CHECK_EQUAL(host_data[4], int(5));
    BOOST_CHECK_EQUAL(host_data[5], int(6));
    BOOST_CHECK_EQUAL(host_data[6], int(7));
    BOOST_CHECK_EQUAL(host_data[7], int(8));
    BOOST_VERIFY(device_to_host_future.get() == host_data + 8);
}

BOOST_AUTO_TEST_CASE(copy_to_back_inserter)
{
    compute::vector<int> device_vector(5, context);
    compute::iota(device_vector.begin(), device_vector.end(), 10, queue);

    std::vector<int> host_vector;
    compute::copy(
        device_vector.begin(),
        device_vector.end(),
        std::back_inserter(host_vector),
        queue
    );

    BOOST_CHECK_EQUAL(host_vector.size(), size_t(5));
    BOOST_CHECK_EQUAL(host_vector[0], 10);
    BOOST_CHECK_EQUAL(host_vector[1], 11);
    BOOST_CHECK_EQUAL(host_vector[2], 12);
    BOOST_CHECK_EQUAL(host_vector[3], 13);
    BOOST_CHECK_EQUAL(host_vector[4], 14);
}

BOOST_AUTO_TEST_CASE(copy_to_stringstream)
{
    std::stringstream stream;

    int data[] = { 2, 3, 4, 5, 6, 7, 8, 9 };
    compute::vector<int> vector(data, data + 8, queue);

    compute::copy(
        vector.begin(),
        vector.end(),
        std::ostream_iterator<int>(stream, " "),
        queue
    );
    BOOST_CHECK_EQUAL(stream.str(), std::string("2 3 4 5 6 7 8 9 "));
}

BOOST_AUTO_TEST_CASE(check_copy_type)
{
    // copy from host to device and ensure clEnqueueWriteBuffer() is used
    int data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    compute::vector<int> a(8, context);
    compute::future<void> future =
        compute::copy_async(data, data + 8, a.begin(), queue);
    BOOST_CHECK(
        future.get_event().get_command_type() == CL_COMMAND_WRITE_BUFFER
    );
    future.wait();
    CHECK_RANGE_EQUAL(int, 8, a, (1, 2, 3, 4, 5, 6, 7, 8));

    // copy on the device and ensure clEnqueueCopyBuffer() is used
    compute::vector<int> b(8, context);
    future = compute::copy_async(a.begin(), a.end(), b.begin(), queue);
    BOOST_CHECK(
        future.get_event().get_command_type() == CL_COMMAND_COPY_BUFFER
    );
    future.wait();
    CHECK_RANGE_EQUAL(int, 8, b, (1, 2, 3, 4, 5, 6, 7, 8));

    // copy between vectors of different types on the device and ensure
    // that the copy kernel is used
    compute::vector<short> c(8, context);
    future = compute::copy_async(a.begin(), a.end(), c.begin(), queue);
    BOOST_CHECK(
        future.get_event().get_command_type() == CL_COMMAND_NDRANGE_KERNEL
    );
    future.wait();
    CHECK_RANGE_EQUAL(short, 8, c, (1, 2, 3, 4, 5, 6, 7, 8));

    // copy from device to host and ensure clEnqueueReadBuffer() is used
    future = compute::copy_async(b.begin(), b.end(), data, queue);
    BOOST_CHECK(
        future.get_event().get_command_type() == CL_COMMAND_READ_BUFFER
    );
    future.wait();
    CHECK_HOST_RANGE_EQUAL(int, 8, data, (1, 2, 3, 4, 5, 6, 7, 8));
}

#ifdef CL_VERSION_2_0
BOOST_AUTO_TEST_CASE(copy_svm_ptr)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    using boost::compute::int_;

    if(bug_in_svmmemcpy(device)){
        std::cerr << "skipping copy_svm_ptr test case" << std::endl;
        return;
    }

    int_ data[] = { 1, 3, 2, 4 };

    compute::svm_ptr<int_> ptr = compute::svm_alloc<int_>(context, 4);
    compute::copy(data, data + 4, ptr, queue);

    int_ output[] = { 0, 0, 0, 0 };
    compute::copy(ptr, ptr + 4, output, queue);
    CHECK_HOST_RANGE_EQUAL(int_, 4, output, (1, 3, 2, 4));

    compute::svm_free(context, ptr);
}

BOOST_AUTO_TEST_CASE(copy_async_svm_ptr)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    using boost::compute::int_;

    if(bug_in_svmmemcpy(device)){
        std::cerr << "skipping copy_svm_ptr test case" << std::endl;
        return;
    }

    int_ data[] = { 1, 3, 2, 4 };

    compute::svm_ptr<int_> ptr = compute::svm_alloc<int_>(context, 4);
    boost::compute::future<void> future =
        compute::copy_async(data, data + 4, ptr, queue);
    future.wait();

    int_ output[] = { 0, 0, 0, 0 };
    future =
        compute::copy_async(ptr, ptr + 4, output, queue);
    future.wait();
    CHECK_HOST_RANGE_EQUAL(int_, 4, output, (1, 3, 2, 4));

    compute::svm_free(context, ptr);
}
#endif // CL_VERSION_2_0

BOOST_AUTO_TEST_CASE(copy_to_vector_bool)
{
    using compute::uchar_;

    compute::vector<uchar_> vec(2, context);

    // copy to device
    bool data[] = {true, false};
    compute::copy(data, data + 2, vec.begin(), queue);
    BOOST_CHECK(static_cast<bool>(vec[0]) == true);
    BOOST_CHECK(static_cast<bool>(vec[1]) == false);

    // copy to host
    std::vector<bool> host_vec(vec.size());
    compute::copy(vec.begin(), vec.end(), host_vec.begin(), queue);
    BOOST_CHECK(host_vec[0] == true);
    BOOST_CHECK(host_vec[1] == false);
}

BOOST_AUTO_TEST_SUITE_END()
