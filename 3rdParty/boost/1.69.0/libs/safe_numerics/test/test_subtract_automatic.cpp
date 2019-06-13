//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <exception>

#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/automatic.hpp>

template <class T>
using safe_t = boost::safe_numerics::safe<
    T,
    boost::safe_numerics::automatic
>;

#include "test_subtract.hpp"
#include "test.hpp"
#include "test_values.hpp"

// note: same test matrix as used in test_checked.  Here we test all combinations
// safe and unsafe integers.  in test_checked we test all combinations of
// integer primitives

const char *test_subtraction_result[VALUE_ARRAY_SIZE] = {
//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/* 0*/ "..............x...............xx.",
/* 1*/ "..............x...............xx.",
/* 2*/ ".............x...............xxx.",
/* 3*/ "..............................xx.",
/* 4*/ "..............x...............xx.",
/* 5*/ "..............x...............xx.",
/* 6*/ ".............x...............xxx.",
/* 7*/ "..............................xx.",

/* 8*/ "..............x...............xx.",
/* 9*/ "..............x...............xx.",
/*10*/ ".............x...............xxx.",
/*11*/ "..............................xx.",
/*12*/ "..............x...............xx.",
/*13*/ "..xx..xx..xx..xx..............xx.",
/*14*/ "xx..xx..xx..xx..xxxxxxxxxxxxxxxx.",
/*15*/ "..............................xx.",

//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/*16*/ "..............x...............xx.",
/*17*/ "..............x...............xx.",
/*18*/ "..............x...............xx.",
/*19*/ "..............x...............xx.",
/*20*/ "..............x...............xx.",
/*21*/ "..............x...............xx.",
/*22*/ "..............x...............xx.",
/*23*/ "..............x...............xx.",

/*24*/ "..............x...............xx.",
/*25*/ "..............x...............xx.",
/*26*/ "..............x...............xx.",
/*27*/ "..............x...............xx.",
/*28*/ "..............x...............xx.",
/*29*/ "..xx..xx..xx..xx..............xx.",
/*30*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
/*31*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
/*32*/ "..............x...............xx."
};

#define TEST_IMPL(v1, v2, result) \
    rval &= test_subtract(   \
        v1,                       \
        v2,                       \
        BOOST_PP_STRINGIZE(v1),   \
        BOOST_PP_STRINGIZE(v2),   \
        result                    \
    );
/**/

#define TESTX(value_index1, value_index2)          \
    (std::cout << value_index1 << ',' << value_index2 << ' '); \
    TEST_IMPL(                                     \
        BOOST_PP_ARRAY_ELEM(value_index1, VALUES), \
        BOOST_PP_ARRAY_ELEM(value_index2, VALUES), \
        test_subtraction_result[value_index1][value_index2] \
    )
/**/

int main(int, char *[]){
    bool rval = true;
    TEST_EACH_VALUE_PAIR
    std::cout << (rval ? "success!" : "failure") << std::endl;
    return ! rval ;
}
