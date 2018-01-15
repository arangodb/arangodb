//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestZipIterator
#include <boost/test/unit_test.hpp>

#include <boost/type_traits.hpp>
#include <boost/static_assert.hpp>
#include <boost/tuple/tuple_io.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include <boost/compute/functional.hpp>
#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/algorithm/transform.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/constant_iterator.hpp>
#include <boost/compute/iterator/zip_iterator.hpp>
#include <boost/compute/types/tuple.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

namespace compute = boost::compute;

BOOST_AUTO_TEST_CASE(value_type)
{
    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::compute::zip_iterator<
                boost::tuple<
                    boost::compute::buffer_iterator<float>,
                    boost::compute::buffer_iterator<int>
                >
            >::value_type,
            boost::tuple<float, int>
        >::value
    ));
}

BOOST_AUTO_TEST_CASE(distance)
{
    boost::compute::vector<char> char_vector(5, context);
    boost::compute::vector<int> int_vector(5, context);

    BOOST_CHECK_EQUAL(
        std::distance(
            boost::compute::make_zip_iterator(
                boost::make_tuple(
                    char_vector.begin(),
                    int_vector.begin()
                )
            ),
            boost::compute::make_zip_iterator(
                boost::make_tuple(
                    char_vector.end(),
                    int_vector.end()
                )
            )
        ),
        ptrdiff_t(5)
    );

    BOOST_CHECK_EQUAL(
        std::distance(
            boost::compute::make_zip_iterator(
                boost::make_tuple(
                    char_vector.begin(),
                    int_vector.begin()
                )
            ) + 1,
            boost::compute::make_zip_iterator(
                boost::make_tuple(
                    char_vector.end(),
                    int_vector.end()
                )
            ) - 1
        ),
        ptrdiff_t(3)
    );

    BOOST_CHECK_EQUAL(
        std::distance(
            boost::compute::make_zip_iterator(
                boost::make_tuple(
                    char_vector.begin() + 2,
                    int_vector.begin() + 2
                )
            ),
            boost::compute::make_zip_iterator(
                boost::make_tuple(
                    char_vector.end() - 1,
                    int_vector.end() - 1
                )
            )
        ),
        ptrdiff_t(2)
    );
}

BOOST_AUTO_TEST_CASE(copy)
{
    // create three separate vectors of three different types
    char char_data[] = { 'x', 'y', 'z' };
    boost::compute::vector<char> char_vector(char_data, char_data + 3, queue);

    int int_data[] = { 4, 7, 9 };
    boost::compute::vector<int> int_vector(int_data, int_data + 3, queue);

    float float_data[] = { 3.2f, 4.5f, 7.6f };
    boost::compute::vector<float> float_vector(float_data, float_data + 3, queue);

    // zip all three vectors into a single tuple vector
    boost::compute::vector<boost::tuple<char, int, float> > tuple_vector(3, context);

    boost::compute::copy(
        boost::compute::make_zip_iterator(
            boost::make_tuple(
                char_vector.begin(),
                int_vector.begin(),
                float_vector.begin()
            )
        ),
        boost::compute::make_zip_iterator(
            boost::make_tuple(
                char_vector.end(),
                int_vector.end(),
                float_vector.end()
            )
        ),
        tuple_vector.begin(),
        queue
    );

    // copy tuple vector to host
    std::vector<boost::tuple<char, int, float> > host_vector(3);

    boost::compute::copy(
        tuple_vector.begin(),
        tuple_vector.end(),
        host_vector.begin(),
        queue
    );

    // check tuple values
    BOOST_CHECK_EQUAL(host_vector[0], boost::make_tuple('x', 4, 3.2f));
    BOOST_CHECK_EQUAL(host_vector[1], boost::make_tuple('y', 7, 4.5f));
    BOOST_CHECK_EQUAL(host_vector[2], boost::make_tuple('z', 9, 7.6f));
}

BOOST_AUTO_TEST_CASE(zip_iterator_get)
{
    int data1[] = { 0, 2, 4, 6, 8 };
    int data2[] = { 1, 3, 5, 7, 9 };

    compute::vector<int> input1(data1, data1 + 5, queue);
    compute::vector<int> input2(data2, data2 + 5, queue);
    compute::vector<int> output(5, context);

    // extract first component from (input1)
    compute::transform(
        compute::make_zip_iterator(
            boost::make_tuple(input1.begin())
        ),
        compute::make_zip_iterator(
            boost::make_tuple(input1.end())
        ),
        output.begin(),
        compute::get<0>(),
        queue
    );
    CHECK_RANGE_EQUAL(int, 5, output, (0, 2, 4, 6, 8));

    // extract first component from (input2, input1)
    compute::transform(
        compute::make_zip_iterator(
            boost::make_tuple(input2.begin(), input1.begin())
        ),
        compute::make_zip_iterator(
            boost::make_tuple(input2.end(), input1.end())
        ),
        output.begin(),
        compute::get<0>(),
        queue
    );
    CHECK_RANGE_EQUAL(int, 5, output, (1, 3, 5, 7, 9));

    // extract second component from (input1, input2, input1)
    compute::transform(
        compute::make_zip_iterator(
            boost::make_tuple(input1.begin(), input2.begin(), input1.begin())
        ),
        compute::make_zip_iterator(
            boost::make_tuple(input1.end(), input2.end(), input1.end())
        ),
        output.begin(),
        compute::get<1>(),
        queue
    );
    CHECK_RANGE_EQUAL(int, 5, output, (1, 3, 5, 7, 9));
}

BOOST_AUTO_TEST_CASE(zip_constant_iterator)
{
    compute::vector<int> result(4, context);

    compute::transform(
        compute::make_zip_iterator(
            boost::make_tuple(
                compute::make_constant_iterator(7)
            )
        ),
        compute::make_zip_iterator(
            boost::make_tuple(
                compute::make_constant_iterator(7, result.size())
            )
        ),
        result.begin(),
        compute::get<0>(),
        queue
    );

    CHECK_RANGE_EQUAL(int, 4, result, (7, 7, 7, 7));
}

BOOST_AUTO_TEST_SUITE_END()
