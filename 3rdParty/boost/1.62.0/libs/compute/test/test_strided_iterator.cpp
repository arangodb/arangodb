//---------------------------------------------------------------------------//
// Copyright (c) 2015 Jakub Szuppe <j.szuppe@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestStridedIterator
#include <boost/test/unit_test.hpp>

#include <iterator>

#include <boost/type_traits.hpp>
#include <boost/static_assert.hpp>

#include <boost/compute/algorithm/copy.hpp>
#include <boost/compute/container/vector.hpp>
#include <boost/compute/iterator/strided_iterator.hpp>

#include "check_macros.hpp"
#include "context_setup.hpp"

BOOST_AUTO_TEST_CASE(value_type)
{
    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::compute::strided_iterator<
                boost::compute::buffer_iterator<int>
            >::value_type,
            int
        >::value
    ));
    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::compute::strided_iterator<
                boost::compute::buffer_iterator<float>
            >::value_type,
            float
        >::value
    ));
}

BOOST_AUTO_TEST_CASE(base_type)
{
    BOOST_STATIC_ASSERT((
        boost::is_same<
            boost::compute::strided_iterator<
                boost::compute::buffer_iterator<int>
            >::base_type,
            boost::compute::buffer_iterator<int>
        >::value
    ));
}

BOOST_AUTO_TEST_CASE(distance)
{
    int data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    boost::compute::vector<int> vec(data, data + 8, queue);

    BOOST_CHECK_EQUAL(
         std::distance(
             boost::compute::make_strided_iterator(vec.begin(), 1),
             boost::compute::make_strided_iterator(vec.end(), 1)
         ),
         std::ptrdiff_t(8)
    );
    BOOST_CHECK_EQUAL(
        std::distance(
            boost::compute::make_strided_iterator(vec.begin(), 2),
            boost::compute::make_strided_iterator(vec.end(), 2)
        ),
        std::ptrdiff_t(4)
    );

    BOOST_CHECK_EQUAL(
        std::distance(
            boost::compute::make_strided_iterator(vec.begin(), 3),
            boost::compute::make_strided_iterator(vec.begin()+6, 3)
        ),
        std::ptrdiff_t(2)
    );
}

BOOST_AUTO_TEST_CASE(copy)
{
    boost::compute::int_ data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    boost::compute::vector<boost::compute::int_> vec(data, data + 8, queue);

    boost::compute::vector<boost::compute::int_> result(4, context);

    // copy every other element to result
    boost::compute::copy(
        boost::compute::make_strided_iterator(vec.begin(), 2),
        boost::compute::make_strided_iterator(vec.end(), 2),
        result.begin(),
        queue
    );
    CHECK_RANGE_EQUAL(boost::compute::int_, 4, result, (1, 3, 5, 7));

    // copy every 3rd element to result
    boost::compute::copy(
        boost::compute::make_strided_iterator(vec.begin(), 3),
        boost::compute::make_strided_iterator(vec.begin()+9, 3),
        result.begin(),
        queue
    );
    CHECK_RANGE_EQUAL(boost::compute::int_, 3, result, (1, 4, 7));
}

BOOST_AUTO_TEST_CASE(make_strided_iterator_end)
{
    boost::compute::int_ data[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    boost::compute::vector<boost::compute::int_> vec(data, data + 8, queue);

    // stride equals 3
    typedef boost::compute::vector<boost::compute::int_>::iterator IterType;
    boost::compute::strided_iterator<IterType> end =
        boost::compute::make_strided_iterator_end(vec.begin(),
                                                  vec.end(),
                                                  3);

    // end should be vec.begin() + 9 which is one step after last element
    // accessible through strided_iterator, i.e. vec.begin()+6
    BOOST_CHECK(boost::compute::make_strided_iterator(vec.begin()+9, 3) ==
                end);

    // stride equals 2
    end = boost::compute::make_strided_iterator_end(vec.begin(),
                                                    vec.end(),
                                                    2);
    // end should be vec.end(), because vector size is divisible by 2
    BOOST_CHECK(boost::compute::make_strided_iterator(vec.end(), 2) == end);

    // stride equals 1000
    end = boost::compute::make_strided_iterator_end(vec.begin(),
                                                    vec.end(),
                                                    1000);
    // end should be vec.begin() + 1000, because stride > vector size
    BOOST_CHECK(boost::compute::make_strided_iterator(vec.begin()+1000, 1000) ==
                end);


    // test boost::compute::make_strided_iterator_end with copy(..)

    boost::compute::vector<boost::compute::int_> result(4, context);

    // copy every other element to result
    boost::compute::copy(
        boost::compute::make_strided_iterator(vec.begin()+1, 2),
        boost::compute::make_strided_iterator_end(vec.begin()+1, vec.end(), 2),
        result.begin(),
        queue
    );
    CHECK_RANGE_EQUAL(boost::compute::int_, 4, result, (2, 4, 6, 8));
}

BOOST_AUTO_TEST_SUITE_END()
