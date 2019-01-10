#ifndef SAFE_NUMERIC_TEST_HPP
#define SAFE_NUMERIC_TEST_HPP

// MS compatible compilers support #pragma once
#if defined(_MSC_VER)
# pragma once
#endif

//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/preprocessor/array/elem.hpp>
#include <boost/preprocessor/array/size.hpp>
#include <boost/preprocessor/stringize.hpp>

#define VALUE_ARRAY_SIZE BOOST_PP_ARRAY_SIZE(VALUES)

#define TEST_EACH_VALUE2(z, value_index2, value_index1) \
    TESTX(value_index1, value_index2)
/**/

#define TEST_EACH_VALUE1(z, value_index1, nothing) \
    BOOST_PP_REPEAT(                               \
        BOOST_PP_ARRAY_SIZE(VALUES),               \
        TEST_EACH_VALUE2,                          \
        value_index1                               \
    )
/**/

#define TEST_EACH_VALUE_PAIR                       \
    BOOST_PP_REPEAT(                               \
        BOOST_PP_ARRAY_SIZE(VALUES),               \
        TEST_EACH_VALUE1,                          \
        0                                          \
    )
/**/

#endif
