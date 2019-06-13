//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <exception>
#include <cstdlib> // EXIT_SUCCESS

#include <boost/safe_numerics/checked_integer.hpp>

// test conversion to T2 from different literal types
template<class T2, class T1>
bool test_cast(
    const T1 & v1,
    const char *t2_name,
    const char *t1_name,
    char expected_result
){
    std::cout
        << "testing static_cast<" << t2_name << ">(" << t1_name << ")"
        << std::endl;

    boost::safe_numerics::checked_result<T2> r2 = boost::safe_numerics::checked::cast<T2>(v1);

    if(expected_result == 'x' && ! r2.exception()){
        std::cout
            << "failed to detect error in construction "
            << t2_name << "<-" << t1_name
            << std::endl;
        boost::safe_numerics::checked::cast<T2>(v1);
        return false;
    }
    if(expected_result == '.' && r2.exception()){
        std::cout
            << "erroneously emitted error "
            << t2_name << "<-" << t1_name
            << std::endl;
        boost::safe_numerics::checked::cast<T2>(v1);
        return false;
    }
    return true; // passed test
}

#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/preprocessor/array/elem.hpp>
#include <boost/preprocessor/array/size.hpp>
#include <boost/preprocessor/stringize.hpp>

#include "test_types.hpp"
#include "test_values.hpp"
#include "test.hpp"

// note: same test matrix as used in test_checked.  Here we test all combinations
// safe and unsafe integers.  in test_checked we test all combinations of
// integer primitives

const char *test_result[VALUE_ARRAY_SIZE] = {
//      0       0       0       0
//      01234567012345670123456701234567
//      01234567890123456789012345678901
/* 0*/ ".....xx..xx..xx...xx.xxx.xxx.xxx",
/* 1*/ "..xx.xxx.xxx.xxx.....xxx.xxx.xxx",
/* 2*/ ".........xx..xx.......xx.xxx.xxx",
/* 3*/ "..xx..xx.xxx.xxx.........xxx.xxx",
/* 4*/ ".............xx...........xx.xxx",
/* 5*/ "..xx..xx..xx.xxx.............xxx",
/* 6*/ "..............................xx",
/* 7*/ "..xx..xx..xx..xx................"
};

#define TEST_CAST(T1, v, result)      \
    rval = rval && test_cast<T1>(     \
        v,                            \
        BOOST_PP_STRINGIZE(T1),       \
        BOOST_PP_STRINGIZE(v),        \
        result                        \
    );
/**/

#define EACH_VALUE(z, value_index, type_index)     \
    (std::cout << type_index << ',' << value_index << ','); \
    TEST_CAST(                                     \
        BOOST_PP_ARRAY_ELEM(type_index, TYPES),    \
        BOOST_PP_ARRAY_ELEM(value_index, VALUES),  \
        test_result[type_index][value_index]       \
    )                                              \
/**/

#define EACH_TYPE1(z, type_index, nothing)         \
    BOOST_PP_REPEAT(                               \
        BOOST_PP_ARRAY_SIZE(VALUES),               \
        EACH_VALUE,                                \
        type_index                                 \
    )                                              \
/**/

int main(){
    bool rval = true;
    BOOST_PP_REPEAT(                               \
        BOOST_PP_ARRAY_SIZE(TYPES),                \
        EACH_TYPE1,                                \
        nothing                                    \
    )
    std::cout << (rval ? "success!" : "failure") << std::endl;
    return rval ? EXIT_SUCCESS : EXIT_FAILURE;
}
