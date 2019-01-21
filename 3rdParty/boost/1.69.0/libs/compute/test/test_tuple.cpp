//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestTuple
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_io.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/fill.hpp>
#include <boost/compute/algorithm/find.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/types/tuple.hpp>

#include "quirks.hpp"
#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(vector_tuple_int_float)
{
    boost::compute::vector<boost::tuple<int, float> > vector(context);

    vector.push_back(boost::make_tuple(1, 2.1f), queue);
    vector.push_back(boost::make_tuple(2, 3.2f), queue);
    vector.push_back(boost::make_tuple(3, 4.3f), queue);
}

BOOST_AUTO_TEST_CASE(copy_vector_tuple)
{
    // create vector of tuples on device
    boost::compute::vector<boost::tuple<char, int, float> > input(context);
    input.push_back(boost::make_tuple('a', 1, 2.3f), queue);
    input.push_back(boost::make_tuple('c', 3, 4.5f), queue);
    input.push_back(boost::make_tuple('f', 6, 7.8f), queue);

    // copy on device
    boost::compute::vector<boost::tuple<char, int, float> > output(context);

    boost::compute::copy(
        input.begin(),
        input.end(),
        output.begin(),
        queue
    );

    // copy to host
    std::vector<boost::tuple<char, int, float> > host_output(3);

    boost::compute::copy(
        input.begin(),
        input.end(),
        host_output.begin(),
        queue
    );

    // check tuple data
    BOOST_CHECK_EQUAL(host_output[0], boost::make_tuple('a', 1, 2.3f));
    BOOST_CHECK_EQUAL(host_output[1], boost::make_tuple('c', 3, 4.5f));
    BOOST_CHECK_EQUAL(host_output[2], boost::make_tuple('f', 6, 7.8f));
}

BOOST_AUTO_TEST_CASE(extract_tuple_elements)
{
    compute::vector<boost::tuple<char, int, float> > vector(context);
    vector.push_back(boost::make_tuple('a', 1, 2.3f), queue);
    vector.push_back(boost::make_tuple('c', 3, 4.5f), queue);
    vector.push_back(boost::make_tuple('f', 6, 7.8f), queue);

    compute::vector<char> chars(3, context);
    compute::transform(
        vector.begin(), vector.end(), chars.begin(), compute::get<0>(), queue
    );
    CHECK_RANGE_EQUAL(char, 3, chars, ('a', 'c', 'f'));

    compute::vector<int> ints(3, context);
    compute::transform(
        vector.begin(), vector.end(), ints.begin(), compute::get<1>(), queue
    );
    CHECK_RANGE_EQUAL(int, 3, ints, (1, 3, 6));

    compute::vector<float> floats(3, context);
    compute::transform(
        vector.begin(), vector.end(), floats.begin(), compute::get<2>(), queue
    );
    CHECK_RANGE_EQUAL(float, 3, floats, (2.3f, 4.5f, 7.8f));
}

BOOST_AUTO_TEST_CASE(fill_tuple_vector)
{
    if(bug_in_struct_assignment(device)){
        std::cerr << "skipping fill_tuple_vector test" << std::endl;
        return;
    }

    compute::vector<boost::tuple<char, int, float> > vector(5, context);
    compute::fill(vector.begin(), vector.end(), boost::make_tuple('z', 4, 3.14f), queue);

    std::vector<boost::tuple<char, int, float> > host_output(5);
    compute::copy(vector.begin(), vector.end(), host_output.begin(), queue);
    BOOST_CHECK_EQUAL(host_output[0], boost::make_tuple('z', 4, 3.14f));
    BOOST_CHECK_EQUAL(host_output[1], boost::make_tuple('z', 4, 3.14f));
    BOOST_CHECK_EQUAL(host_output[2], boost::make_tuple('z', 4, 3.14f));
    BOOST_CHECK_EQUAL(host_output[3], boost::make_tuple('z', 4, 3.14f));
    BOOST_CHECK_EQUAL(host_output[4], boost::make_tuple('z', 4, 3.14f));
}

#ifndef BOOST_COMPUTE_NO_VARIADIC_TEMPLATES
BOOST_AUTO_TEST_CASE(variadic_tuple)
{
    BOOST_CHECK_EQUAL(
        (compute::type_name<boost::tuple<char, short, int, float> >()),
        "boost_tuple_char_short_int_float_t"
    );
}
#endif // BOOST_COMPUTE_NO_VARIADIC_TEMPLATES

#ifndef BOOST_COMPUTE_NO_STD_TUPLE
BOOST_AUTO_TEST_CASE(std_tuple)
{
    BOOST_CHECK_EQUAL(
        (compute::type_name<std::tuple<char, short, int, float>>()),
        "std_tuple_char_short_int_float_t"
    );
}
#endif // BOOST_COMPUTE_NO_STD_TUPLE

BOOST_AUTO_TEST_SUITE_END()
