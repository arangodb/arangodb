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

#include "test_multiply.hpp"
#include "test.hpp"
#include "test_values.hpp"
#include "check_symmetry.hpp"

// note: These tables presume that the the size of an int is 32 bits.
// This should be changed for a different architecture or better yet
// be dynamically adjusted depending on the indicated architecture

const char *test_multiplication_result[VALUE_ARRAY_SIZE] = {
//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/* 0*/ "..............................xx.",
/* 1*/ ".............xx..............xxx.",
/* 2*/ ".............xx..............xxx.",
/* 3*/ "..............x...............xx.",
/* 4*/ "..............................xx.",
/* 5*/ ".............xx..............xxx.",
/* 6*/ ".............xx..............xxx.",
/* 7*/ "..............x...............xx.",

/* 8*/ "..............................xx.",
/* 9*/ ".............xx..............xxx.",
/*10*/ ".............xx..............xxx.",
/*11*/ "..............x...............xx.",
/*12*/ "..............................xx.",
/*13*/ ".xx..xx..xx..xx..xxx.xxx.xxx.xxx.",
/*14*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*15*/ "..............x...............xx.",

//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/*16*/ ".................................",
/*17*/ ".............xx..............xxx.",
/*18*/ ".............xx..............xxx.",
/*19*/ ".............xx..............xxx.",
/*20*/ ".................................",
/*21*/ ".............xx..............xxx.",
/*22*/ ".............xx..............xxx.",
/*23*/ ".............xx..............xxx.",

/*24*/ ".................................",
/*25*/ ".............xx..............xxx.",
/*26*/ ".............xx..............xxx.",
/*27*/ ".............xx..............xxx.",
/*28*/ ".................................",
/*29*/ ".xx..xx..xx..xx..xxx.xxx.xxx.xxx.",
/*30*/ "xxxxxxxxxxxxxxxx.xxx.xxx.xxx.xxxx",
/*31*/ "xxxxxxxxxxxxxxxx.xxx.xxx.xxx.xxxx",
/*32*/ "..............................xx."
};

#define TEST_IMPL(v1, v2, result) \
    rval &= test_multiply(        \
        v1,                       \
        v2,                       \
        BOOST_PP_STRINGIZE(v1),   \
        BOOST_PP_STRINGIZE(v2),   \
        result                    \
    );
/**/

#define TESTX(value_index1, value_index2)          \
    (std::cout << value_index1 << ',' << value_index2 << ','); \
    TEST_IMPL(                                     \
        BOOST_PP_ARRAY_ELEM(value_index1, VALUES), \
        BOOST_PP_ARRAY_ELEM(value_index2, VALUES), \
        test_multiplication_result[value_index1][value_index2] \
    )
/**/

int main(int, char *[]){
    // sanity check on test matrix - should be symetrical
    check_symmetry(test_multiplication_result);
    bool rval = true;
    TEST_EACH_VALUE_PAIR
    std::cout << (rval ? "success!" : "failure") << std::endl;
    return ! rval ;
}
