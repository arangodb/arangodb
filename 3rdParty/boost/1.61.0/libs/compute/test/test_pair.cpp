//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestPair
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/fill.hpp>
#include <boost/compute/algorithm/find.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/functional/get.hpp>
#include <boost/compute/functional/field.hpp>
#include <boost/compute/types/pair.hpp>

#include "quirks.hpp"
#include "check_macros.hpp"
#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(vector_pair_int_float)
{
    boost::compute::vector<std::pair<int, float> > vector(context);
    vector.push_back(std::make_pair(1, 1.1f), queue);
    vector.push_back(std::make_pair(2, 2.2f), queue);
    vector.push_back(std::make_pair(3, 3.3f), queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(3));
    BOOST_CHECK(vector[0] == std::make_pair(1, 1.1f));
    BOOST_CHECK(vector[1] == std::make_pair(2, 2.2f));
    BOOST_CHECK(vector[2] == std::make_pair(3, 3.3f));
}

BOOST_AUTO_TEST_CASE(copy_pair_vector)
{
    boost::compute::vector<std::pair<int, float> > input(context);
    input.push_back(std::make_pair(1, 2.0f), queue);
    input.push_back(std::make_pair(3, 4.0f), queue);
    input.push_back(std::make_pair(5, 6.0f), queue);
    input.push_back(std::make_pair(7, 8.0f), queue);
    BOOST_CHECK_EQUAL(input.size(), size_t(4));

    boost::compute::vector<std::pair<int, float> > output(4, context);
    boost::compute::copy(input.begin(), input.end(), output.begin(), queue);
    queue.finish();
    BOOST_CHECK(output[0] == std::make_pair(1, 2.0f));
    BOOST_CHECK(output[1] == std::make_pair(3, 4.0f));
    BOOST_CHECK(output[2] == std::make_pair(5, 6.0f));
    BOOST_CHECK(output[3] == std::make_pair(7, 8.0f));
}

BOOST_AUTO_TEST_CASE(fill_pair_vector)
{
    if(bug_in_struct_assignment(device)){
        std::cerr << "skipping fill_pair_vector test" << std::endl;
        return;
    }

    boost::compute::vector<std::pair<int, float> > vector(5, context);
    boost::compute::fill(vector.begin(), vector.end(), std::make_pair(4, 2.0f), queue);
    queue.finish();
    BOOST_CHECK(vector[0] == std::make_pair(4, 2.0f));
    BOOST_CHECK(vector[1] == std::make_pair(4, 2.0f));
    BOOST_CHECK(vector[2] == std::make_pair(4, 2.0f));
    BOOST_CHECK(vector[3] == std::make_pair(4, 2.0f));
    BOOST_CHECK(vector[4] == std::make_pair(4, 2.0f));
}

BOOST_AUTO_TEST_CASE(fill_char_pair_vector)
{
    if(bug_in_struct_assignment(device)){
        std::cerr << "skipping fill_char_pair_vector test" << std::endl;
        return;
    }

    std::pair<char, unsigned char> value('c', static_cast<unsigned char>(127));
    boost::compute::vector<std::pair<char, unsigned char> > vector(5, context);
    boost::compute::fill(vector.begin(), vector.end(), value, queue);
    queue.finish();
    BOOST_CHECK(vector[0] == value);
    BOOST_CHECK(vector[1] == value);
    BOOST_CHECK(vector[2] == value);
    BOOST_CHECK(vector[3] == value);
    BOOST_CHECK(vector[4] == value);
}

BOOST_AUTO_TEST_CASE(transform_pair_get)
{
    boost::compute::vector<std::pair<int, float> > input(context);
    input.push_back(std::make_pair(1, 2.0f), queue);
    input.push_back(std::make_pair(3, 4.0f), queue);
    input.push_back(std::make_pair(5, 6.0f), queue);
    input.push_back(std::make_pair(7, 8.0f), queue);

    boost::compute::vector<int> first_output(4, context);
    boost::compute::transform(
        input.begin(),
        input.end(),
        first_output.begin(),
        ::boost::compute::get<0>(),
        queue
    );
    CHECK_RANGE_EQUAL(int, 4, first_output, (1, 3, 5, 7));

    boost::compute::vector<float> second_output(4, context);
    boost::compute::transform(
        input.begin(),
        input.end(),
        second_output.begin(),
        ::boost::compute::get<1>(),
        queue
    );
    CHECK_RANGE_EQUAL(float, 4, second_output, (2.0f, 4.0f, 6.0f, 8.0f));
}

BOOST_AUTO_TEST_CASE(transform_pair_field)
{
    boost::compute::vector<std::pair<int, float> > input(context);
    input.push_back(std::make_pair(1, 2.0f), queue);
    input.push_back(std::make_pair(3, 4.0f), queue);
    input.push_back(std::make_pair(5, 6.0f), queue);
    input.push_back(std::make_pair(7, 8.0f), queue);

    boost::compute::vector<int> first_output(4, context);
    boost::compute::transform(
        input.begin(),
        input.end(),
        first_output.begin(),
        boost::compute::field<int>("first"),
        queue
    );
    CHECK_RANGE_EQUAL(int, 4, first_output, (1, 3, 5, 7));

    boost::compute::vector<float> second_output(4, context);
    boost::compute::transform(
        input.begin(),
        input.end(),
        second_output.begin(),
        boost::compute::field<float>("second"),
        queue
    );
    CHECK_RANGE_EQUAL(float, 4, second_output, (2.0f, 4.0f, 6.0f, 8.0f));
}

BOOST_AUTO_TEST_CASE(find_vector_pair)
{
    boost::compute::vector<std::pair<int, float> > vector(context);
    vector.push_back(std::make_pair(1, 1.1f), queue);
    vector.push_back(std::make_pair(2, 2.2f), queue);
    vector.push_back(std::make_pair(3, 3.3f), queue);
    BOOST_CHECK_EQUAL(vector.size(), size_t(3));

    BOOST_CHECK(
        boost::compute::find(
            boost::compute::make_transform_iterator(
                vector.begin(),
                boost::compute::get<0>()
            ),
            boost::compute::make_transform_iterator(
                vector.end(),
                boost::compute::get<0>()
            ),
            int(2),
            queue
        ).base() == vector.begin() + 1
    );

    BOOST_CHECK(
        boost::compute::find(
            boost::compute::make_transform_iterator(
                vector.begin(),
                boost::compute::get<1>()
            ),
            boost::compute::make_transform_iterator(
                vector.end(),
                boost::compute::get<1>()
            ),
            float(3.3f),
            queue
        ).base() == vector.begin() + 2
    );
}

BOOST_AUTO_TEST_SUITE_END()
