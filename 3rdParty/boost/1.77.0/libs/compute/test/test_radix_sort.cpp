//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestRadixSort
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/is_sorted.hpp>
#include <boost/compute/algorithm/detail/radix_sort.hpp>
#include <boost/compute/container/vector.hpp>

#include "quirks.hpp"
#include "check_macros.hpp"
#include "context_setup.hpp"

namespace bc = boost::compute;

const bool descending = false;

BOOST_AUTO_TEST_CASE(sort_char_vector)
{
    if(is_apple_cpu_device(device)) {
        std::cerr
            << "skipping all radix_sort tests due to Apple platform"
            << " behavior when local memory is used on a CPU device"
            << std::endl;
        return;
    }

    using boost::compute::char_;

    char_ data[] = { 'c', 'a', '0', '7', 'B', 'F', '\0', '$' };
    boost::compute::vector<char_> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == false);

    boost::compute::detail::radix_sort(vector.begin(), vector.end(), queue);
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == true);
    CHECK_RANGE_EQUAL(char_, 8, vector, ('\0', '$', '0', '7', 'B', 'F', 'a', 'c'));
}

BOOST_AUTO_TEST_CASE(sort_uchar_vector)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::uchar_;

    uchar_ data[] = { 0x12, 0x00, 0xFF, 0xB4, 0x80, 0x32, 0x64, 0xA2 };
    boost::compute::vector<uchar_> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == false);

    boost::compute::detail::radix_sort(vector.begin(), vector.end(), queue);
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == true);
    CHECK_RANGE_EQUAL(uchar_, 8, vector, (0x00, 0x12, 0x32, 0x64, 0x80, 0xA2, 0xB4, 0xFF));
}

BOOST_AUTO_TEST_CASE(sort_short_vector)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::short_;

    short_ data[] = { -4, 152, -94, 963, 31002, -456, 0, -2113 };
    boost::compute::vector<short_> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == false);

    boost::compute::detail::radix_sort(vector.begin(), vector.end(), queue);
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == true);
    CHECK_RANGE_EQUAL(short_, 8, vector, (-2113, -456, -94, -4, 0, 152, 963, 31002));
}

BOOST_AUTO_TEST_CASE(sort_ushort_vector)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::ushort_;

    ushort_ data[] = { 4, 152, 94, 963, 63202, 34560, 0, 2113 };
    boost::compute::vector<ushort_> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == false);

    boost::compute::detail::radix_sort(vector.begin(), vector.end(), queue);
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == true);
    CHECK_RANGE_EQUAL(ushort_, 8, vector, (0, 4, 94, 152, 963, 2113, 34560, 63202));
}

BOOST_AUTO_TEST_CASE(sort_int_vector)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    int data[] = { -4, 152, -5000, 963, 75321, -456, 0, 1112 };
    boost::compute::vector<int> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == false);

    boost::compute::detail::radix_sort(vector.begin(), vector.end(), queue);
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == true);
    CHECK_RANGE_EQUAL(int, 8, vector, (-5000, -456, -4, 0, 152, 963, 1112, 75321));
}

BOOST_AUTO_TEST_CASE(sort_uint_vector)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::uint_;

    uint_ data[] = { 500, 1988, 123456, 562, 0, 4000000, 9852, 102030 };
    boost::compute::vector<uint_> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == false);

    boost::compute::detail::radix_sort(vector.begin(), vector.end(), queue);
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == true);
    CHECK_RANGE_EQUAL(uint_, 8, vector, (0, 500, 562, 1988, 9852, 102030, 123456, 4000000));
}

BOOST_AUTO_TEST_CASE(sort_long_vector)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::long_;

    long_ data[] = { 500, 1988, 123456, 562, 0, 4000000, 9852, 102030 };
    boost::compute::vector<long_> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == false);

    boost::compute::detail::radix_sort(vector.begin(), vector.end(), queue);
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == true);
    CHECK_RANGE_EQUAL(long_, 8, vector, (0, 500, 562, 1988, 9852, 102030, 123456, 4000000));
}

BOOST_AUTO_TEST_CASE(sort_ulong_vector)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::ulong_;

    ulong_ data[] = { 500, 1988, 123456, 562, 0, 4000000, 9852, 102030 };
    boost::compute::vector<ulong_> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == false);

    boost::compute::detail::radix_sort(vector.begin(), vector.end(), queue);
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == true);
    CHECK_RANGE_EQUAL(ulong_, 8, vector, (0, 500, 562, 1988, 9852, 102030, 123456, 4000000));
}

BOOST_AUTO_TEST_CASE(sort_float_vector)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    float data[] = { -6023.0f, 152.5f, -63.0f, 1234567.0f, 11.2f,
                     -5000.1f, 0.0f, 14.0f, -8.25f, -0.0f };
    boost::compute::vector<float> vector(data, data + 10, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(10));
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == false);

    boost::compute::detail::radix_sort(vector.begin(), vector.end(), queue);
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == true);
    CHECK_RANGE_EQUAL(
        float, 10, vector,
        (-6023.0f, -5000.1f, -63.0f, -8.25f, -0.0f, 0.0f, 11.2f, 14.0f, 152.5f, 1234567.0f)
    );

    // copy data, sort, and check again (to check program caching)
    boost::compute::copy(data, data + 10, vector.begin(), queue);
    boost::compute::detail::radix_sort(vector.begin(), vector.end(), queue);
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == true);
    CHECK_RANGE_EQUAL(
        float, 10, vector,
        (-6023.0f, -5000.1f, -63.0f, -8.25f, -0.0f, 0.0f, 11.2f, 14.0f, 152.5f, 1234567.0f)
    );
}

BOOST_AUTO_TEST_CASE(sort_double_vector)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    if(!device.supports_extension("cl_khr_fp64")){
        std::cout << "skipping test: device does not support double" << std::endl;
        return;
    }

    double data[] = { -6023.0, 152.5, -63.0, 1234567.0, 11.2,
                     -5000.1, 0.0, 14.0, -8.25, -0.0 };
    boost::compute::vector<double> vector(data, data + 10, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(10));
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == false);

    boost::compute::detail::radix_sort(vector.begin(), vector.end(), queue);
    BOOST_CHECK(boost::compute::is_sorted(vector.begin(), vector.end(), queue) == true);
    CHECK_RANGE_EQUAL(
        double, 10, vector,
        (-6023.0, -5000.1, -63.0, -8.25, -0.0, 0.0, 11.2, 14.0, 152.5, 1234567.0)
    );
}

BOOST_AUTO_TEST_CASE(sort_char_vector_desc)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::char_;

    char_ data[] = { 'c', 'a', '0', '7', 'B', 'F', '\0', '$' };
    boost::compute::vector<char_> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));

    BOOST_CHECK(!boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<char_>(), queue
    ));
    boost::compute::detail::radix_sort(
        vector.begin(), vector.end(), descending, queue
    );
    BOOST_CHECK(boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<char_>(), queue
    ));

    CHECK_RANGE_EQUAL(
        char_, 8, vector,
        ('c', 'a', 'F', 'B', '7', '0', '$', '\0')
    );
}

BOOST_AUTO_TEST_CASE(sort_uchar_vector_desc)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::uchar_;

    uchar_ data[] = { 0x12, 0x00, 0xFF, 0xB4, 0x80, 0x32, 0x64, 0xA2 };
    boost::compute::vector<uchar_> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));

    BOOST_CHECK(!boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<uchar_>(), queue
    ));
    boost::compute::detail::radix_sort(
        vector.begin(), vector.end(), descending, queue
    );
    BOOST_CHECK(boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<uchar_>(), queue
    ));

    CHECK_RANGE_EQUAL(
        uchar_, 8, vector,
        (0xFF, 0xB4, 0xA2, 0x80, 0x64, 0x32, 0x12, 0x00)
    );
}

BOOST_AUTO_TEST_CASE(sort_short_vector_desc)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::short_;

    short_ data[] = { -4, 152, -94, 963, 31002, -456, 0, -2113 };
    boost::compute::vector<short_> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));

    BOOST_CHECK(!boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<short_>(), queue
    ));
    boost::compute::detail::radix_sort(
        vector.begin(), vector.end(), descending, queue
    );
    BOOST_CHECK(boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<short_>(), queue
    ));

    CHECK_RANGE_EQUAL(
        short_, 8, vector,
        (31002, 963, 152, 0, -4, -94, -456, -2113)
    );
}

BOOST_AUTO_TEST_CASE(sort_ushort_vector_desc)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::ushort_;

    ushort_ data[] = { 4, 152, 94, 963, 63202, 34560, 0, 2113 };
    boost::compute::vector<ushort_> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));

    BOOST_CHECK(!boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<ushort_>(), queue
    ));
    boost::compute::detail::radix_sort(
        vector.begin(), vector.end(), descending, queue
    );
    BOOST_CHECK(boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<ushort_>(), queue
    ));

    CHECK_RANGE_EQUAL(
        ushort_, 8, vector,
        (63202, 34560, 2113, 963, 152, 94, 4, 0)
    );
}

BOOST_AUTO_TEST_CASE(sort_int_vector_desc)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::int_;

    int_ data[] = { -4, 152, -5000, 963, 75321, -456, 0, 1112 };
    boost::compute::vector<int_> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));

    BOOST_CHECK(!boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<int_>(), queue
    ));
    boost::compute::detail::radix_sort(
        vector.begin(), vector.end(), descending, queue
    );
    BOOST_CHECK(boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<int_>(), queue
    ));

    CHECK_RANGE_EQUAL(
        int_, 8, vector,
        (75321, 1112, 963, 152, 0, -4, -456, -5000)
    );
}

BOOST_AUTO_TEST_CASE(sort_uint_vector_desc)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::uint_;

    uint_ data[] = { 500, 1988, 123456, 562, 0, 4000000, 9852, 102030 };
    boost::compute::vector<uint_> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));

    BOOST_CHECK(!boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<uint_>(), queue
    ));
    boost::compute::detail::radix_sort(
        vector.begin(), vector.end(), descending, queue
    );
    BOOST_CHECK(boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<uint_>(), queue
    ));

    CHECK_RANGE_EQUAL(
        uint_, 8, vector,
        (4000000, 123456, 102030, 9852, 1988, 562, 500, 0)
    );
}

BOOST_AUTO_TEST_CASE(sort_long_vector_desc)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::long_;

    long_ data[] = { -500, 1988, 123456, 562, 0, 4000000, 9852, 102030 };
    boost::compute::vector<long_> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));

    BOOST_CHECK(!boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<long_>(), queue
    ));
    boost::compute::detail::radix_sort(
        vector.begin(), vector.end(), descending, queue
    );
    BOOST_CHECK(boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<long_>(), queue
    ));

    CHECK_RANGE_EQUAL(
        long_, 8, vector,
        (4000000, 123456, 102030, 9852, 1988, 562, 0, -500)
    );
}

BOOST_AUTO_TEST_CASE(sort_ulong_vector_desc)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    using boost::compute::ulong_;

    ulong_ data[] = { 500, 1988, 123456, 562, 0, 4000000, 9852, 102030 };
    boost::compute::vector<ulong_> vector(data, data + 8, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(8));

    BOOST_CHECK(!boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<ulong_>(), queue
    ));
    boost::compute::detail::radix_sort(
        vector.begin(), vector.end(), descending, queue
    );
    BOOST_CHECK(boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<ulong_>(), queue
    ));

    CHECK_RANGE_EQUAL(
        ulong_, 8, vector,
        (4000000, 123456, 102030, 9852, 1988, 562, 500, 0)
    );
}

BOOST_AUTO_TEST_CASE(sort_float_vector_desc)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    float data[] = {
        -6023.0f, 152.5f, -63.0f, 1234567.0f, 11.2f,
        -5000.1f, 0.0f, 14.0f, -8.25f, -0.0f
    };

    boost::compute::vector<float> vector(data, data + 10, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(10));

    BOOST_CHECK(!boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<float>(), queue
    ));
    boost::compute::detail::radix_sort(
        vector.begin(), vector.end(), descending, queue
    );
    BOOST_CHECK(boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<float>(), queue
    ));

    CHECK_RANGE_EQUAL(
        float, 10, vector,
        (1234567.0f, 152.5f, 14.0f, 11.2f, 0.0f, -0.0f, -8.25f, -63.0f, -5000.1f, -6023.0f)
    );

    // copy data, sort, and check again (to check program caching)
    boost::compute::copy(data, data + 10, vector.begin(), queue);
    boost::compute::detail::radix_sort(
        vector.begin(), vector.end(), descending, queue
    );
    BOOST_CHECK(boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<float>(), queue
    ));
    CHECK_RANGE_EQUAL(
        float, 10, vector,
        (1234567.0f, 152.5f, 14.0f, 11.2f, 0.0f, -0.0f, -8.25f, -63.0f, -5000.1f, -6023.0f)
    );
}

BOOST_AUTO_TEST_CASE(sort_double_vector_desc)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    if(!device.supports_extension("cl_khr_fp64")){
        std::cout << "skipping test: device does not support double" << std::endl;
        return;
    }

    double data[] = {
        -6023.0, 152.5, -63.0, 1234567.0, 11.2, -5000.1, 0.0, 14.0, -8.25, -0.0
    };

    boost::compute::vector<double> vector(data, data + 10, queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(10));

    BOOST_CHECK(!boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<double>(), queue
    ));
    boost::compute::detail::radix_sort(
        vector.begin(), vector.end(), descending, queue
    );
    BOOST_CHECK(boost::compute::is_sorted(
        vector.begin(), vector.end(), boost::compute::greater<double>(), queue
    ));

    CHECK_RANGE_EQUAL(
        double, 10, vector,
        (1234567.0, 152.5, 14.0, 11.2, 0.0, -0.0, -8.25, -63.0, -5000.1, -6023.0)
    );
}

BOOST_AUTO_TEST_CASE(sort_partial_vector)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    int data[] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
    boost::compute::vector<int> vec(data, data + 10, queue);

    boost::compute::detail::radix_sort(vec.begin() + 2, vec.end() - 2, queue);
    CHECK_RANGE_EQUAL(int, 10, vec, (9, 8, 2, 3, 4, 5, 6, 7, 1, 0));
}

BOOST_AUTO_TEST_SUITE_END()
