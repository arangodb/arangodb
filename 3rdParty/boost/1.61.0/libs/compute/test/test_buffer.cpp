//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestBuffer
#include <boost/test/unit_test.hpp>

#include <boost/compute/buffer.hpp>
#include <boost/compute/system.hpp>
#include <boost/bind.hpp>

#ifdef BOOST_COMPUTE_USE_CPP11
#include <mutex>
#include <future>
#endif // BOOST_COMPUTE_USE_CPP11

#include "quirks.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(size)
{
    bc::buffer buffer(context, 100);
    BOOST_CHECK_EQUAL(buffer.size(), size_t(100));
    BOOST_VERIFY(buffer.max_size() > buffer.size());
}

BOOST_AUTO_TEST_CASE(cl_context)
{
    bc::buffer buffer(context, 100);
    BOOST_VERIFY(buffer.get_context() == context);
}

BOOST_AUTO_TEST_CASE(equality_operator)
{
    bc::buffer a(context, 10);
    bc::buffer b(context, 10);
    BOOST_VERIFY(a == a);
    BOOST_VERIFY(b == b);
    BOOST_VERIFY(!(a == b));
    BOOST_VERIFY(a != b);

    a = b;
    BOOST_VERIFY(a == b);
    BOOST_VERIFY(!(a != b));
}

BOOST_AUTO_TEST_CASE(construct_from_cl_mem)
{
    // create cl_mem
    cl_mem mem = clCreateBuffer(context, CL_MEM_READ_WRITE, 16, 0, 0);
    BOOST_VERIFY(mem);

    // create boost::compute::buffer
    boost::compute::buffer buffer(mem);

    // check buffer
    BOOST_CHECK(buffer.get() == mem);
    BOOST_CHECK(buffer.get_context() == context);
    BOOST_CHECK_EQUAL(buffer.size(), size_t(16));

    // cleanup cl_mem
    clReleaseMemObject(mem);
}

BOOST_AUTO_TEST_CASE(reference_count)
{
    using boost::compute::uint_;

    boost::compute::buffer buf(context, 16);
    BOOST_CHECK_GE(buf.reference_count(), uint_(1));
}

BOOST_AUTO_TEST_CASE(get_size)
{
    boost::compute::buffer buf(context, 16);
    BOOST_CHECK_EQUAL(buf.size(), size_t(16));
    BOOST_CHECK_EQUAL(buf.get_info<CL_MEM_SIZE>(), size_t(16));
    BOOST_CHECK_EQUAL(buf.get_info<size_t>(CL_MEM_SIZE), size_t(16));
}

#ifndef BOOST_COMPUTE_NO_RVALUE_REFERENCES
BOOST_AUTO_TEST_CASE(move_constructor)
{
    boost::compute::buffer buffer1(context, 16);
    BOOST_CHECK(buffer1.get() != 0);
    BOOST_CHECK_EQUAL(buffer1.size(), size_t(16));

    boost::compute::buffer buffer2(std::move(buffer1));
    BOOST_CHECK(buffer1.get() == 0);
    BOOST_CHECK(buffer2.get() != 0);
    BOOST_CHECK_EQUAL(buffer2.size(), size_t(16));
}
#endif // BOOST_COMPUTE_NO_RVALUE_REFERENCES

BOOST_AUTO_TEST_CASE(clone_buffer)
{
    boost::compute::buffer buffer1(context, 16);
    boost::compute::buffer buffer2 = buffer1.clone(queue);
    BOOST_CHECK(buffer1.get() != buffer2.get());
    BOOST_CHECK_EQUAL(buffer1.size(), buffer2.size());
    BOOST_CHECK(buffer1.get_memory_flags() == buffer2.get_memory_flags());
}

#ifdef CL_VERSION_1_1
static void BOOST_COMPUTE_CL_CALLBACK
destructor_callback_function(cl_mem memobj, void *user_data)
{
    (void) memobj;

    bool *flag = static_cast<bool *>(user_data);

    *flag = true;
}

BOOST_AUTO_TEST_CASE(destructor_callback)
{
    REQUIRES_OPENCL_VERSION(1,2);

    if(!supports_destructor_callback(device))
    {
        return;
    }

    bool invoked = false;
    {
        boost::compute::buffer buf(context, 128);
        buf.set_destructor_callback(destructor_callback_function, &invoked);
    }
    BOOST_CHECK(invoked == true);
}

#ifdef BOOST_COMPUTE_USE_CPP11

std::mutex callback_mutex;
std::condition_variable callback_condition_variable;

static void BOOST_COMPUTE_CL_CALLBACK
destructor_templated_callback_function(bool *flag)
{
    std::lock_guard<std::mutex> lock(callback_mutex);
    *flag = true;
    callback_condition_variable.notify_one();
}

BOOST_AUTO_TEST_CASE(destructor_templated_callback)
{
    bool invoked = false;
    {
        boost::compute::buffer buf(context, 128);
        buf.set_destructor_callback(boost::bind(destructor_templated_callback_function, &invoked));
    }

    std::unique_lock<std::mutex> lock(callback_mutex);
    callback_condition_variable.wait_for(
        lock, std::chrono::seconds(1), [&](){ return invoked; }
    );

    BOOST_CHECK(invoked == true);
}

#endif // BOOST_COMPUTE_USE_CPP11

BOOST_AUTO_TEST_CASE(create_subbuffer)
{
    REQUIRES_OPENCL_VERSION(1, 1);

    size_t base_addr_align = device.get_info<CL_DEVICE_MEM_BASE_ADDR_ALIGN>() / 8;
    size_t multiplier = 16;
    size_t buffer_size = base_addr_align * multiplier;
    size_t subbuffer_size = 64;
    boost::compute::buffer buffer(context, buffer_size);

    for(size_t i = 0; i < multiplier; ++i)
    {
        boost::compute::buffer subbuffer = buffer.create_subbuffer(
              boost::compute::buffer::read_write, base_addr_align * i, subbuffer_size);
        BOOST_CHECK(buffer.get() != subbuffer.get());
        BOOST_CHECK_EQUAL(subbuffer.size(), subbuffer_size);
    }
}

#endif // CL_VERSION_1_1

BOOST_AUTO_TEST_CASE(create_buffer_doctest)
{
//! [constructor]
boost::compute::buffer buf(context, 32 * sizeof(float));
//! [constructor]

    BOOST_CHECK_EQUAL(buf.size(), 32 * sizeof(float));
}

BOOST_AUTO_TEST_SUITE_END()
