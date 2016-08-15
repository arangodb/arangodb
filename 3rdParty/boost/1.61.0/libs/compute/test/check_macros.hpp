//---------------------------------------------------------------------------//
// Copyright (c) 2013 Kyle Lutz <kyle.r.lutz@gmail.com>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#ifndef BOOST_COMPUTE_TEST_CHECK_MACROS_HPP
#define BOOST_COMPUTE_TEST_CHECK_MACROS_HPP

#define LIST_ARRAY_VALUES(z, n, data) \
    BOOST_PP_COMMA_IF(n) BOOST_PP_ARRAY_ELEM(n, data)

// checks 'size' values of 'type' in the device range 'actual`
// against the values given in the array 'expected'
#define CHECK_RANGE_EQUAL(type, size, actual, expected) \
    { \
        type _actual[size]; \
        boost::compute::copy( \
            actual.begin(), actual.begin()+size, _actual, queue \
        ); \
        const type _expected[size] = { \
            BOOST_PP_REPEAT(size, LIST_ARRAY_VALUES, (size, expected)) \
        }; \
        BOOST_CHECK_EQUAL_COLLECTIONS( \
            _actual, _actual + size, _expected, _expected + size \
        ); \
    }

#define CHECK_HOST_RANGE_EQUAL(type, size, actual, expected) \
    { \
        const type _expected[size] = { \
            BOOST_PP_REPEAT(size, LIST_ARRAY_VALUES, (size, expected)) \
        }; \
        BOOST_CHECK_EQUAL_COLLECTIONS( \
            actual, actual + size, _expected, _expected + size \
        ); \
    }

#define CHECK_STRING_EQUAL(actual, expected) \
    { \
        std::string _actual(actual.size(), '\0'); \
        boost::compute::copy( \
            actual.begin(), actual.end(), _actual.begin(), queue \
        ); \
        BOOST_CHECK_EQUAL(_actual, expected); \
    }

#endif // BOOST_COMPUTE_TEST_CHECK_MACROS_HPP
