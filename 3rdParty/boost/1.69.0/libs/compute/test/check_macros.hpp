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

template <typename Left, typename Right, typename ToleranceBaseType>
inline void
equal_close_impl(Left left_begin,
                 Left left_end,
                 Right right_begin,
                 Right right_end,
                 ToleranceBaseType tolerance)
{
    for(; left_begin != (left_end); ++left_begin, ++right_begin) {
        BOOST_CHECK_CLOSE(*left_begin, *right_begin, tolerance); \
    }
}

#define BOOST_COMPUTE_TEST_CHECK_CLOSE_COLLECTIONS(L_begin, L_end, R_begin, R_end, tolerance) \
    { \
        equal_close_impl(L_begin, L_end, R_begin, R_end, tolerance); \
    }

#define CHECK_RANGE_CLOSE(type, size, actual, expected, tolerance) \
    { \
        type _actual[size]; \
        boost::compute::copy( \
            actual.begin(), actual.begin()+size, _actual, queue \
        ); \
        const type _expected[size] = { \
            BOOST_PP_REPEAT(size, LIST_ARRAY_VALUES, (size, expected)) \
        }; \
        BOOST_COMPUTE_TEST_CHECK_CLOSE_COLLECTIONS( \
            _actual, _actual + size, _expected, _expected + size, tolerance \
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
