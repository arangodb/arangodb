//  Copyright (c) 2012 Robert Ramey
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <exception>

#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/native.hpp>

template <class T>
using safe_t = boost::safe_numerics::safe<
    T,
    boost::safe_numerics::native
>;

#include "test_add.hpp"
#include "test.hpp"
#include "test_values.hpp"
#include "check_symmetry.hpp"

// note: same test matrix as used in test_checked.  Here we test all combinations
// safe and unsafe integers.  in test_checked we test all combinations of
// integer primitives

const char *test_addition_result[VALUE_ARRAY_SIZE] = {
//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/* 0*/ ".........x...x.............x...x.",
/* 1*/ ".........x...x.............x...x.",
/* 2*/ "..........x...x.........xxxxxxxx.",
/* 3*/ "..........x...x.........xxxxxxxx.",
/* 4*/ ".........x...x.............x...x.",
/* 5*/ ".........x...x.............x...x.",
/* 6*/ "..........x...x.........xxxxxxxx.",
/* 7*/ "..........x...x.........xxxxxxxx.",

/* 8*/ ".........x...x.............x...x.",
/* 9*/ "xx..xx..xx...x..xxxxxxxx...x...x.",
/*10*/ "..xx..xx..xx..x.........xxxxxxxx.",
/*11*/ "..........x...x.........xxxxxxxx.",
/*12*/ ".............x.................x.",
/*13*/ "xx..xx..xx..xx..xxxxxxxxxxxx...x.",
/*14*/ "..xx..xx..xx..xx............xxxx.",
/*15*/ "..............x.............xxxx.",

//      0       0       0       0
//      012345670123456701234567012345670
//      012345678901234567890123456789012
/*16*/ ".........x...x.............x...x.",
/*17*/ ".........x...x.............x...x.",
/*18*/ ".........x...x.............x...x.",
/*19*/ ".........x...x.............x...x.",
/*20*/ ".........x...x.............x...x.",
/*21*/ ".........x...x.............x...x.",
/*22*/ ".........x...x.............x...x.",
/*23*/ ".........x...x.............x...x.",

/*24*/ "..xx..xx..xx.x.............x...x.",
/*25*/ "..xx..xx..xx.x.............x...x.",
/*26*/ "..xx..xx..xx.x............xx...x.",
/*27*/ "xxxxxxxxxxxx.x..xxxxxxxxxxxx...x.",
/*28*/ "..xx..xx..xx..xx...............x.",
/*29*/ "..xx..xx..xx..xx...............x.",
/*30*/ "..xx..xx..xx..xx..............xx.",
/*31*/ "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.",
/*32*/ "................................."
};

#include <boost/preprocessor/stringize.hpp>

#define TEST_IMPL(v1, v2, result) \
    rval &= test_add(             \
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
        test_addition_result[value_index1][value_index2] \
    )
/**/
int main(){
    // sanity check on test matrix - should be symetrical
    check_symmetry(test_addition_result);
    bool rval = true;
    TEST_EACH_VALUE_PAIR
    std::cout << (rval ? "success!" : "failure") << std::endl;
    return ! rval ;
}
