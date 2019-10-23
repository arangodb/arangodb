//---------------------------------------------------------------------------//
// Copyright (c) 2016 Jakub Szuppe <j.szuppe@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestMergeSortOnGPU
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/is_sorted.hpp>
#include <boost/compute/algorithm/detail/merge_sort_on_gpu.hpp>
#include <boost/compute/container/vector.hpp>

#include "quirks.hpp"
#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

BOOST_AUTO_TEST_CASE(sort_small_vector_char)
{
    if(is_apple_cpu_device(device)) {
        std::cerr
            << "skipping all merge_sort_on_gpu tests due to Apple platform"
            << " behavior when local memory is used on a CPU device"
            << std::endl;
        return;
    }

    using boost::compute::char_;
    ::boost::compute::greater<char_> greater;
    ::boost::compute::less<char_> less;

    char_ data[] = { 'c', 'a', '0', '7', 'B', 'F', '\0', '$' };
    boost::compute::vector<char_> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == false);

    // <
    boost::compute::detail::merge_sort_on_gpu(
        vector.begin(), vector.end(), less, queue
    );
    BOOST_CHECK(
        boost::compute::is_sorted(vector.begin(), vector.end(), less, queue)
    );
    CHECK_RANGE_EQUAL(char_, 8, vector, ('\0', '$', '0', '7', 'B', 'F', 'a', 'c'));

    // >
    boost::compute::detail::merge_sort_on_gpu(
        vector.begin(), vector.end(), greater, queue
    );
    BOOST_CHECK(
        boost::compute::is_sorted(vector.begin(), vector.end(), greater, queue)
    );
    CHECK_RANGE_EQUAL(char_, 8, vector, ('c', 'a', 'F', 'B', '7', '0', '$', '\0'));
}

BOOST_AUTO_TEST_CASE(sort_mid_vector_int)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::int_;
    ::boost::compute::greater<int_> greater;
    ::boost::compute::less<int_> less;

    const int_ size = 748;
    std::vector<int_> data(size);
    for(int_ i = 0; i < size; i++){
        data[i] = i%2 ? i : -i;
    }

    boost::compute::vector<int_> vector(data.begin(), data.end(), queue);
    BOOST_CHECK_EQUAL(vector.size(), size);
    BOOST_CHECK(!boost::compute::is_sorted(vector.begin(), vector.end(), queue));

    // <
    boost::compute::detail::merge_sort_on_gpu(
        vector.begin(), vector.end(), less, queue
    );
    BOOST_CHECK(
        boost::compute::is_sorted(vector.begin(), vector.end(), less, queue)
    );

    // >
    boost::compute::detail::merge_sort_on_gpu(
        vector.begin(), vector.end(), greater, queue
    );
    BOOST_CHECK(
        boost::compute::is_sorted(vector.begin(), vector.end(), greater, queue)
    );
}

BOOST_AUTO_TEST_CASE(sort_mid_vector_ulong)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::ulong_;
    ::boost::compute::greater<ulong_> greater;
    ::boost::compute::less<ulong_> less;

    const ulong_ size = 260;
    std::vector<ulong_> data(size);
    for(ulong_ i = 0; i < size; i++){
        data[i] = i%2 ? i : i * i;
    }

    boost::compute::vector<ulong_> vector(data.begin(), data.end(), queue);
    BOOST_CHECK_EQUAL(vector.size(), size);
    BOOST_CHECK(!boost::compute::is_sorted(vector.begin(), vector.end(), queue));

    // <
    boost::compute::detail::merge_sort_on_gpu(
        vector.begin(), vector.end(), less, queue
    );
    BOOST_CHECK(
        boost::compute::is_sorted(vector.begin(), vector.end(), less, queue)
    );

    // >
    boost::compute::detail::merge_sort_on_gpu(
        vector.begin(), vector.end(), greater, queue
    );
    BOOST_CHECK(
        boost::compute::is_sorted(vector.begin(), vector.end(), greater, queue)
    );
}


BOOST_AUTO_TEST_CASE(sort_mid_vector_float)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::float_;
    ::boost::compute::greater<float_> greater;
    ::boost::compute::less<float_> less;

    const int size = 513;
    std::vector<float_> data(size);
    for(int i = 0; i < size; i++){
        data[i] = float_(i%2 ? i : -i);
    }

    boost::compute::vector<float_> vector(data.begin(), data.end(), queue);
    BOOST_CHECK_EQUAL(vector.size(), size);
    BOOST_CHECK(!boost::compute::is_sorted(vector.begin(), vector.end(), queue));

    // <
    boost::compute::detail::merge_sort_on_gpu(
        vector.begin(), vector.end(), less, queue
    );
    BOOST_CHECK(
        boost::compute::is_sorted(vector.begin(), vector.end(), less, queue)
    );

    // >
    boost::compute::detail::merge_sort_on_gpu(
        vector.begin(), vector.end(), greater, queue
    );
    queue.finish();

    BOOST_CHECK(
        boost::compute::is_sorted(vector.begin(), vector.end(), greater, queue)
    );
}

BOOST_AUTO_TEST_CASE(sort_mid_vector_double)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    if(!device.supports_extension("cl_khr_fp64")){
        std::cout << "skipping test: device does not support double" << std::endl;
        return;
    }

    using boost::compute::double_;
    ::boost::compute::greater<double_> greater;
    ::boost::compute::less<double_> less;

    const int size = 1023;
    std::vector<double_> data(size);
    for(int i = 0; i < size; i++){
        data[i] = double_(i%2 ? i : -i);
    }

    boost::compute::vector<double_> vector(data.begin(), data.end(), queue);
    BOOST_CHECK_EQUAL(vector.size(), size);
    BOOST_CHECK(!boost::compute::is_sorted(vector.begin(), vector.end(), queue));

    // <
    boost::compute::detail::merge_sort_on_gpu(
        vector.begin(), vector.end(), less, queue
    );
    BOOST_CHECK(
        boost::compute::is_sorted(vector.begin(), vector.end(), less, queue)
    );

    // >
    boost::compute::detail::merge_sort_on_gpu(
        vector.begin(), vector.end(), greater, queue
    );
    queue.finish();

    BOOST_CHECK(
        boost::compute::is_sorted(vector.begin(), vector.end(), greater, queue)
    );
}

BOOST_AUTO_TEST_CASE(sort_mid_vector_int_custom_comparison_func)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::int_;
    ::boost::compute::greater<int_> greater;
    ::boost::compute::less<int_> less;

    const int_ size = 1024;
    std::vector<int_> data(size);
    for(int_ i = 0; i < size; i++){
        data[i] = i%2 ? size - i : i - size;
    }

    BOOST_COMPUTE_FUNCTION(bool, abs_sort, (int_ a, int_ b),
    {
        return abs(a) < abs(b);
    });

    boost::compute::vector<int_> vector(data.begin(), data.end(), queue);
    BOOST_CHECK_EQUAL(vector.size(), size);
    BOOST_CHECK(
        !boost::compute::is_sorted(vector.begin(), vector.end(), abs_sort, queue)
    );

    boost::compute::detail::merge_sort_on_gpu(
        vector.begin(), vector.end(), abs_sort, queue
    );
    BOOST_CHECK(
        boost::compute::is_sorted(vector.begin(), vector.end(), abs_sort, queue)
    );
}

BOOST_AUTO_TEST_CASE(sort_mid_vector_int2)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::int2_;
    using boost::compute::int_;
    ::boost::compute::greater<int2_> greater;
    ::boost::compute::less<int2_> less;

    const int_ size = 1024;
    std::vector<int2_> data(size);
    for(int_ i = 0; i < size; i++){
        data[i] = i%2 ? int2_(i, i) : int2_(i - size, i - size);
    }

    BOOST_COMPUTE_FUNCTION(bool, abs_sort, (int2_ a, int2_ b),
    {
        return abs(a.x + a.y) < abs(b.x + b.y);
    });

    boost::compute::vector<int2_> vector(data.begin(), data.end(), queue);
    BOOST_CHECK_EQUAL(vector.size(), size);
    BOOST_CHECK(
        !boost::compute::is_sorted(vector.begin(), vector.end(), abs_sort, queue)
    );

    boost::compute::detail::merge_sort_on_gpu(
        vector.begin(), vector.end(), abs_sort, queue
    );
    BOOST_CHECK(
        boost::compute::is_sorted(vector.begin(), vector.end(), abs_sort, queue)
    );
}

BOOST_AUTO_TEST_CASE(sort_mid_vector_long8)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::long8_;
    using boost::compute::long_;
    ::boost::compute::greater<long8_> greater;
    ::boost::compute::less<long8_> less;

    const long_ size = 256;
    std::vector<long8_> data(size);
    for(long_ i = 0; i < size; i++){
        data[i] = i%2 ? long8_(i) : long8_(i * i);
    }

    BOOST_COMPUTE_FUNCTION(bool, comp, (long8_ a, long8_ b),
    {
        return a.s0 < b.s3;
    });

    boost::compute::vector<long8_> vector(data.begin(), data.end(), queue);
    BOOST_CHECK_EQUAL(vector.size(), size);
    BOOST_CHECK(
        !boost::compute::is_sorted(vector.begin(), vector.end(), comp, queue)
    );

    boost::compute::detail::merge_sort_on_gpu(
        vector.begin(), vector.end(), comp, queue
    );
    BOOST_CHECK(
        boost::compute::is_sorted(vector.begin(), vector.end(), comp, queue)
    );
}

BOOST_AUTO_TEST_CASE(stable_sort_vector_int2)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::int2_;

    int2_ data[] = {
        int2_(8, 3), int2_(5, 1),
        int2_(2, 1), int2_(6, 1),
        int2_(8, 1), int2_(7, 1),
        int2_(4, 1), int2_(8, 2)
    };

    BOOST_COMPUTE_FUNCTION(bool, comp, (int2_ a, int2_ b),
    {
        return a.x < b.x;
    });

    boost::compute::vector<int2_> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), 8);
    BOOST_CHECK(
        !boost::compute::is_sorted(vector.begin(), vector.end(), comp, queue)
    );

    //
    boost::compute::detail::merge_sort_on_gpu(
        vector.begin(), vector.end(), comp, true /*stable*/, queue
    );
    BOOST_CHECK(
        boost::compute::is_sorted(vector.begin(), vector.end(), comp, queue)
    );
    CHECK_RANGE_EQUAL(
        int2_, 8, vector,
        (
            int2_(2, 1), int2_(4, 1),
            int2_(5, 1), int2_(6, 1),
            int2_(7, 1), int2_(8, 3),
            int2_(8, 1), int2_(8, 2)
        )
    );
}

BOOST_AUTO_TEST_SUITE_END()
