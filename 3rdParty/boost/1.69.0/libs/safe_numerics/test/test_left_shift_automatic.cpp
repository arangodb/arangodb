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

#include "test_left_shift.hpp"
#include "test.hpp"
#include "test_values.hpp"

// note: same test matrix as used in test_checked.  Here we test all combinations
// safe and unsafe integers.  in test_checked we test all combinations of
// integer primitives

const char *test_left_shift_result[VALUE_ARRAY_SIZE] = {
//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/* 0*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/* 1*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/* 2*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/* 3*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/* 4*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/* 5*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/* 6*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/* 7*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",

/* 8*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/* 9*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*10*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/*11*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/*12*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*13*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/*14*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/*15*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",

//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/*16*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*17*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*18*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*19*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*20*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*21*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*22*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*23*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",

/*24*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*25*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*26*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*27*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*28*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*29*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*30*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/*31*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/*32*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx."
};

#include <boost/preprocessor/stringize.hpp>

#define TEST_IMPL(v1, v2, result) \
    rval &= test_left_shift(      \
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
        test_left_shift_result[value_index1][value_index2] \
    )
/**/
int main(int, char *[]){
    bool rval = true;
    TEST_EACH_VALUE_PAIR
    std::cout << (rval ? "success!" : "failure") << std::endl;
    return ! rval ;
}
