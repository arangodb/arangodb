//---------------------------------------------------------------------------//
// Copyright (c) 2016 Jakub Szuppe <j.szuppe@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

// Undefining BOOST_COMPUTE_USE_OFFLINE_CACHE macro as we want to modify cached
// parameters for copy algorithm without any undesirable consequences (like
// saving modified values of those parameters).
#ifdef BOOST_COMPUTE_USE_OFFLINE_CACHE
    #undef BOOST_COMPUTE_USE_OFFLINE_CACHE
#endif

#define BOOST_TEST_MODULE TestCopyTypeMismatch
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
#include <boost/compute/async/future.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/detail/device_ptr.hpp>
#include <boost/compute/memory/svm_ptr.hpp>
#include <boost/compute/detail/parameter_cache.hpp>

#include "quirks.hpp"
#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;
namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(is_same_ignore_const)
{
    BOOST_STATIC_ASSERT((
        boost::compute::detail::is_same_value_type<
            std::vector<int>::iterator,
            compute::buffer_iterator<int>
        >::value
    ));
    BOOST_STATIC_ASSERT((
        boost::compute::detail::is_same_value_type<
            std::vector<int>::const_iterator,
            compute::buffer_iterator<int>
        >::value
    ));
    BOOST_STATIC_ASSERT((
        boost::compute::detail::is_same_value_type<
            std::vector<int>::iterator,
            compute::buffer_iterator<const int>
        >::value
    ));
    BOOST_STATIC_ASSERT((
        boost::compute::detail::is_same_value_type<
            std::vector<int>::const_iterator,
            compute::buffer_iterator<const int>
        >::value
    ));
}

BOOST_AUTO_TEST_CASE(copy_to_device_float_to_double)
{
    if(!device.supports_extension("cl_khr_fp64")) {
        std::cout << "skipping test: device does not support double" << std::endl;
        return;
    }

    using compute::double_;
    using compute::float_;

    float_ host[] = { 6.1f, 10.2f, 19.3f, 25.4f };
    bc::vector<double_> device_vector(4, context);

    // copy host float data to double device vector
    bc::copy(host, host + 4, device_vector.begin(), queue);
    CHECK_RANGE_EQUAL(double_, 4, device_vector, (6.1f, 10.2f, 19.3f, 25.4f));
}

BOOST_AUTO_TEST_CASE(copy_to_device_float_to_int)
{
    using compute::int_;
    using compute::float_;

    float_ host[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    bc::vector<int_> device_vector(4, context);

    // copy host float data to int device vector
    bc::copy(host, host + 4, device_vector.begin(), queue);
    CHECK_RANGE_EQUAL(
        int_,
        4,
        device_vector,
        (
            static_cast<int_>(6.1f),
            static_cast<int_>(-10.2f),
            static_cast<int_>(19.3f),
            static_cast<int_>(25.4f)
        )
    );
}

// HOST -> DEVICE

BOOST_AUTO_TEST_CASE(copy_to_device_float_to_int_mapping_device_vector)
{
    using compute::int_;
    using compute::uint_;
    using compute::float_;

    float_ host[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    bc::vector<int_> device_vector(4, context);

    std::string cache_key =
        std::string("__boost_compute_copy_to_device_float_int");
    boost::shared_ptr<bc::detail::parameter_cache> parameters =
        bc::detail::parameter_cache::get_global_cache(device);

    // save
    uint_ map_copy_threshold =
        parameters->get(cache_key, "map_copy_threshold", 0);

    // force copy_to_device_map (mapping device vector to the host)
    parameters->set(cache_key, "map_copy_threshold", 1024);

    // copy host float data to int device vector
    bc::copy(host, host + 4, device_vector.begin(), queue);
    CHECK_RANGE_EQUAL(
        int_,
        4,
        device_vector,
        (
            static_cast<int_>(6.1f),
            static_cast<int_>(-10.2f),
            static_cast<int_>(19.3f),
            static_cast<int_>(25.4f)
        )
    );

    // restore
    parameters->set(cache_key, "map_copy_threshold", map_copy_threshold);
}

BOOST_AUTO_TEST_CASE(copy_to_device_float_to_int_convert_on_host)
{
    using compute::int_;
    using compute::uint_;
    using compute::float_;

    std::string cache_key =
        std::string("__boost_compute_copy_to_device_float_int");
    boost::shared_ptr<bc::detail::parameter_cache> parameters =
        bc::detail::parameter_cache::get_global_cache(device);

    // save
    uint_ map_copy_threshold =
        parameters->get(cache_key, "map_copy_threshold", 0);
    uint_ direct_copy_threshold =
        parameters->get(cache_key, "direct_copy_threshold", 0);

    // force copying by casting input data on host and performing
    // normal copy host->device (since types match now)
    parameters->set(cache_key, "map_copy_threshold", 0);
    parameters->set(cache_key, "direct_copy_threshold", 1024);

    float_ host[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    bc::vector<int_> device_vector(4, context);

    // copy host float data to int device vector
    bc::copy(host, host + 4, device_vector.begin(), queue);
    CHECK_RANGE_EQUAL(
        int_,
        4,
        device_vector,
        (
            static_cast<int_>(6.1f),
            static_cast<int_>(-10.2f),
            static_cast<int_>(19.3f),
            static_cast<int_>(25.4f)
        )
    );

    // restore
    parameters->set(cache_key, "map_copy_threshold", map_copy_threshold);
    parameters->set(cache_key, "direct_copy_threshold", direct_copy_threshold);
}

BOOST_AUTO_TEST_CASE(copy_to_device_float_to_int_with_transform)
{
    using compute::int_;
    using compute::uint_;
    using compute::float_;

    std::string cache_key =
        std::string("__boost_compute_copy_to_device_float_int");
    boost::shared_ptr<bc::detail::parameter_cache> parameters =
        bc::detail::parameter_cache::get_global_cache(device);

    // save
    uint_ map_copy_threshold =
        parameters->get(cache_key, "map_copy_threshold", 0);
    uint_ direct_copy_threshold =
        parameters->get(cache_key, "direct_copy_threshold", 0);

    // force copying by mapping input data to the device memory
    // and using transform operation for casting & copying
    parameters->set(cache_key, "map_copy_threshold", 0);
    parameters->set(cache_key, "direct_copy_threshold", 0);

    float_ host[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    bc::vector<int_> device_vector(4, context);

    // copy host float data to int device vector
    bc::copy(host, host + 4, device_vector.begin(), queue);
    CHECK_RANGE_EQUAL(
        int_,
        4,
        device_vector,
        (
            static_cast<int_>(6.1f),
            static_cast<int_>(-10.2f),
            static_cast<int_>(19.3f),
            static_cast<int_>(25.4f)
        )
    );

    // restore
    parameters->set(cache_key, "map_copy_threshold", map_copy_threshold);
    parameters->set(cache_key, "direct_copy_threshold", direct_copy_threshold);
}

BOOST_AUTO_TEST_CASE(copy_async_to_device_float_to_int)
{
    using compute::int_;
    using compute::float_;

    float_ host[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    bc::vector<int_> device_vector(4, context);

    // copy host float data to int device vector
    compute::future<void> future =
        bc::copy_async(host, host + 4, device_vector.begin(), queue);
    future.wait();

    CHECK_RANGE_EQUAL(
        int_,
        4,
        device_vector,
        (
            static_cast<int_>(6.1f),
            static_cast<int_>(-10.2f),
            static_cast<int_>(19.3f),
            static_cast<int_>(25.4f)
        )
    );
}

BOOST_AUTO_TEST_CASE(copy_async_to_device_float_to_int_empty)
{
    using compute::int_;
    using compute::float_;

    float_ host[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    bc::vector<int_> device_vector(size_t(4), int_(1), queue);

    // copy nothing to int device vector
    compute::future<bc::vector<int_>::iterator > future =
        bc::copy_async(host, host, device_vector.begin(), queue);
    if(future.valid()) {
        future.wait();
    }

    CHECK_RANGE_EQUAL(
        int_,
        4,
        device_vector,
        (
            int_(1),
            int_(1),
            int_(1),
            int_(1)
        )
    );
}

// Test copying from a std::list to a bc::vector. This differs from
// the test copying from std::vector because std::list has non-contiguous
// storage for its data values.
BOOST_AUTO_TEST_CASE(copy_to_device_float_to_int_list_device_map)
{
    using compute::int_;
    using compute::uint_;
    using compute::float_;

    std::string cache_key =
        std::string("__boost_compute_copy_to_device_float_int");
    boost::shared_ptr<bc::detail::parameter_cache> parameters =
        bc::detail::parameter_cache::get_global_cache(device);

    // save
    uint_ map_copy_threshold =
        parameters->get(cache_key, "map_copy_threshold", 0);

    // force copy_to_device_map (mapping device vector to the host)
    parameters->set(cache_key, "map_copy_threshold", 1024);

    float_ data[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    std::list<float_> host(data, data + 4);
    bc::vector<int_> device_vector(4, context);

    // copy host float data to int device vector
    bc::copy(host.begin(), host.end(), device_vector.begin(), queue);
    CHECK_RANGE_EQUAL(
        int_,
        4,
        device_vector,
        (
            static_cast<int_>(6.1f),
            static_cast<int_>(-10.2f),
            static_cast<int_>(19.3f),
            static_cast<int_>(25.4f)
        )
    );

    // restore
    parameters->set(cache_key, "map_copy_threshold", map_copy_threshold);
}

// Test copying from a std::list to a bc::vector. This differs from
// the test copying from std::vector because std::list has non-contiguous
// storage for its data values.
BOOST_AUTO_TEST_CASE(copy_to_device_float_to_int_list_convert_on_host)
{
    using compute::int_;
    using compute::uint_;
    using compute::float_;

    std::string cache_key =
        std::string("__boost_compute_copy_to_device_float_int");
    boost::shared_ptr<bc::detail::parameter_cache> parameters =
        bc::detail::parameter_cache::get_global_cache(device);

    // save
    uint_ map_copy_threshold =
        parameters->get(cache_key, "map_copy_threshold", 0);
    uint_ direct_copy_threshold =
        parameters->get(cache_key, "direct_copy_threshold", 0);

    // force copying by casting input data on host and performing
    // normal copy host->device (since types match now)
    parameters->set(cache_key, "map_copy_threshold", 0);
    parameters->set(cache_key, "direct_copy_threshold", 1024);

    float_ data[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    std::list<float_> host(data, data + 4);
    bc::vector<int_> device_vector(4, context);

    // copy host float data to int device vector
    bc::copy(host.begin(), host.end(), device_vector.begin(), queue);
    CHECK_RANGE_EQUAL(
        int_,
        4,
        device_vector,
        (
            static_cast<int_>(6.1f),
            static_cast<int_>(-10.2f),
            static_cast<int_>(19.3f),
            static_cast<int_>(25.4f)
        )
    );

    // restore
    parameters->set(cache_key, "map_copy_threshold", map_copy_threshold);
    parameters->set(cache_key, "direct_copy_threshold", direct_copy_threshold);
}

// SVM requires OpenCL 2.0
#if defined(BOOST_COMPUTE_CL_VERSION_2_0) || defined(BOOST_COMPUTE_DOXYGEN_INVOKED)
BOOST_AUTO_TEST_CASE(copy_to_device_svm_float_to_int_map)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    using compute::int_;
    using compute::uint_;
    using compute::float_;

    std::string cache_key =
        std::string("__boost_compute_copy_to_device_float_int");
    boost::shared_ptr<bc::detail::parameter_cache> parameters =
        bc::detail::parameter_cache::get_global_cache(device);

    // save
    uint_ map_copy_threshold =
        parameters->get(cache_key, "map_copy_threshold", 0);

    // force copy_to_device_map (mapping device vector to the host)
    parameters->set(cache_key, "map_copy_threshold", 1024);

    float_ host[] = { 5.1f, -10.3f, 19.4f, 26.7f };
    compute::svm_ptr<int_> ptr = compute::svm_alloc<int_>(context, 4);

    // copy host float data to int device vector
    bc::copy(host, host + 4, ptr, queue);

    queue.enqueue_svm_map(ptr.get(), 4 * sizeof(cl_int), CL_MAP_READ);
    CHECK_HOST_RANGE_EQUAL(
        int_,
        4,
        static_cast<int_*>(ptr.get()),
        (
            static_cast<int_>(5.1f),
            static_cast<int_>(-10.3f),
            static_cast<int_>(19.4f),
            static_cast<int_>(26.7f)
        )
    );
    queue.enqueue_svm_unmap(ptr.get()).wait();

    compute::svm_free(context, ptr);

    // restore
    parameters->set(cache_key, "map_copy_threshold", map_copy_threshold);
}

BOOST_AUTO_TEST_CASE(copy_to_device_svm_float_to_int_convert_on_host)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    if(bug_in_svmmemcpy(device)){
        std::cerr << "skipping svmmemcpy test case" << std::endl;
        return;
    }

    using compute::int_;
    using compute::uint_;
    using compute::float_;

    std::string cache_key =
        std::string("__boost_compute_copy_to_device_float_int");
    boost::shared_ptr<bc::detail::parameter_cache> parameters =
        bc::detail::parameter_cache::get_global_cache(device);

    // save
    uint_ map_copy_threshold =
        parameters->get(cache_key, "map_copy_threshold", 0);
    uint_ direct_copy_threshold =
        parameters->get(cache_key, "direct_copy_threshold", 0);

    // force copying by casting input data on host and performing
    // normal copy host->device (since types match now)
    parameters->set(cache_key, "map_copy_threshold", 0);
    parameters->set(cache_key, "direct_copy_threshold", 1024);

    float_ host[] = { 0.1f, 10.3f, 9.4f, -26.7f };
    compute::svm_ptr<int_> ptr = compute::svm_alloc<int_>(context, 4);

    // copy host float data to int device vector
    bc::copy(host, host + 4, ptr, queue);

    queue.enqueue_svm_map(ptr.get(), 4 * sizeof(cl_int), CL_MAP_READ);
    CHECK_HOST_RANGE_EQUAL(
        int_,
        4,
        static_cast<int_*>(ptr.get()),
        (
            static_cast<int_>(0.1f),
            static_cast<int_>(10.3f),
            static_cast<int_>(9.4f),
            static_cast<int_>(-26.7f)
        )
    );
    queue.enqueue_svm_unmap(ptr.get()).wait();

    compute::svm_free(context, ptr);

    // restore
    parameters->set(cache_key, "map_copy_threshold", map_copy_threshold);
    parameters->set(cache_key, "direct_copy_threshold", direct_copy_threshold);
}

BOOST_AUTO_TEST_CASE(copy_to_device_svm_float_to_int_with_transform)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    using compute::int_;
    using compute::uint_;
    using compute::float_;

    std::string cache_key =
        std::string("__boost_compute_copy_to_device_float_int");
    boost::shared_ptr<bc::detail::parameter_cache> parameters =
        bc::detail::parameter_cache::get_global_cache(device);

    // save
    uint_ map_copy_threshold =
        parameters->get(cache_key, "map_copy_threshold", 0);
    uint_ direct_copy_threshold =
        parameters->get(cache_key, "direct_copy_threshold", 0);

    // force copying by mapping input data to the device memory
    // and using transform operation (copy kernel) for casting & copying
    parameters->set(cache_key, "map_copy_threshold", 0);
    parameters->set(cache_key, "direct_copy_threshold", 0);

    float_ host[] = { 4.1f, -11.3f, 219.4f, -26.7f };
    compute::svm_ptr<int_> ptr = compute::svm_alloc<int_>(context, 4);

    // copy host float data to int device vector
    bc::copy(host, host + 4, ptr, queue);

    queue.enqueue_svm_map(ptr.get(), 4 * sizeof(cl_int), CL_MAP_READ);
    CHECK_HOST_RANGE_EQUAL(
        int_,
        4,
        static_cast<int_*>(ptr.get()),
        (
            static_cast<int_>(4.1f),
            static_cast<int_>(-11.3f),
            static_cast<int_>(219.4f),
            static_cast<int_>(-26.7f)
        )
    );
    queue.enqueue_svm_unmap(ptr.get()).wait();

    compute::svm_free(context, ptr);

    // restore
    parameters->set(cache_key, "map_copy_threshold", map_copy_threshold);
    parameters->set(cache_key, "direct_copy_threshold", direct_copy_threshold);
}

BOOST_AUTO_TEST_CASE(copy_async_to_device_svm_float_to_int)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    using compute::int_;
    using compute::uint_;
    using compute::float_;

    float_ host[] = { 44.1f, -14.3f, 319.4f, -26.7f };
    compute::svm_ptr<int_> ptr = compute::svm_alloc<int_>(context, 4);

    // copy host float data to int device vector
    compute::future<void> future =
        bc::copy_async(host, host + 4, ptr, queue);
    future.wait();

    queue.enqueue_svm_map(ptr.get(), 4 * sizeof(cl_int), CL_MAP_READ);
    CHECK_HOST_RANGE_EQUAL(
        int_,
        4,
        static_cast<int_*>(ptr.get()),
        (
            static_cast<int_>(44.1f),
            static_cast<int_>(-14.3f),
            static_cast<int_>(319.4f),
            static_cast<int_>(-26.7f)
        )
    );
    queue.enqueue_svm_unmap(ptr.get()).wait();

    compute::svm_free(context, ptr);
}
#endif

// DEVICE -> DEVICE

BOOST_AUTO_TEST_CASE(copy_on_device_float_to_int)
{
    using compute::int_;
    using compute::float_;

    float_ data[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    bc::vector<float_> device_fvector(data, data + 4, queue);
    bc::vector<int_> device_ivector(4, context);

    // copy device float vector to device int vector
    bc::copy(
        device_fvector.begin(),
        device_fvector.end(),
        device_ivector.begin(),
        queue
    );

    CHECK_RANGE_EQUAL(
        int_,
        4,
        device_ivector,
        (
            static_cast<int_>(6.1f),
            static_cast<int_>(-10.2f),
            static_cast<int_>(19.3f),
            static_cast<int_>(25.4f)
        )
    );
}

BOOST_AUTO_TEST_CASE(copy_async_on_device_float_to_int)
{
    using compute::int_;
    using compute::float_;

    float_ data[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    bc::vector<float_> device_fvector(data, data + 4, queue);
    bc::vector<int_> device_ivector(4, context);

    // copy device float vector to device int vector
    compute::future<void> future =
        bc::copy_async(
            device_fvector.begin(),
            device_fvector.end(),
            device_ivector.begin(),
            queue
        );
    future.wait();

    CHECK_RANGE_EQUAL(
        int_,
        4,
        device_ivector,
        (
            static_cast<int_>(6.1f),
            static_cast<int_>(-10.2f),
            static_cast<int_>(19.3f),
            static_cast<int_>(25.4f)
        )
    );
}

BOOST_AUTO_TEST_CASE(copy_async_on_device_float_to_int_empty)
{
    using compute::int_;
    using compute::float_;

    float_ data[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    bc::vector<float_> device_fvector(data, data + 4, queue);
    bc::vector<int_> device_ivector(size_t(4), int_(1), queue);

    // copy device float vector to device int vector
    compute::future<void> future =
        bc::copy_async(
            device_fvector.begin(),
            device_fvector.begin(),
            device_ivector.begin(),
            queue
        );
    if(future.valid()) {
        future.wait();
    }

    CHECK_RANGE_EQUAL(
        int_,
        4,
        device_ivector,
        (
            int_(1),
            int_(1),
            int_(1),
            int_(1)
        )
    );
}

// SVM requires OpenCL 2.0
#if defined(BOOST_COMPUTE_CL_VERSION_2_0) || defined(BOOST_COMPUTE_DOXYGEN_INVOKED)
BOOST_AUTO_TEST_CASE(copy_on_device_buffer_to_svm_float_to_int)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    using compute::int_;
    using compute::float_;

    float_ data[] = { 65.1f, -110.2f, -19.3f, 26.7f };
    bc::vector<float_> device_vector(data, data + 4, queue);
    compute::svm_ptr<int_> ptr = compute::svm_alloc<int_>(context, 4);

    // copy host float data to int svm memory
    bc::copy(device_vector.begin(), device_vector.end(), ptr, queue);

    queue.enqueue_svm_map(ptr.get(), 4 * sizeof(cl_int), CL_MAP_READ);
    CHECK_HOST_RANGE_EQUAL(
        int_,
        4,
        static_cast<int_*>(ptr.get()),
        (
            static_cast<int_>(65.1f),
            static_cast<int_>(-110.2f),
            static_cast<int_>(-19.3f),
            static_cast<int_>(26.7f)
        )
    );
    queue.enqueue_svm_unmap(ptr.get()).wait();

    compute::svm_free(context, ptr);
}

BOOST_AUTO_TEST_CASE(copy_on_device_svm_to_buffer_float_to_int)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    using compute::int_;
    using compute::float_;

    float_ data[] = { 6.1f, 11.2f, 19.3f, 6.7f };
    bc::vector<int_> device_vector(4, context);
    compute::svm_ptr<float_> ptr = compute::svm_alloc<float_>(context, 4);

    queue.enqueue_svm_map(ptr.get(), 4 * sizeof(cl_int), CL_MAP_WRITE);
    for(size_t i = 0; i < 4; i++) {
        static_cast<float_*>(ptr.get())[i] = data[i];
    }
    queue.enqueue_svm_unmap(ptr.get()).wait();

    // copy host float svm data to int device vector
    bc::copy(ptr, ptr + 4, device_vector.begin(), queue);

    CHECK_RANGE_EQUAL(
        int_,
        4,
        device_vector,
        (
            static_cast<int_>(6.1f),
            static_cast<int_>(11.2f),
            static_cast<int_>(19.3f),
            static_cast<int_>(6.7f)
        )
    );

    compute::svm_free(context, ptr);
}

BOOST_AUTO_TEST_CASE(copy_on_device_svm_to_svm_float_to_int)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    using compute::int_;
    using compute::float_;

    float_ data[] = { 0.1f, -10.2f, -1.3f, 2.7f };
    compute::svm_ptr<float_> ptr = compute::svm_alloc<float_>(context, 4);
    compute::svm_ptr<int_> ptr2 = compute::svm_alloc<int_>(context, 4);

    queue.enqueue_svm_map(ptr.get(), 4 * sizeof(cl_int), CL_MAP_WRITE);
    for(size_t i = 0; i < 4; i++) {
        static_cast<float_*>(ptr.get())[i] = data[i];
    }
    queue.enqueue_svm_unmap(ptr.get()).wait();

    // copy host float svm to int svm
    bc::copy(ptr, ptr + 4, ptr2, queue);

    queue.enqueue_svm_map(ptr2.get(), 4 * sizeof(cl_int), CL_MAP_READ);
    CHECK_HOST_RANGE_EQUAL(
        int_,
        4,
        static_cast<int_*>(ptr2.get()),
        (
            static_cast<int_>(0.1f),
            static_cast<int_>(-10.2f),
            static_cast<int_>(-1.3f),
            static_cast<int_>(2.7f)
        )
    );
    queue.enqueue_svm_unmap(ptr2.get()).wait();

    compute::svm_free(context, ptr);
    compute::svm_free(context, ptr2);
}

BOOST_AUTO_TEST_CASE(copy_async_on_device_buffer_to_svm_float_to_int)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    using compute::int_;
    using compute::float_;

    float_ data[] = { 65.1f, -110.2f, -19.3f, 26.7f };
    bc::vector<float_> device_vector(data, data + 4, queue);
    compute::svm_ptr<int_> ptr = compute::svm_alloc<int_>(context, 4);

    // copy host float data to int svm memory
    compute::future<bc::svm_ptr<int_> > future =
        bc::copy_async(device_vector.begin(), device_vector.end(), ptr, queue);
    future.wait();

    queue.enqueue_svm_map(ptr.get(), 4 * sizeof(cl_int), CL_MAP_READ);
    CHECK_HOST_RANGE_EQUAL(
        int_,
        4,
        static_cast<int_*>(ptr.get()),
        (
            static_cast<int_>(65.1f),
            static_cast<int_>(-110.2f),
            static_cast<int_>(-19.3f),
            static_cast<int_>(26.7f)
        )
    );
    queue.enqueue_svm_unmap(ptr.get()).wait();

    compute::svm_free(context, ptr);
}

BOOST_AUTO_TEST_CASE(copy_async_on_device_svm_to_buffer_float_to_int)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    using compute::int_;
    using compute::float_;

    float_ data[] = { 65.1f, -110.2f, -19.3f, 26.7f };
    bc::vector<int_> device_vector(4, context);
    compute::svm_ptr<float_> ptr = compute::svm_alloc<float_>(context, 4);

    queue.enqueue_svm_map(ptr.get(), 4 * sizeof(cl_int), CL_MAP_WRITE);
    for(size_t i = 0; i < 4; i++) {
        static_cast<float_*>(ptr.get())[i] = data[i];
    }
    queue.enqueue_svm_unmap(ptr.get()).wait();

    // copy host float svm data to int device vector
    compute::future<bc::vector<int_>::iterator > future =
        bc::copy_async(ptr, ptr + 4, device_vector.begin(), queue);
    future.wait();

    CHECK_RANGE_EQUAL(
        int_,
        4,
        device_vector,
        (
            static_cast<int_>(65.1f),
            static_cast<int_>(-110.2f),
            static_cast<int_>(-19.3f),
            static_cast<int_>(26.7f)
        )
    );

    compute::svm_free(context, ptr);
}

BOOST_AUTO_TEST_CASE(copy_async_on_device_svm_to_svm_float_to_int)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    using compute::int_;
    using compute::float_;

    float_ data[] = { 0.1f, -10.2f, -1.3f, 2.7f };
    compute::svm_ptr<float_> ptr = compute::svm_alloc<float_>(context, 4);
    compute::svm_ptr<int_> ptr2 = compute::svm_alloc<int_>(context, 4);

    queue.enqueue_svm_map(ptr.get(), 4 * sizeof(cl_int), CL_MAP_WRITE);
    for(size_t i = 0; i < 4; i++) {
        static_cast<float_*>(ptr.get())[i] = data[i];
    }
    queue.enqueue_svm_unmap(ptr.get()).wait();

    // copy host float svm to int svm
    compute::future<bc::svm_ptr<int_> > future =
        bc::copy_async(ptr, ptr + 4, ptr2, queue);
    future.wait();

    queue.enqueue_svm_map(ptr2.get(), 4 * sizeof(cl_int), CL_MAP_READ);
    CHECK_HOST_RANGE_EQUAL(
        int_,
        4,
        static_cast<int_*>(ptr2.get()),
        (
            static_cast<int_>(0.1f),
            static_cast<int_>(-10.2f),
            static_cast<int_>(-1.3f),
            static_cast<int_>(2.7f)
        )
    );
    queue.enqueue_svm_unmap(ptr2.get()).wait();

    compute::svm_free(context, ptr);
    compute::svm_free(context, ptr2);
}
#endif

// DEVICE -> HOST

BOOST_AUTO_TEST_CASE(copy_to_host_float_to_int)
{
    using compute::int_;
    using compute::float_;

    float_ data[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    bc::vector<float_> device_vector(data, data + 4, queue);

    std::vector<int_> host_vector(4);
    // copy device float vector to int host vector
    bc::copy(device_vector.begin(), device_vector.end(), host_vector.begin(), queue);
    CHECK_HOST_RANGE_EQUAL(
        int_,
        4,
        host_vector.begin(),
        (
            static_cast<int_>(6.1f),
            static_cast<int_>(-10.2f),
            static_cast<int_>(19.3f),
            static_cast<int_>(25.4f)
        )
    );
}

BOOST_AUTO_TEST_CASE(copy_to_host_float_to_int_map)
{
    using compute::int_;
    using compute::uint_;
    using compute::float_;

    std::string cache_key =
        std::string("__boost_compute_copy_to_host_float_int");
    boost::shared_ptr<bc::detail::parameter_cache> parameters =
        bc::detail::parameter_cache::get_global_cache(device);

    // save
    uint_ map_copy_threshold =
        parameters->get(cache_key, "map_copy_threshold", 0);

    // force copy_to_host_map (mapping device vector to the host)
    parameters->set(cache_key, "map_copy_threshold", 1024);

    float_ data[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    bc::vector<float_> device_vector(data, data + 4, queue);

    std::vector<int_> host_vector(4);
    // copy device float vector to int host vector
    bc::copy(device_vector.begin(), device_vector.end(), host_vector.begin(), queue);
    CHECK_HOST_RANGE_EQUAL(
        int_,
        4,
        host_vector.begin(),
        (
            static_cast<int_>(6.1f),
            static_cast<int_>(-10.2f),
            static_cast<int_>(19.3f),
            static_cast<int_>(25.4f)
        )
    );

    // restore
    parameters->set(cache_key, "map_copy_threshold", map_copy_threshold);
}

BOOST_AUTO_TEST_CASE(copy_to_host_float_to_int_convert_on_host)
{
    using compute::int_;
    using compute::uint_;
    using compute::float_;

    std::string cache_key =
        std::string("__boost_compute_copy_to_host_float_int");
    boost::shared_ptr<bc::detail::parameter_cache> parameters =
        bc::detail::parameter_cache::get_global_cache(device);

    // save
    uint_ map_copy_threshold =
        parameters->get(cache_key, "map_copy_threshold", 0);
    uint_ direct_copy_threshold =
        parameters->get(cache_key, "direct_copy_threshold", 0);

    // force copying by copying input device vector to temporary
    // host vector of the same type and then copying from that temporary
    // vector to result using std::copy()
    parameters->set(cache_key, "map_copy_threshold", 0);
    parameters->set(cache_key, "direct_copy_threshold", 1024);

    float_ data[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    bc::vector<float_> device_vector(data, data + 4, queue);

    std::vector<int_> host_vector(4);
    // copy device float vector to int host vector
    bc::copy(device_vector.begin(), device_vector.end(), host_vector.begin(), queue);
    CHECK_HOST_RANGE_EQUAL(
        int_,
        4,
        host_vector.begin(),
        (
            static_cast<int_>(6.1f),
            static_cast<int_>(-10.2f),
            static_cast<int_>(19.3f),
            static_cast<int_>(25.4f)
        )
    );

    // restore
    parameters->set(cache_key, "map_copy_threshold", map_copy_threshold);
    parameters->set(cache_key, "direct_copy_threshold", direct_copy_threshold);
}

BOOST_AUTO_TEST_CASE(copy_to_host_float_to_int_convert_on_device)
{
    using compute::int_;
    using compute::uint_;
    using compute::float_;

    std::string cache_key =
        std::string("__boost_compute_copy_to_host_float_int");
    boost::shared_ptr<bc::detail::parameter_cache> parameters =
        bc::detail::parameter_cache::get_global_cache(device);

    // save
    uint_ map_copy_threshold =
        parameters->get(cache_key, "map_copy_threshold", 0);
    uint_ direct_copy_threshold =
        parameters->get(cache_key, "direct_copy_threshold", 0);

    // force copying by mapping output data to the device memory
    // and using transform operation for casting & copying
    parameters->set(cache_key, "map_copy_threshold", 0);
    parameters->set(cache_key, "direct_copy_threshold", 0);

    float_ data[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    bc::vector<float_> device_vector(data, data + 4, queue);

    std::vector<int_> host_vector(4);
    // copy device float vector to int host vector
    bc::copy(device_vector.begin(), device_vector.end(), host_vector.begin(), queue);
    CHECK_HOST_RANGE_EQUAL(
        int_,
        4,
        host_vector.begin(),
        (
            static_cast<int_>(6.1f),
            static_cast<int_>(-10.2f),
            static_cast<int_>(19.3f),
            static_cast<int_>(25.4f)
        )
    );

    // restore
    parameters->set(cache_key, "map_copy_threshold", map_copy_threshold);
    parameters->set(cache_key, "direct_copy_threshold", direct_copy_threshold);
}

// Test copying from a bc::vector to a std::list . This differs from
// the test copying to std::vector because std::list has non-contiguous
// storage for its data values.
BOOST_AUTO_TEST_CASE(copy_to_host_list_float_to_int_map)
{
    using compute::int_;
    using compute::uint_;
    using compute::float_;

    std::string cache_key =
        std::string("__boost_compute_copy_to_host_float_int");
    boost::shared_ptr<bc::detail::parameter_cache> parameters =
        bc::detail::parameter_cache::get_global_cache(device);

    // save
    uint_ map_copy_threshold =
        parameters->get(cache_key, "map_copy_threshold", 0);

    // force copy_to_host_map (mapping device vector to the host)
    parameters->set(cache_key, "map_copy_threshold", 1024);

    float_ data[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    bc::vector<float_> device_vector(data, data + 4, queue);

    std::list<int_> host_list(4);
    // copy device float vector to int host vector
    bc::copy(device_vector.begin(), device_vector.end(), host_list.begin(), queue);

    int_ expected[4] = {
        static_cast<int_>(6.1f),
        static_cast<int_>(-10.2f),
        static_cast<int_>(19.3f),
        static_cast<int_>(25.4f)
    };
    BOOST_CHECK_EQUAL_COLLECTIONS(
        host_list.begin(), host_list.end(),
        expected, expected + 4
    );

    // restore
    parameters->set(cache_key, "map_copy_threshold", map_copy_threshold);
}

// Test copying from a bc::vector to a std::list . This differs from
// the test copying to std::vector because std::list has non-contiguous
// storage for its data values.
BOOST_AUTO_TEST_CASE(copy_to_host_list_float_to_int_covert_on_host)
{
    using compute::int_;
    using compute::uint_;
    using compute::float_;

    std::string cache_key =
        std::string("__boost_compute_copy_to_host_float_int");
    boost::shared_ptr<bc::detail::parameter_cache> parameters =
        bc::detail::parameter_cache::get_global_cache(device);

    // save
    uint_ map_copy_threshold =
        parameters->get(cache_key, "map_copy_threshold", 0);
    uint_ direct_copy_threshold =
        parameters->get(cache_key, "direct_copy_threshold", 0);

    // force copying by copying input device vector to temporary
    // host vector of the same type and then copying from that temporary
    // vector to result using std::copy()
    parameters->set(cache_key, "map_copy_threshold", 0);
    parameters->set(cache_key, "direct_copy_threshold", 1024);

    float_ data[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    bc::vector<float_> device_vector(data, data + 4, queue);

    std::list<int_> host_list(4);
    // copy device float vector to int host vector
    bc::copy(device_vector.begin(), device_vector.end(), host_list.begin(), queue);
    int_ expected[4] = {
        static_cast<int_>(6.1f),
        static_cast<int_>(-10.2f),
        static_cast<int_>(19.3f),
        static_cast<int_>(25.4f)
    };
    BOOST_CHECK_EQUAL_COLLECTIONS(
        host_list.begin(), host_list.end(),
        expected, expected + 4
    );

    // restore
    parameters->set(cache_key, "map_copy_threshold", map_copy_threshold);
    parameters->set(cache_key, "direct_copy_threshold", direct_copy_threshold);
}

BOOST_AUTO_TEST_CASE(copy_async_to_host_float_to_int)
{
    using compute::int_;
    using compute::float_;

    float_ data[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    bc::vector<float_> device_vector(data, data + 4, queue);
    std::vector<int_> host_vector(device_vector.size());

    // copy device float vector to host int vector
    compute::future<void> future =
        bc::copy_async(
            device_vector.begin(),
            device_vector.end(),
            host_vector.begin(),
            queue
        );
    future.wait();

    CHECK_HOST_RANGE_EQUAL(
        int_,
        4,
        host_vector.begin(),
        (
            static_cast<int_>(6.1f),
            static_cast<int_>(-10.2f),
            static_cast<int_>(19.3f),
            static_cast<int_>(25.4f)
        )
    );
}

BOOST_AUTO_TEST_CASE(copy_async_to_host_float_to_int_empty)
{
    using compute::int_;
    using compute::float_;

    float_ data[] = { 6.1f, -10.2f, 19.3f, 25.4f };
    bc::vector<float_> device_vector(data, data + 4, queue);
    std::vector<int_> host_vector(device_vector.size(), int_(1));

    // copy device float vector to host int vector
    compute::future<void> future =
        bc::copy_async(
            device_vector.begin(),
            device_vector.begin(),
            host_vector.begin(),
            queue
        );
    if(future.valid()) {
        future.wait();
    }

    CHECK_HOST_RANGE_EQUAL(
        int_,
        4,
        host_vector.begin(),
        (
            int_(1),
            int_(1),
            int_(1),
            int_(1)
        )
    );
}

// SVM requires OpenCL 2.0
#if defined(BOOST_COMPUTE_CL_VERSION_2_0) || defined(BOOST_COMPUTE_DOXYGEN_INVOKED)
BOOST_AUTO_TEST_CASE(copy_to_host_svm_float_to_int_map)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    using compute::int_;
    using compute::uint_;
    using compute::float_;

    std::string cache_key =
        std::string("__boost_compute_copy_to_host_float_int");
    boost::shared_ptr<bc::detail::parameter_cache> parameters =
        bc::detail::parameter_cache::get_global_cache(device);

    // save
    uint_ map_copy_threshold =
        parameters->get(cache_key, "map_copy_threshold", 0);

    // force copy_to_host_map (mapping device vector to the host)
    parameters->set(cache_key, "map_copy_threshold", 1024);

    float_ data[] = { 6.1f, 1.2f, 1.3f, -66.7f };
    std::vector<int_> host_vector(4, 0);
    compute::svm_ptr<float_> ptr = compute::svm_alloc<float_>(context, 4);

    queue.enqueue_svm_map(ptr.get(), 4 * sizeof(cl_int), CL_MAP_WRITE);
    for(size_t i = 0; i < 4; i++) {
        static_cast<float_*>(ptr.get())[i] = data[i];
    }
    queue.enqueue_svm_unmap(ptr.get()).wait();

    // copy host float svm data to int host vector
    bc::copy(ptr, ptr + 4, host_vector.begin(), queue);

    CHECK_HOST_RANGE_EQUAL(
        int_,
        4,
        host_vector.begin(),
        (
            static_cast<int_>(6.1f),
            static_cast<int_>(1.2f),
            static_cast<int_>(1.3f),
            static_cast<int_>(-66.7f)
        )
    );

    compute::svm_free(context, ptr);

    // restore
    parameters->set(cache_key, "map_copy_threshold", map_copy_threshold);
}

BOOST_AUTO_TEST_CASE(copy_to_host_svm_float_to_int_convert_on_host)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    if(bug_in_svmmemcpy(device)){
        std::cerr << "skipping svmmemcpy test case" << std::endl;
        return;
    }

    using compute::int_;
    using compute::uint_;
    using compute::float_;

    std::string cache_key =
        std::string("__boost_compute_copy_to_host_float_int");
    boost::shared_ptr<bc::detail::parameter_cache> parameters =
        bc::detail::parameter_cache::get_global_cache(device);

    // save
    uint_ map_copy_threshold =
        parameters->get(cache_key, "map_copy_threshold", 0);
    uint_ direct_copy_threshold =
        parameters->get(cache_key, "direct_copy_threshold", 0);

    // force copying by copying input device vector to temporary
    // host vector of the same type and then copying from that temporary
    // vector to result using std::copy()
    parameters->set(cache_key, "map_copy_threshold", 0);
    parameters->set(cache_key, "direct_copy_threshold", 1024);

    float_ data[] = { 6.1f, 1.2f, 1.3f, 766.7f };
    std::vector<int_> host_vector(4, 0);
    compute::svm_ptr<float_> ptr = compute::svm_alloc<float_>(context, 4);

    queue.enqueue_svm_map(ptr.get(), 4 * sizeof(cl_int), CL_MAP_WRITE);
    for(size_t i = 0; i < 4; i++) {
        static_cast<float_*>(ptr.get())[i] = data[i];
    }
    queue.enqueue_svm_unmap(ptr.get()).wait();

    // copy host float svm data to int host vector
    bc::copy(ptr, ptr + 4, host_vector.begin(), queue);

    CHECK_HOST_RANGE_EQUAL(
        int_,
        4,
        host_vector.begin(),
        (
            static_cast<int_>(6.1f),
            static_cast<int_>(1.2f),
            static_cast<int_>(1.3f),
            static_cast<int_>(766.7f)
        )
    );

    compute::svm_free(context, ptr);

    // restore
    parameters->set(cache_key, "map_copy_threshold", map_copy_threshold);
    parameters->set(cache_key, "direct_copy_threshold", direct_copy_threshold);
}

BOOST_AUTO_TEST_CASE(copy_to_host_svm_float_to_int_transform)
{
    REQUIRES_OPENCL_VERSION(2, 0);

    using compute::int_;
    using compute::uint_;
    using compute::float_;

    std::string cache_key =
        std::string("__boost_compute_copy_to_host_float_int");
    boost::shared_ptr<bc::detail::parameter_cache> parameters =
        bc::detail::parameter_cache::get_global_cache(device);

    // save
    uint_ map_copy_threshold =
        parameters->get(cache_key, "map_copy_threshold", 0);
    uint_ direct_copy_threshold =
        parameters->get(cache_key, "direct_copy_threshold", 0);

    // force copying by copying input device vector to temporary
    // host vector of the same type and then copying from that temporary
    // vector to result using std::copy()
    parameters->set(cache_key, "map_copy_threshold", 0);
    parameters->set(cache_key, "direct_copy_threshold", 0);

    float_ data[] = { 0.1f, 11.2f, 1.3f, -66.7f };
    std::vector<int_> host_vector(4, 0);
    compute::svm_ptr<float_> ptr = compute::svm_alloc<float_>(context, 4);

    queue.enqueue_svm_map(ptr.get(), 4 * sizeof(cl_int), CL_MAP_WRITE);
    for(size_t i = 0; i < 4; i++) {
        static_cast<float_*>(ptr.get())[i] = data[i];
    }
    queue.enqueue_svm_unmap(ptr.get()).wait();

    // copy host float svm data to int host vector
    bc::copy(ptr, ptr + 4, host_vector.begin(), queue);

    CHECK_HOST_RANGE_EQUAL(
        int_,
        4,
        host_vector.begin(),
        (
            static_cast<int_>(0.1f),
            static_cast<int_>(11.2f),
            static_cast<int_>(1.3f),
            static_cast<int_>(-66.7f)
        )
    );

    compute::svm_free(context, ptr);

    // restore
    parameters->set(cache_key, "map_copy_threshold", map_copy_threshold);
    parameters->set(cache_key, "direct_copy_threshold", direct_copy_threshold);
}
#endif

BOOST_AUTO_TEST_SUITE_END()
