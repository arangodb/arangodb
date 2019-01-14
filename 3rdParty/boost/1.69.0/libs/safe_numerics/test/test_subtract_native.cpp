//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <exception>
#include <cassert>

#include <boost/safe_numerics/safe_integer.hpp>

template <class T>
using safe_t = boost::safe_numerics::safe<T>;

#include "test_subtract.hpp"
#include "test.hpp"
#include "test_values.hpp"

const char *test_subtraction_result[VALUE_ARRAY_SIZE] = {
//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/* 0*/ "..........x...x..........xxx.xxx.",
/* 1*/ "..........x...x..........xxx.xxx.",
/* 2*/ ".........x...x..........xxxxxxxx.",
/* 3*/ "........................xxxxxxxx.",
/* 4*/ "..........x...x..........xxx.xxx.",
/* 5*/ "..........x...x..........xxx.xxx.",
/* 6*/ ".........x...x..........xxxxxxxx.",
/* 7*/ "........................xxxxxxxx.",

/* 8*/ "..........x...x..........xxx.xxx.",
/* 9*/ "..xx..xx..xx..x...........xx.xxx.",
/*10*/ "xx..xx..xx...x..xxxxxxxxxxxxxxxx.",
/*11*/ "........................xxxxxxxx.",
/*12*/ "..............x..............xxx.",
/*13*/ "..xx..xx..xx..xx..............xx.",
/*14*/ "xx..xx..xx..xx..xxxxxxxxxxxxxxxx.",
/*15*/ "............................xxxx.",

//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/*16*/ "..........x...x..........xxx.xxx.",
/*17*/ "..........x...x..........xxx.xxx.",
/*18*/ "..........x...x..........xxx.xxx.",
/*19*/ "..........x...x..........xxx.xxx.",
/*20*/ "..........x...x..........xxx.xxx.",
/*21*/ "..........x...x..........xxx.xxx.",
/*22*/ "..........x...x..........xxx.xxx.",
/*23*/ "..........x...x..........xxx.xxx.",

/*24*/ ".xxx.xxx.xxx..x..xxx.xxx.xxx.xxx.",
/*25*/ "..xx..xx..xx..x...........xx.xxx.",
/*26*/ "..xx..xx..xx..x............x.xxx.",
/*27*/ "..xx..xx..xx..x..............xxx.",
/*28*/ ".xxx.xxx.xxx.xxx.xxx.xxx.xxx.xxx.",
/*29*/ "..xx..xx..xx..xx..............xx.",
/*30*/ "..xx..xx..xx..xx...............x.",
/*31*/ "..xx..xx..xx..xx.................",
/*32*/ "..........x...x.........xxxxxxxx."
};

#define TEST_IMPL(v1, v2, result) \
    rval &= test_subtract(        \
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
        test_subtraction_result[value_index1][value_index2] \
    )
/**/

int main(int , char * []){
    bool rval = true;
    TEST_EACH_VALUE_PAIR
    std::cout << (rval ? "success!" : "failure") << std::endl;
    return ! rval ;
}
