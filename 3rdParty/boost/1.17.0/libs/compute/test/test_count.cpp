//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestCount
#include <boost/test/unit_test.hpp>

#include <string>

#include <boost/compute/command_queue.hpp>
#include <boost/compute/function.hpp>
#include <boost/compute/lambda.hpp>
#include <boost/compute/system.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/count.hpp>
#include <boost/compute/algorithm/count_if.hpp>
#include <boost/compute/algorithm/iota.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/constant_iterator.hpp>

#include "context_setup.hpp"

namespace bc = boost::compute;
namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(count_int)
{
    int data[] = { 1, 2, 1, 2, 3 };
    bc::vector<int> vector(data, data + 5, queue);
    BOOST_CHECK_EQUAL(bc::count(vector.begin(), vector.end(), 1, queue), size_t(2));
    BOOST_CHECK_EQUAL(bc::count(vector.begin(), vector.end(), 2, queue), size_t(2));
    BOOST_CHECK_EQUAL(bc::count(vector.begin(), vector.end(), 3, queue), size_t(1));
    BOOST_CHECK_EQUAL(bc::count(vector.begin() + 1, vector.end(), 1, queue), size_t(1));
    BOOST_CHECK_EQUAL(bc::count(vector.begin() + 1, vector.end() - 1, 3, queue), size_t(0));
    BOOST_CHECK_EQUAL(bc::count(vector.begin() + 1, vector.end() - 1, 2, queue), size_t(2));
}

BOOST_AUTO_TEST_CASE(count_constant_int_range)
{
    BOOST_CHECK_EQUAL(
        bc::count(bc::make_constant_iterator(18, 0),
                  bc::make_constant_iterator(18, 5),
                  18,
                  queue),
        size_t(5)
    );

    BOOST_CHECK_EQUAL(
        bc::count(bc::make_constant_iterator(19, 0),
                  bc::make_constant_iterator(19, 5),
                  18,
                  queue),
        size_t(0)
    );
}

BOOST_AUTO_TEST_CASE(count_if_greater_than_two)
{
    float data[] = { 1.0f, 2.5f, -1.0f, 3.0f, 5.0f, -8.0f };
    bc::vector<float> vector(data, data + 5, queue);

    BOOST_CHECK_EQUAL(
        bc::count_if(vector.begin(), vector.end(), bc::_1 > 2.0f, queue),
        size_t(3)
    );
}

BOOST_AUTO_TEST_CASE(count_int4)
{
    int data[] = { 1, 2, 3, 4,
                   4, 5, 6, 7,
                   7, 8, 9, 1,
                   1, 2, 3, 4,
                   4, 5, 6, 7,
                   0, 3, 2, 2 };
    bc::vector<bc::int4_> vector(reinterpret_cast<bc::int4_ *>(data),
                                 reinterpret_cast<bc::int4_ *>(data) + 6,
                                 queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(6));

    BOOST_CHECK_EQUAL(
        bc::count(vector.begin(), vector.end(), bc::int4_(1, 2, 3, 4), queue),
        size_t(2)
    );
    BOOST_CHECK_EQUAL(
        bc::count(vector.begin(), vector.end(), bc::int4_(4, 5, 6, 7), queue),
        size_t(2)
    );
    BOOST_CHECK_EQUAL(
        bc::count(vector.begin(), vector.end(), bc::int4_(7, 8, 9, 1), queue),
        size_t(1)
    );
    BOOST_CHECK_EQUAL(
        bc::count(vector.begin(), vector.end(), bc::int4_(0, 3, 2, 2), queue),
        size_t(1)
    );
    BOOST_CHECK_EQUAL(
        bc::count(vector.begin(), vector.end(), bc::int4_(3, 4, 4, 5), queue),
        size_t(0)
    );
    BOOST_CHECK_EQUAL(
        bc::count(vector.begin(), vector.end(), bc::int4_(1, 2, 3, 0), queue),
        size_t(0)
    );
    BOOST_CHECK_EQUAL(
        bc::count(vector.begin(), vector.end(), bc::int4_(1, 9, 8, 7), queue),
        size_t(0)
    );
}

BOOST_AUTO_TEST_CASE(count_newlines)
{
    std::string string = "abcdefg\nhijklmn\nopqrs\ntuv\nwxyz\n";
    compute::vector<char> data(string.size(), context);
    compute::copy(string.begin(), string.end(), data.begin(), queue);

    BOOST_CHECK_EQUAL(
        compute::count(data.begin(), data.end(), '\n', queue),
        size_t(5)
    );
}

BOOST_AUTO_TEST_CASE(count_uchar)
{
    using boost::compute::uchar_;

    unsigned char data[] = { 0x00, 0x10, 0x2F, 0x10, 0x01, 0x00, 0x01, 0x00 };
    compute::vector<uchar_> vector(8, context);
    compute::copy(data, data + 8, vector.begin(), queue);

    BOOST_CHECK_EQUAL(
        compute::count(vector.begin(), vector.end(), 0x00, queue),
        size_t(3)
    );
    BOOST_CHECK_EQUAL(
        compute::count(vector.begin(), vector.end(), 0x10, queue),
        size_t(2)
    );
    BOOST_CHECK_EQUAL(
        compute::count(vector.begin(), vector.end(), 0x2F, queue),
        size_t(1)
    );
    BOOST_CHECK_EQUAL(
        compute::count(vector.begin(), vector.end(), 0x01, queue),
        size_t(2)
    );
    BOOST_CHECK_EQUAL(
        compute::count(vector.begin(), vector.end(), 0xFF, queue),
        size_t(0)
    );
}

BOOST_AUTO_TEST_CASE(count_vector_component)
{
    int data[] = {
        1, 2,
        3, 4,
        5, 6,
        7, 8
    };

    using boost::compute::int2_;

    compute::vector<int2_> vector(4, context);
    compute::copy(
        reinterpret_cast<int2_ *>(data),
        reinterpret_cast<int2_ *>(data) + 4,
        vector.begin(),
        queue
    );

    using boost::compute::lambda::_1;
    using boost::compute::lambda::get;

    BOOST_CHECK_EQUAL(
        compute::count_if(vector.begin(), vector.end(), get<0>(_1) < 4, queue),
        size_t(2)
    );
    BOOST_CHECK_EQUAL(
        compute::count_if(vector.begin(), vector.end(), get<1>(_1) > 3, queue),
        size_t(3)
    );
}

BOOST_AUTO_TEST_CASE(count_if_odd)
{
    compute::vector<int> vec(2048, context);
    compute::iota(vec.begin(), vec.end(), 0, queue);

    BOOST_COMPUTE_FUNCTION(bool, is_odd, (int x),
    {
        return x & 1;
    });

    BOOST_CHECK_EQUAL(
        compute::count_if(vec.begin(), vec.end(), is_odd, queue), vec.size() / 2
    );
}

BOOST_AUTO_TEST_CASE(count_if_with_reduce)
{
    compute::vector<int> vec(2048, context);
    compute::iota(vec.begin(), vec.end(), 0, queue);

    using boost::compute::lambda::_1;

    BOOST_CHECK_EQUAL(
        compute::detail::count_if_with_reduce(
            vec.begin(), vec.end(), _1 > 1024, queue
        ),
        size_t(1023)
    );
}

BOOST_AUTO_TEST_SUITE_END()
