//---------------------------------------------------------------------------//
// Copyright (c) 2016 Jakub Szuppe <j.szuppe@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestRadixSortByKey
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/is_sorted.hpp>
#include <boost/compute/algorithm/detail/radix_sort.hpp>
#include <boost/compute/container/vector.hpp>

#include "quirks.hpp"
#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

const bool descending = false;

// radix_sort_by_key should be stable
BOOST_AUTO_TEST_CASE(stable_radix_sort_int_by_int)
{
    if(is_apple_cpu_device(device)) {
        std::cerr
            << "skipping all radix_sort_by_key tests due to Apple platform"
            << " behavior when local memory is used on a CPU device"
            << std::endl;
        return;
    }

    compute::int_ keys_data[] =   { 10, 9, 2, 7, 6, -1, 4, 2, 2, 10 };
    compute::int_ values_data[] = { 1,  2, 3, 4, 5,  6, 7, 8, 9, 10 };

    compute::vector<compute::int_> keys(keys_data, keys_data + 10, queue);
    compute::vector<compute::int_> values(values_data, values_data + 10, queue);

    BOOST_CHECK(!compute::is_sorted(keys.begin(), keys.end(), queue));
    compute::detail::radix_sort_by_key(keys.begin(), keys.end(), values.begin(), queue);
    BOOST_CHECK(compute::is_sorted(keys.begin(), keys.end(), queue));

    CHECK_RANGE_EQUAL(
        compute::int_, 10, keys,
        (-1, 2, 2, 2, 4, 6, 7, 9, 10, 10) // keys
     // ( 6, 3, 8, 9, 7, 5, 4, 2,  1, 10) values
    );
    CHECK_RANGE_EQUAL(
        compute::int_, 10, values,
     // (-1, 2, 2, 2, 4, 6, 7, 9, 10, 10) keys
        ( 6, 3, 8, 9, 7, 5, 4, 2,  1, 10) // values
    );
}

// radix_sort_by_key should be stable
BOOST_AUTO_TEST_CASE(stable_radix_sort_int_by_int_desc)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    compute::int_ keys_data[] =   { 10, 9, 2, 7, 6, -1, 4, 2, 2, 10 };
    compute::int_ values_data[] = { 1,  2, 3, 4, 5,  6, 7, 8, 9, 10 };

    compute::vector<compute::int_> keys(keys_data, keys_data + 10, queue);
    compute::vector<compute::int_> values(values_data, values_data + 10, queue);

    BOOST_CHECK(
        !compute::is_sorted(
            keys.begin(), keys.end(), compute::greater<compute::int_>(), queue
        )
    );
    compute::detail::radix_sort_by_key(
        keys.begin(), keys.end(), values.begin(), descending, queue
    );
    BOOST_CHECK(
        compute::is_sorted(
            keys.begin(), keys.end(), compute::greater<compute::int_>(), queue
        )
    );

    CHECK_RANGE_EQUAL(
        compute::int_, 10, keys,
        (10, 10, 9, 7, 6, 4, 2, 2, 2, -1) // keys
     // ( 1, 10, 2, 4, 5, 7, 3, 8, 9,  6) values
    );
    CHECK_RANGE_EQUAL(
        compute::int_, 10, values,
    //  (10, 10, 9, 7, 6, 4, 2, 2, 2, -1) // keys
        ( 1, 10, 2, 4, 5, 7, 3, 8, 9,  6) // values
    );
}

// radix_sort_by_key should be stable
BOOST_AUTO_TEST_CASE(stable_radix_sort_uint_by_uint)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    compute::uint_ keys_data[] =   { 10, 9, 2, 7, 6, 1, 4, 2, 2, 10 };
    compute::uint_ values_data[] = { 1,  2, 3, 4, 5, 6, 7, 8, 9, 10 };

    compute::vector<compute::uint_> keys(keys_data, keys_data + 10, queue);
    compute::vector<compute::uint_> values(values_data, values_data + 10, queue);

    BOOST_CHECK(!compute::is_sorted(keys.begin(), keys.end(), queue));
    compute::detail::radix_sort_by_key(keys.begin(), keys.end(), values.begin(), queue);
    BOOST_CHECK(compute::is_sorted(keys.begin(), keys.end(), queue));

    CHECK_RANGE_EQUAL(
        compute::uint_, 10, keys,
        (1, 2, 2, 2, 4, 6, 7, 9, 10, 10) // keys
     // (6, 3, 8, 9, 7, 5, 4, 2,  1, 10) values
    );
    CHECK_RANGE_EQUAL(
        compute::uint_, 10, values,
     // (1, 2, 2, 2, 4, 6, 7, 9, 10, 10) keys
        (6, 3, 8, 9, 7, 5, 4, 2,  1, 10) // values
    );
}

// radix_sort_by_key should be stable
BOOST_AUTO_TEST_CASE(stable_radix_sort_uint_by_uint_desc)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    compute::uint_ keys_data[] =   { 10, 9, 2, 7, 6, 1, 4, 2, 2, 10 };
    compute::uint_ values_data[] = { 1,  2, 3, 4, 5, 6, 7, 8, 9, 10 };

    compute::vector<compute::uint_> keys(keys_data, keys_data + 10, queue);
    compute::vector<compute::uint_> values(values_data, values_data + 10, queue);

    BOOST_CHECK(
        !compute::is_sorted(
            keys.begin(), keys.end(), compute::greater<compute::uint_>(), queue
        )
    );
    compute::detail::radix_sort_by_key(
        keys.begin(), keys.end(), values.begin(), descending, queue
    );
    BOOST_CHECK(
        compute::is_sorted(
            keys.begin(), keys.end(), compute::greater<compute::uint_>(), queue
        )
    );

    CHECK_RANGE_EQUAL(
        compute::uint_, 10, keys,
        (10, 10, 9, 7, 6, 4, 2, 2, 2, 1) // keys
     // ( 1, 10, 2, 4, 5, 7, 3, 8, 9, 6) values
    );
    CHECK_RANGE_EQUAL(
        compute::uint_, 10, values,
    //  (10, 10, 9, 7, 6, 4, 2, 2, 2, 1) // keys
        ( 1, 10, 2, 4, 5, 7, 3, 8, 9, 6) // values
    );
}


// radix_sort_by_key should be stable
BOOST_AUTO_TEST_CASE(stable_radix_sort_int_by_float)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    compute::float_ keys_data[]   = { 10., 5.5, 10., 7., 5.5};
    compute::int_   values_data[] = {   1, 200, -10,  2, 4 };

    compute::vector<compute::float_> keys(keys_data, keys_data + 5, queue);
    compute::vector<compute::uint_> values(values_data, values_data + 5, queue);

    BOOST_CHECK(!compute::is_sorted(keys.begin(), keys.end(), queue));
    compute::detail::radix_sort_by_key(keys.begin(), keys.end(), values.begin(), queue);
    BOOST_CHECK(compute::is_sorted(keys.begin(), keys.end(), queue));

    CHECK_RANGE_EQUAL(
        compute::float_, 5, keys,
        (5.5, 5.5, 7., 10., 10.) // keys
     // (200,   4,  2,   1, -10) values
    );
    CHECK_RANGE_EQUAL(
        compute::int_, 5, values,
     // (5.5, 5.5, 7., 10., 10.) keys
        (200,   4,  2,   1, -10) // values
    );
}

// radix_sort_by_key should be stable
BOOST_AUTO_TEST_CASE(stable_radix_sort_int_by_float_desc)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    compute::float_ keys_data[]   = { 10., 5.5, 10., 7., 5.5};
    compute::int_   values_data[] = {   1, 200, -10,  2, 4 };

    compute::vector<compute::float_> keys(keys_data, keys_data + 5, queue);
    compute::vector<compute::uint_> values(values_data, values_data + 5, queue);

    BOOST_CHECK(
        !compute::is_sorted(
            keys.begin(), keys.end(), compute::greater<compute::float_>(), queue
        )
    );
    compute::detail::radix_sort_by_key(
        keys.begin(), keys.end(), values.begin(), descending, queue
    );
    BOOST_CHECK(
        compute::is_sorted(
            keys.begin(), keys.end(), compute::greater<compute::float_>(), queue
        )
    );

    CHECK_RANGE_EQUAL(
        compute::float_, 5, keys,
        (10.,  10., 7., 5.5, 5.5) // keys
     // (  1, -10,   2, 200, 4) values
    );
    CHECK_RANGE_EQUAL(
        compute::int_, 5, values,
     // (10.,  10., 7., 5.5, 5.5) // keys
        (  1, -10,   2, 200, 4) // values
    );
}


// radix_sort_by_key should be stable
BOOST_AUTO_TEST_CASE(stable_radix_sort_char_by_int)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    compute::int_ keys_data[] = { 6, 1, 1, 3, 4, 7, 5, 1 };
    compute::char_ values_data[] = { 'g', 'c', 'b', 'd', 'e', 'h', 'f', 'a' };

    compute::vector<compute::int_> keys(keys_data, keys_data + 8, queue);
    compute::vector<compute::char_> values(values_data, values_data + 8, queue);

    BOOST_CHECK(!compute::is_sorted(keys.begin(), keys.end(), queue));
    compute::detail::radix_sort_by_key(keys.begin(), keys.end(), values.begin(), queue);
    BOOST_CHECK(compute::is_sorted(keys.begin(), keys.end(), queue));

    CHECK_RANGE_EQUAL(
        compute::int_, 8, keys,
        (1, 1, 1, 3, 4, 5, 6, 7)
    );
    CHECK_RANGE_EQUAL(
        compute::char_, 8, values,
        ('c', 'b', 'a', 'd', 'e', 'f', 'g', 'h')
    );
}

// radix_sort_by_key should be stable
BOOST_AUTO_TEST_CASE(stable_radix_sort_int2_by_int)
{
    if(is_apple_cpu_device(device)) {
        return;
    }

    compute::int_ keys_data[] = { 6, 1, 1, 3, 4, 7, 5, 1 };
    compute::int2_ values_data[] = {
        compute::int2_(1, 1), // 6
        compute::int2_(1, 2), // 1
        compute::int2_(1, 3), // 1
        compute::int2_(1, 4), // 3
        compute::int2_(1, 5), // 4
        compute::int2_(1, 6), // 7
        compute::int2_(1, 7), // 5
        compute::int2_(1, 8)  // 1
    };

    compute::vector<compute::int_> keys(keys_data, keys_data + 8, queue);
    compute::vector<compute::int2_> values(values_data, values_data + 8, queue);

    BOOST_CHECK(!compute::is_sorted(keys.begin(), keys.end(), queue));
    compute::detail::radix_sort_by_key(keys.begin(), keys.end(), values.begin(), queue);
    BOOST_CHECK(compute::is_sorted(keys.begin(), keys.end(), queue));

    CHECK_RANGE_EQUAL(
        compute::int_, 8, keys,
        (1, 1, 1, 3, 4, 5, 6, 7)
    );
    CHECK_RANGE_EQUAL(
        compute::int2_, 8, values,
        (
            compute::int2_(1, 2),
            compute::int2_(1, 3),
            compute::int2_(1, 8),
            compute::int2_(1, 4),
            compute::int2_(1, 5),
            compute::int2_(1, 7),
            compute::int2_(1, 1),
            compute::int2_(1, 6)
        )
    );
}

BOOST_AUTO_TEST_SUITE_END()
