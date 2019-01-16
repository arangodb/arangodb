//  Copyright (c) 2017 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// test construction assignments

#include <iostream>
#include <exception>

#include <boost/safe_numerics/safe_integer.hpp>

template <class T>
using safe_t = boost::safe_numerics::safe<
    T,
    boost::safe_numerics::native
>;

#include "test_assignment.hpp"
#include "test.hpp"
#include "test_values.hpp"

// note: same test matrix as used in test_checked.  Here we test all combinations
// safe and unsafe integers.  in test_checked we test all combinations of
// integer primitives

const char *test_assignment_result[VALUE_ARRAY_SIZE] = {
//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/* 0*/ ".....xx..xx..xx...xx.xxx.xxx.xxx.",
/* 1*/ ".....xx..xx..xx...xx.xxx.xxx.xxx.",
/* 2*/ ".....xx..xx..xx...xx.xxx.xxx.xxx.",
/* 3*/ ".....xx..xx..xx...xx.xxx.xxx.xxx.",
/* 4*/ ".........xx..xx.......xx.xxx.xxx.",
/* 5*/ ".........xx..xx.......xx.xxx.xxx.",
/* 6*/ ".........xx..xx.......xx.xxx.xxx.",
/* 7*/ ".........xx..xx.......xx.xxx.xxx.",

/* 8*/ ".............xx...........xx.xxx.",
/* 9*/ ".............xx...........xx.xxx.",
/*10*/ ".............xx...........xx.xxx.",
/*11*/ ".............xx...........xx.xxx.",
/*12*/ "..............................xx.",
/*13*/ "..............................xx.",
/*14*/ "..............................xx.",
/*15*/ "..............................xx.",

//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/*16*/ "..xx.xxx.xxx.xxx.....xxx.xxx.xxx.",
/*17*/ "..xx.xxx.xxx.xxx.....xxx.xxx.xxx.",
/*18*/ "..xx.xxx.xxx.xxx.....xxx.xxx.xxx.",
/*19*/ "..xx.xxx.xxx.xxx.....xxx.xxx.xxx.",
/*20*/ "..xx..xx.xxx.xxx.........xxx.xxx.",
/*21*/ "..xx..xx.xxx.xxx.........xxx.xxx.",
/*22*/ "..xx..xx.xxx.xxx.........xxx.xxx.",
/*23*/ "..xx..xx.xxx.xxx.........xxx.xxx.",

/*24*/ "..xx..xx..xx.xxx.............xxx.",
/*25*/ "..xx..xx..xx.xxx.............xxx.",
/*26*/ "..xx..xx..xx.xxx.............xxx.",
/*27*/ "..xx..xx..xx.xxx.............xxx.",
/*28*/ "..xx..xx..xx..xx.................",
/*29*/ "..xx..xx..xx..xx.................",
/*30*/ "..xx..xx..xx..xx.................",
/*31*/ "..xx..xx..xx..xx.................",
//      012345678901234567890123456789012
/*32*/ ".....xx..xx..xx...xx.xxx.xxx.xxx."
};

#include <boost/preprocessor/stringize.hpp>

#define TEST_IMPL(v1, v2, result)   \
    rval &= test_assignment(        \
        v1,                         \
        v2,                         \
        BOOST_PP_STRINGIZE(v1),     \
        BOOST_PP_STRINGIZE(v2),     \
        result                      \
    );
/**/

#define TESTX(value_index1, value_index2)          \
    (std::cout << value_index1 << ',' << value_index2 << ','); \
    TEST_IMPL(                                     \
        BOOST_PP_ARRAY_ELEM(value_index1, VALUES), \
        BOOST_PP_ARRAY_ELEM(value_index2, VALUES), \
        test_assignment_result[value_index1][value_index2] \
    )
/**/
int main(int, char *[]){
    bool rval = true;
    TEST_EACH_VALUE_PAIR
    std::cout << (rval ? "success!" : "failure") << std::endl;
    return ! rval ;
}
