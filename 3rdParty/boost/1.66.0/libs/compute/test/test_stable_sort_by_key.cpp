//---------------------------------------------------------------------------//
// Copyright (c) 2016 Jakub Szuppe <j.szuppe@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestStableSortByKey
#include <boost/test/unit_test.hpp>

#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/stable_sort_by_key.hpp>
#include <boost/compute/algorithm/is_sorted.hpp>
#include <boost/compute/container/vector.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(empty_int_by_int)
{
    compute::vector<compute::int_> keys(size_t(0), compute::int_(0), queue);
    compute::vector<compute::int_> values(size_t(0), compute::int_(0), queue);

    BOOST_CHECK_EQUAL(keys.size(), size_t(0));
    BOOST_CHECK_EQUAL(values.size(), size_t(0));

    BOOST_CHECK(compute::is_sorted(keys.begin(), keys.end(), queue));
    BOOST_CHECK(compute::is_sorted(values.begin(), values.end(), queue));

    compute::stable_sort_by_key(
        keys.begin(), keys.end(), values.begin(), queue
    );

    BOOST_CHECK(compute::is_sorted(keys.begin(), keys.end()));
    BOOST_CHECK(compute::is_sorted(values.begin(), values.end()));
}

BOOST_AUTO_TEST_CASE(one_element_int_by_int)
{
    compute::int_ keys_data[] = { 1 };
    compute::int_ values_data[] = { 2 };

    compute::vector<compute::int_> keys(keys_data, keys_data + 1, queue);
    compute::vector<compute::int_> values(values_data, values_data + 1, queue);

    BOOST_CHECK(compute::is_sorted(keys.begin(), keys.end(), queue));
    BOOST_CHECK(compute::is_sorted(values.begin(), values.end(), queue));

    compute::stable_sort_by_key(keys.begin(), keys.end(), values.begin(), queue);

    BOOST_CHECK(compute::is_sorted(keys.begin(), keys.end(), queue));
    BOOST_CHECK(compute::is_sorted(values.begin(), values.end(), queue));
}

BOOST_AUTO_TEST_CASE(two_elements_int_by_int)
{
    compute::int_ keys_data[] = { 1, -1 };
    compute::int_ values_data[] = { -10, 1 };

    compute::vector<compute::int_> keys(keys_data, keys_data + 2, queue);
    compute::vector<compute::int_> values(values_data, values_data + 2, queue);

    BOOST_CHECK(!compute::is_sorted(keys.begin(), keys.end(), queue));
    compute::stable_sort_by_key(keys.begin(), keys.end(), values.begin(), queue);
    BOOST_CHECK(compute::is_sorted(keys.begin(), keys.end(), queue));
}

BOOST_AUTO_TEST_CASE(stable_sort_int_by_int)
{
    compute::int_ keys_data[] =   { 10, 9, 2, 7, 6, -1, 4, 2, 2, 10 };
    compute::int_ values_data[] = { 1,  2, 3, 4, 5,  6, 7, 8, 9, 10 };

    compute::vector<compute::int_> keys(keys_data, keys_data + 10, queue);
    compute::vector<compute::int_> values(values_data, values_data + 10, queue);

    BOOST_CHECK(!compute::is_sorted(keys.begin(), keys.end(), queue));
    compute::stable_sort_by_key(keys.begin(), keys.end(), values.begin(), queue);
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

BOOST_AUTO_TEST_CASE(stable_sort_uint_by_uint)
{
    compute::uint_ keys_data[] =   { 10, 9, 2, 7, 6, 1, 4, 2, 2, 10 };
    compute::uint_ values_data[] = { 1,  2, 3, 4, 5, 6, 7, 8, 9, 10 };

    compute::vector<compute::uint_> keys(keys_data, keys_data + 10, queue);
    compute::vector<compute::uint_> values(values_data, values_data + 10, queue);

    BOOST_CHECK(!compute::is_sorted(keys.begin(), keys.end(), queue));
    compute::stable_sort_by_key(keys.begin(), keys.end(), values.begin(), queue);
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

BOOST_AUTO_TEST_CASE(stable_sort_int_by_float)
{
    compute::float_ keys_data[]   = { 10., 5.5, 10., 7., 5.5};
    compute::int_   values_data[] = {   1, 200, -10,  2, 4 };

    compute::vector<compute::float_> keys(keys_data, keys_data + 5, queue);
    compute::vector<compute::uint_> values(values_data, values_data + 5, queue);

    BOOST_CHECK(!compute::is_sorted(keys.begin(), keys.end(), queue));
    compute::stable_sort_by_key(keys.begin(), keys.end(), values.begin(), queue);
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

BOOST_AUTO_TEST_CASE(stable_sort_char_by_int)
{
    compute::int_ keys_data[] = { 6, 1, 1, 3, 4, 7, 5, 1 };
    compute::char_ values_data[] = { 'g', 'c', 'b', 'd', 'e', 'h', 'f', 'a' };

    compute::vector<compute::int_> keys(keys_data, keys_data + 8, queue);
    compute::vector<compute::char_> values(values_data, values_data + 8, queue);

    compute::sort_by_key(keys.begin(), keys.end(), values.begin(), queue);

    CHECK_RANGE_EQUAL(
        compute::int_, 8, keys,
        (1, 1, 1, 3, 4, 5, 6, 7)
    );
    CHECK_RANGE_EQUAL(
        compute::char_, 8, values,
        ('c', 'b', 'a', 'd', 'e', 'f', 'g', 'h')
    );
}

BOOST_AUTO_TEST_CASE(stable_sort_mid_uint_by_uint)
{
    using boost::compute::int_;

    const int_ size = 128;
    std::vector<int_> keys_data(size);
    std::vector<int_> values_data(size);
    for(int_ i = 0; i < size; i++){
        keys_data[i] = -i;
        values_data[i] = -i;
    }

    keys_data[size/2] = -256;
    keys_data[size - 2] = -256;
    keys_data[size - 1] = -256;
    values_data[size/2] = 3;
    values_data[size - 2] = 1;
    values_data[size - 1] = 2;

    compute::vector<int_> keys(keys_data.begin(), keys_data.end(), queue);
    compute::vector<int_> values(values_data.begin(), values_data.end(), queue);

    // less function for float
    BOOST_COMPUTE_FUNCTION(bool, comp, (int_ a, int_ b),
    {
        return a < b;
    });

    BOOST_CHECK(!compute::is_sorted(keys.begin(), keys.end(), comp, queue));
    compute::stable_sort_by_key(keys.begin(), keys.end(), values.begin(), comp, queue);
    BOOST_CHECK(compute::is_sorted(keys.begin(), keys.end(), comp, queue));

    BOOST_CHECK(keys.begin().read(queue) == -256);
    BOOST_CHECK((keys.begin() + 1).read(queue) == -256);
    BOOST_CHECK((keys.begin() + 2).read(queue) == -256);

    BOOST_CHECK(values.begin().read(queue) == 3);
    BOOST_CHECK((values.begin() + 1).read(queue) == 1);
    BOOST_CHECK((values.begin() + 2).read(queue) == 2);
}

BOOST_AUTO_TEST_SUITE_END()
